# Stamps the git version and build time into the ELF's esp_app_desc_t. Those come from the tuned core, which is
# cached, so without this every build would carry the version the core happened to be compiled at.
import struct
import subprocess
import time

Import("env")


def stamp(source, target, env):
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--dirty", "--always"], text=True, cwd=env["PROJECT_DIR"]
    ).strip()
    with open(target[0].get_abspath(), "r+b") as f:
        elf = f.read()
        at = elf.find(struct.pack("<I", 0xABCD5432))  # the descriptor's magic word
        assert at >= 0, "no esp_app_desc_t in the ELF"
        f.seek(at + 16)
        f.write(version.encode()[:31].ljust(32, b"\0"))
        f.seek(at + 80)
        f.write(time.strftime("%H:%M:%S").encode().ljust(16, b"\0") + time.strftime("%b %e %Y").encode().ljust(16, b"\0"))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", stamp)
