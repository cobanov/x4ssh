// x4ssh: SSH terminal for the Xteink X4, held sideways.
// Boots into a tiny local prompt (x4>), connects out with libssh, draws with X4Term's VT100 engine.
// Keys come from a BLE keyboard, the X4's buttons, or USB serial (so a Mac can drive it too).
#include <Arduino.h>
#include <BleKeyboardHost.h>
#include <EInkDisplay.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <libssh/libssh.h>
#include <time.h>

#include "HalGPIO.h"
#include "TermBuffer.h"
#include "TermRenderer.h"
#include "VtParser.h"
#include "keys.h"
#include "libssh_esp32.h"
#include "mascot.h"
#include "status.h"
#include "term_config.h"

// LibSSH-ESP32's examples give libssh 51200 B, but boot with key generation peaked under 3 KB here
// and the heap is the scarce thing; the session line on USB reports the real peak.
SET_LOOP_TASK_STACK_SIZE(20480);

static const char* kKeyPath = "/littlefs/id_ed25519";
static const char* kKnownHosts = "/littlefs/known_hosts";
static const char* kDefaultTz = "UTC0";

static EInkDisplay display(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
static HalGPIO gpio;
static TermBuffer termBuf;
static VtParser parser(termBuf);
static TermRenderer renderer(display, termBuf);
static Preferences prefs;
static ssh_key userKey = nullptr;
static freeink::BleKeyboardHost& kb = freeink::BleKeyboardHost::getInstance();
static ssh_channel liveChannel = nullptr;  // where the terminal's answers to host queries go
static unsigned long lastRefreshMs = 0;
static bool wifiWanted = false;  // a network is saved, so the splash snail goes looking for it

// ---- output: everything drawn on the e-ink is mirrored to USB so a Mac can watch and drive it

static void put(const char* s, size_t n) {
  for (size_t i = 0; i < n; i++) parser.feed((uint8_t)s[i]);
  Serial.write((const uint8_t*)s, n);
}

// Local text: a newline also returns the carriage, as a remote pty would.
static void text(const char* s) {
  for (; *s; s++) {
    if (*s == '\n') put("\r", 1);
    put(s, 1);
  }
}

static void say(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  text(buf);
}

// BLE names come from whatever is nearby; a control byte in one would reach the terminal.
static const char* printable(const char* s) {
  static char out[64];
  size_t i = 0;
  for (; s[i] && i < sizeof out - 1; i++) out[i] = (uint8_t)s[i] < 0x20 || s[i] == 0x7f ? '?' : s[i];
  out[i] = 0;
  return out;
}

// Rebuilt only when the minute, the wifi link or the USB cable changes (one refresh a minute);
// battery and signal are sampled then, so their jitter costs no refreshes. The cable is not shown,
// but plugging it in moves the battery reading, so it still triggers a rebuild.
static void updateStatus() {
  static unsigned long lastCheck = 0;
  static long lastKey = -1;
  if (lastKey != -1 && millis() - lastCheck < 1000) return;
  lastCheck = millis();
  struct tm t = {};
  const bool synced = getLocalTime(&t, 0);
  const bool up = WiFi.status() == WL_CONNECTED, usb = gpio.isUsbConnected();
  const long key = (synced ? t.tm_hour * 60 + t.tm_min : 10000 + (long)(millis() / 60000)) * 4 + up * 2 + usb;
  if (key == lastKey) return;
  lastKey = key;
  uint16_t cells[TERM_COLS];
  buildStatus(cells, TERM_COLS, up ? WiFi.SSID().c_str() : nullptr, WiFi.RSSI(), gpio.getBatteryPercentage(),
              synced ? t.tm_hour : -1, t.tm_min);
  renderer.setStatus(cells);
}

// On the splash the snail sweeps its dish until wifi answers, then puts it down. After 40 s it holds
// still: a network that is not there should not cost a refresh a second for as long as the splash stays.
static void updateArt() {
  static const uint8_t* const kSweep[] = {kMascotScan1, kMascotScan2, kMascotScan3};
  static unsigned long last = 0;
  static int frame = 0;
  if (!renderer.hasArt() || !wifiWanted) return;
  if (WiFi.status() == WL_CONNECTED) {
    renderer.setArt(kMascotAwake);
  } else if (millis() < 40000 && millis() - last >= 1000) {
    last = millis();
    renderer.setArt(kSweep[frame++ % 3]);
  }
}

static void pumpDisplay(bool now = false) {
  updateStatus();
  updateArt();
  if (termBuf.dirtyRows() == 0 && !renderer.statusPending()) return;
  if (!now && millis() - lastRefreshMs < MIN_REFRESH_INTERVAL_MS) return;
  renderer.setCursorVisible(parser.cursorVisible());
  renderer.renderDirty();
  lastRefreshMs = millis();
}

// The two lines under the mascot, then the cursor below them.
static void splash(const char* line) {
  static const char kUrl[] = "x.com/mertcobanov";
  say("\033[2J\033[%d;1H%*s%s\n%*s%s\n\n", MASCOT_LAST_ROW + 2, (TERM_COLS - (int)strlen(line)) / 2, "", line,
      (TERM_COLS - (int)strlen(kUrl)) / 2, "", kUrl);
}

// ---- input

// E-ink keeps its last image with the power off, so the sleep screen stays up while it sleeps:
// the sleeping snail, how to wake it, and when it went to sleep with the battery then.
static void sleepNow() {
  struct tm t = {};
  char since[32] = "sleeping", bat[8];
  if (getLocalTime(&t, 0)) snprintf(since, sizeof since, "sleeping since %02d:%02d", t.tm_hour, t.tm_min);
  snprintf(bat, sizeof bat, "%d%%", gpio.getBatteryPercentage());
  uint16_t cells[TERM_COLS];
  textStatus(cells, TERM_COLS, since, bat);
  renderer.setStatus(cells);
  text("\033[?25l");
  splash("press power to wake");
  renderer.setArt(kMascotAsleep);
  renderer.setCursorVisible(false);
  renderer.renderFull();
  display.deepSleep();
  // ESP-IDF wants the radios down before deep sleep; left up they can wake the chip right back up.
  if (kb.isRunning()) kb.end();
  WiFi.mode(WIFI_OFF);
  gpio.startDeepSleep();
}

// Fills `out` with key bytes from every input source; returns how many.
static size_t pollInput(uint8_t* out, size_t cap) {
  using K = freeink::SpecialKey;
  static const struct {
    uint8_t btn;
    K key;
  } kButtons[] = {
      {HalGPIO::BTN_UP, K::Up},     {HalGPIO::BTN_DOWN, K::Down},       {HalGPIO::BTN_RIGHT, K::Right},
      {HalGPIO::BTN_LEFT, K::Left}, {HalGPIO::BTN_CONFIRM, K::Enter}, {HalGPIO::BTN_BACK, K::Escape},
  };
  size_t n = 0;
  bool button = false;
  freeink::KeyEvent e;
  gpio.update();
  for (auto& b : kButtons) {
    if (!gpio.wasPressed(b.btn) || n + 5 > cap) continue;
    button = true;
    e.special = b.key;
    n += keyBytes(e, out + n, parser.appCursor());
  }
  if (kb.isRunning()) {
    kb.poll();
    // The keyboard slept past the reconnect window: a press on the X4 starts looking again.
    if (button && !kb.isConnected() && !kb.isConnecting()) kb.armBondedReconnect(freeink::PickPolicy::First, nullptr);
    while (n + 5 <= cap && kb.popKey(e)) n += keyBytes(e, out + n, parser.appCursor());
  }
  if ((gpio.wasPressed(HalGPIO::BTN_CONFIRM) && gpio.isPressed(HalGPIO::BTN_BACK)) ||
      (gpio.wasPressed(HalGPIO::BTN_BACK) && gpio.isPressed(HalGPIO::BTN_CONFIRM))) {
    renderer.renderFull();
  }
  // Power: press = redraw the whole screen, hold = sleep. Boot ignores the press that woke it.
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > 1500) sleepNow();
  if (gpio.wasReleased(HalGPIO::BTN_POWER) && millis() > 5000) renderer.renderFull();
  while (n < cap && Serial.available()) out[n++] = Serial.read();
  return n;
}

