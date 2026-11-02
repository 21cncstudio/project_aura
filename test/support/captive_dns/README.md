# Captive DNS host checks

Run `python -X utf8 tools/test_captive_dns.py --output-dir build/captive-dns-check` from the firmware root. The destination must not exist; MinGW is expected at `C:/msys64/mingw64/bin`.

The stand compiles the production CaptiveDnsServer and CaptiveDnsReply with deterministic socket, fcntl, bind, recvfrom, sendto and close shims. It checks AP-address binding, nonblocking setup, socket ownership, bounded polling, malformed/oversized datagrams, idle/transient/fatal receives, retryable start failures, and dropped sends. There are 65 assertions. No real UDP, lwIP, Wi-Fi or device is used.

Native `test_captive_dns_reply` covers packet parsing/encoding and bounds. NetworkManager A34-A36 cover ownership/admission/retry with DNS mocked. Hardware evidence and Android captive-detection limits are in `docs/HTTP_AP_ANDROID_MAC_20261102.md`.
