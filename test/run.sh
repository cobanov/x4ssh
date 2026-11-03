#!/bin/sh
# Host tests: builds against the freeink-sdk host stubs, no device needed.
set -e
cd "$(dirname "$0")/.."
B=freeink-sdk/libs/network/BleKeyboardHost
c++ -std=c++17 -I include -I $B/include -I $B/test/host/stubs test/keys_test.cpp -o /tmp/x4ssh_keys_test
/tmp/x4ssh_keys_test
c++ -std=c++17 -I lib/TermFont test/glyphs_test.cpp -o /tmp/x4ssh_glyphs_test
/tmp/x4ssh_glyphs_test
c++ -std=c++17 -I include test/status_test.cpp -o /tmp/x4ssh_status_test
/tmp/x4ssh_status_test
c++ -std=c++17 -g -fsanitize=address,undefined -fno-sanitize-recover=all -I include -I lib/TermBuffer -I lib/VtParser test/vt_test.cpp \
  lib/VtParser/VtParser.cpp lib/TermBuffer/TermBuffer.cpp -o /tmp/x4ssh_vt_test
/tmp/x4ssh_vt_test
c++ -std=c++20 -I test/stubs -I include -I lib/TermBuffer -I lib/TermRenderer -I lib/TermFont test/art_test.cpp \
  lib/TermRenderer/TermRenderer.cpp lib/TermBuffer/TermBuffer.cpp -o /tmp/x4ssh_art_test
/tmp/x4ssh_art_test "$@"