// Reads one line, echoing it unless `secret`. Arrow keys and other escape sequences are dropped.
// Ctrl-C gives an empty line and sets askCancelled, which is how a caller tells it from a bare Enter.
static bool askCancelled = false;

static String ask(const char* prompt, bool secret = false) {
  static bool afterCR = false;
  askCancelled = false;
  int esc = 0;  // 1: after ESC, 2: inside a CSI/SS3 sequence
  String line;
  say("%s", prompt);
  for (;;) {
    uint8_t in[64];
    size_t n = pollInput(in, sizeof in);
    for (size_t i = 0; i < n; i++) {
      uint8_t c = in[i];
      bool skipLF = afterCR && c == '\n';
      afterCR = c == '\r';
      if (esc == 1) {
        esc = (c == '[' || c == 'O') ? 2 : 0;
      } else if (esc == 2) {
        if (c >= 0x40 && c <= 0x7e) esc = 0;
      } else if (c == 0x1b) {
        esc = 1;
      } else if (skipLF) {
      } else if (c == '\r' || c == '\n') {
        say("\n");
        return line;
      } else if (c == 0x03) {
        say("^C\n");
        askCancelled = true;
        return String();
      } else if (c == 0x7f || c == 0x08) {
        if (line.length()) {
          line.remove(line.length() - 1);
          if (!secret) say("\b \b");
        }
      } else if (c >= 0x20 && c < 0x7f && line.length() < 200) {
        line += (char)c;
        if (!secret) put((const char*)&c, 1);
      }
    }
    pumpDisplay();
    if (!n) delay(5);
  }
}

// ---- wifi

static bool waitWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!prefs.isKey("ssid")) {
    say("no wifi yet, run: wifi\n");
    return false;
  }
  say("waiting for wifi");
  for (int i = 0; i < 150 && WiFi.status() != WL_CONNECTED; i++) {
    if (i % 10 == 0) say(".");
    pumpDisplay();
    delay(100);
  }
  say("\n");
  if (WiFi.status() == WL_CONNECTED) return true;
  say("wifi did not connect (%s)\n", prefs.getString("ssid").c_str());
  return false;
}

static void cmdWifi() {
  String ssid = ask("ssid: ");
  if (!ssid.length()) return;
  String pass = ask("password: ", true);
  if (askCancelled) return;
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  wifiWanted = true;
  say("saved, joining %s\n", ssid.c_str());
}

// ---- keyboard

static bool startBle() {
  if (kb.isRunning() || kb.begin("x4ssh")) return true;
  say("bluetooth did not start (heap %u)\n", ESP.getFreeHeap());
  return false;
}

static void cmdPair() {
  if (!startBle()) return;
  say("put the keyboard in pairing mode, scanning...\n");
  pumpDisplay(true);
  kb.startScan(6000);
  while (kb.isScanning()) {
    kb.poll();
    delay(50);
  }
  int first = -1;
  for (int i = 0; i < kb.deviceCount(); i++) {
    const auto& d = kb.device(i);
    if (d.hid && first < 0) first = i;
    say("%2d) %s%s\n", i + 1, printable(d.name), d.hid ? "  [keyboard]" : "");
  }
  if (!kb.deviceCount()) {
    say("nothing found\n");
    return;
  }
  String pick = ask("number (Enter: first keyboard): ");
  int i = pick.length() ? pick.toInt() - 1 : first;
  if (i < 0 || i >= kb.deviceCount()) return;
  char addr[sizeof kb.device(i).addr];
  strcpy(addr, kb.device(i).addr);
  say("connecting to %s\n", printable(kb.device(i).name));
  pumpDisplay(true);
  if (!kb.connect(addr)) {
    say("could not start the connection\n");
    return;
  }
  for (unsigned long t0 = millis(); !kb.isConnected(); delay(50)) {
    kb.poll();
    uint32_t passkey;
    char why[64] = "(timed out)";
    if (kb.takePairingPasskey(passkey)) say("type %06u on the keyboard, then Enter\n", (unsigned)passkey);
    if (kb.takeConnectFailure(why, sizeof why) || millis() - t0 > 30000) {
      say("pairing failed %s\n", why);
      return;
    }
    pumpDisplay();
  }
  kb.releaseScanResults();
  say("keyboard connected: %s\n", printable(kb.connectedName()));
}

// ---- keys

static void loadKey() {
  if (ssh_pki_import_privkey_file(kKeyPath, nullptr, nullptr, nullptr, &userKey) == SSH_OK) return;
  say("first boot: generating device key...\n");
  pumpDisplay(true);
  if (ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &userKey) != SSH_OK ||
      ssh_pki_export_privkey_file(userKey, nullptr, nullptr, nullptr, kKeyPath) != SSH_OK) {
    say("key generation failed\n");
  }
}

static void cmdKey() {
  char* b64 = nullptr;
  if (!userKey || ssh_pki_export_pubkey_base64(userKey, &b64) != SSH_OK) {
    say("no key\n");
    return;
  }
  say("add this to ~/.ssh/authorized_keys:\nssh-ed25519 %s x4ssh\n", b64);
  ssh_string_free_char(b64);
}

// Drops the known_hosts lines for `host` (written by libssh as "host ..." or "[host]:port ...").
static void cmdForget(const String& host) {
  if (!host.length()) {
    say("usage: forget <host>\n");
    return;
  }
  File in = LittleFS.open("/known_hosts", "r");
  if (!in) {
    say("no known hosts\n");
    return;
  }
  String keep;
  int dropped = 0;
  while (in.available()) {
    String l = in.readStringUntil('\n');
    String first = l.substring(0, l.indexOf(' '));
    if (first == host || first.startsWith("[" + host + "]:")) {
      dropped++;
    } else if (l.length()) {
      keep += l + "\n";
    }
  }
  in.close();
  File out = LittleFS.open("/known_hosts", "w");
  out.print(keep);
  out.close();
  say("forgot %d key(s) for %s\n", dropped, host.c_str());
}

// ---- ssh

// Trust on first use; a changed key always refuses.
static bool verifyHost(ssh_session s) {
  ssh_key srv;
  unsigned char* hash = nullptr;
  size_t hlen = 0;
  if (ssh_get_server_publickey(s, &srv) < 0) return false;
  int rc = ssh_get_publickey_hash(srv, SSH_PUBLICKEY_HASH_SHA256, &hash, &hlen);
  ssh_key_free(srv);
  if (rc < 0) return false;
  char* fp = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
  ssh_clean_pubkey_hash(&hash);

  bool ok = false;
  switch (ssh_session_is_known_server(s)) {
    case SSH_KNOWN_HOSTS_OK:
      ok = true;
      break;
    case SSH_KNOWN_HOSTS_CHANGED:
    case SSH_KNOWN_HOSTS_OTHER:
      say("\n!! HOST KEY CHANGED, now %s\nrefusing. if you expected this: forget <host>\n", fp);
      break;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
      say("new host, key %s\n", fp);
      if (ask("trust it? (yes/no) ") != "yes") break;
      ok = ssh_session_update_known_hosts(s) == SSH_OK;
      if (!ok) say("could not save known_hosts\n");
      break;
    default:
      say("known_hosts: %s\n", ssh_get_error(s));
  }
  ssh_string_free_char(fp);
  return ok;
}

static bool auth(ssh_session s) {
  if (userKey && ssh_userauth_publickey(s, nullptr, userKey) == SSH_AUTH_SUCCESS) return true;
  for (int i = 0; i < 3; i++) {
    String pw = ask("password: ", true);
    if (!pw.length()) return false;
    if (ssh_userauth_password(s, nullptr, pw.c_str()) == SSH_AUTH_SUCCESS) return true;
    say("denied\n");
  }
  return false;
}

static void shell(ssh_session s) {
  ssh_channel ch = ssh_channel_new(s);
  if (!ch || ssh_channel_open_session(ch) != SSH_OK ||
      ssh_channel_request_pty_size(ch, "xterm-256color", TERM_COLS, TERM_ROWS) != SSH_OK ||
      ssh_channel_request_shell(ch) != SSH_OK) {
    say("shell failed: %s\n", ssh_get_error(s));
    if (ch) ssh_channel_free(ch);
    return;
  }
  char buf[512];
  uint8_t in[64];
  liveChannel = ch;
  renderer.setArt(nullptr);
  while (ssh_channel_is_open(ch) && !ssh_channel_is_eof(ch)) {
    // Drain what has arrived before drawing, so bulk output is not paced by e-ink refreshes.
    int got = 0, n = 0;
    while (got < 16384 && (n = ssh_channel_read_nonblocking(ch, buf, sizeof buf, 0)) > 0) {
      put(buf, n);
      got += n;
    }
    if (n < 0) break;
    size_t k = pollInput(in, sizeof in);
    if (k && ssh_channel_write(ch, in, k) == SSH_ERROR) break;
    pumpDisplay();
    if (!got && !k) delay(5);
  }
  liveChannel = nullptr;
  ssh_channel_close(ch);
  ssh_channel_free(ch);
}

static void cmdSsh(String target) {
  if (!target.length()) target = prefs.getString("last", "");
  int at = target.indexOf('@');
  if (at <= 0) {
    say("usage: ssh user@host[:port]\n");
    return;
  }
  String user = target.substring(0, at), host = target.substring(at + 1);
  int port = 22;
  int colon = host.lastIndexOf(':');
  if (colon > 0) {
    port = host.substring(colon + 1).toInt();
    host = host.substring(0, colon);
  }
  if (!waitWifi()) return;

  ssh_session s = ssh_new();
  if (!s) {
    say("not enough memory (heap %u)\n", ESP.getFreeHeap());
    return;
  }
  long timeout = 15;
  bool no = false;
  ssh_options_set(s, SSH_OPTIONS_HOST, host.c_str());
  ssh_options_set(s, SSH_OPTIONS_PORT, &port);
  ssh_options_set(s, SSH_OPTIONS_USER, user.c_str());
  ssh_options_set(s, SSH_OPTIONS_KNOWNHOSTS, kKnownHosts);
  ssh_options_set(s, SSH_OPTIONS_PROCESS_CONFIG, &no);
  ssh_options_set(s, SSH_OPTIONS_TIMEOUT, &timeout);
  say("connecting to %s:%d\n", host.c_str(), port);
  pumpDisplay(true);

  if (ssh_connect(s) != SSH_OK) {
    say("connect failed: %s\n", ssh_get_error(s));
  } else if (verifyHost(s) && auth(s)) {
    prefs.putString("last", target);
    // USB only, not drawn: heap headroom with a live session, to size the BLE stack against
    Serial.printf("[x4ssh heap %u free, %u largest, stack %u left]\r\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
                  (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    shell(s);
    // A dropped session leaves whatever modes the remote program had on; the scroll region's reset homes the cursor
    say("\033[0m\033(B\033[?1049l\0337\033[r\0338\033[?1l\033[?25h\nconnection to %s closed\n", host.c_str());
  }
  ssh_disconnect(s);
  ssh_free(s);
}

// ---- local prompt

static void cmdStat() {
  say("keyboard: %s\n", kb.isConnected() ? printable(kb.connectedName()) : kb.pairedCount() ? "paired, not connected" : "none");
  say("wifi: %s %s\n", prefs.getString("ssid", "-").c_str(),
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "(not connected)");
  say("heap: %u free, %u largest   battery: %d%%\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
      gpio.getBatteryPercentage());
}

// A POSIX TZ string, as the C library reads it: tz UTC-3, tz CET-1CEST,M3.5.0,M10.5.0/3
static void cmdTz(const String& tz) {
  if (tz.length()) {
    prefs.putString("tz", tz);
    setenv("TZ", tz.c_str(), 1);
    tzset();
  }
  say("tz: %s\n", prefs.getString("tz", kDefaultTz).c_str());
}

// CrossPoint's SD update writes the other OTA slot, so the firmware it came from is still there.
static void cmdCrosspoint() {
  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  if (!other || esp_ota_set_boot_partition(other) != ESP_OK) {
    say("no other firmware to boot\n");
    return;
  }
  say("restarting\n");
  pumpDisplay(true);
  ESP.restart();
}

static void cmdHelp() {
  // text, not say: this is longer than say's buffer
  text("ssh user@host[:port]   connect (no argument: the last one)\n"
       "wifi                   set the wifi network\n"
       "pair                   pair a BLE keyboard\n"
       "key                    show this device's public key\n"
       "forget <host>          drop a saved host key\n"
       "stat                   wifi, memory, battery\n"
       "tz [zone]              show or set the clock's zone, e.g. tz UTC-3\n"
       "crosspoint             go back to the firmware x4ssh was installed from\n"
       "sleep                  turn off (the power button wakes it)\n"
       "clear                  clear the screen\n");
}

void setup() {
  Serial.setRxBufferSize(4096);  // keys keep arriving while the e-ink refreshes
  Serial.setTxTimeoutMs(0);      // the USB mirror must never stall on battery with no host reading
  Serial.begin(TERM_BAUD);
  gpio.begin();
  display.begin();
  display.clearScreen(0xFF);
  display.displayBuffer(EInkDisplay::FULL_REFRESH, false);
  renderer.renderFull();
  parser.onReply([](const char* r) {
    if (liveChannel) ssh_channel_write(liveChannel, r, strlen(r));
  });

  splash("XTEINK SSH");
  renderer.setArt(kMascotAwake);
  pumpDisplay(true);
  // Bluetooth first: NimBLE wants ~50 KB in large blocks, which only an unfragmented heap still has.
  if (startBle() && kb.pairedCount()) kb.armBondedReconnect(freeink::PickPolicy::First, nullptr);
  prefs.begin("x4ssh");
  WiFi.mode(WIFI_STA);  // also turns the RF on, which the hardware RNG behind key generation needs
  wifiWanted = prefs.isKey("ssid");
  if (wifiWanted) WiFi.begin(prefs.getString("ssid").c_str(), prefs.getString("pass").c_str());
  configTzTime(prefs.getString("tz", kDefaultTz).c_str(), "pool.ntp.org", "time.google.com");  // SNTP retries until wifi is up
  if (!LittleFS.begin(true)) say("flash filesystem failed\n");
  libssh_begin();
  loadKey();
  say(kb.pairedCount() ? "type help\n" : "no keyboard yet: press Confirm to pair one\n");
}

void loop() {
  String l = ask("x4> ");
  l.trim();
  int sp = l.indexOf(' ');
  String cmd = sp < 0 ? l : l.substring(0, sp);
  String arg = sp < 0 ? "" : l.substring(sp + 1);
  arg.trim();
  if (cmd == "" && !kb.pairedCount()) cmdPair();  // first keyboard, with no keyboard to type "pair"
  else if (cmd == "") return;
  else if (cmd == "ssh") cmdSsh(arg);
  else if (cmd == "wifi") cmdWifi();
  else if (cmd == "pair") cmdPair();
  else if (cmd == "key") cmdKey();
  else if (cmd == "forget") cmdForget(arg);
  else if (cmd == "stat") cmdStat();
  else if (cmd == "tz") cmdTz(arg);
  else if (cmd == "crosspoint") cmdCrosspoint();
  else if (cmd == "clear") {
    renderer.setArt(nullptr);
    say("\033[2J\033[H");
  }
  else if (cmd == "sleep") sleepNow();
  else if (cmd == "help") cmdHelp();
  else say("unknown: %s (try help)\n", cmd.c_str());
}
