# NetworkManager host regressions

Promoted from the independent AP/HTTP review stand, with save-transition regressions A30–A33.
Run from the firmware root:

```powershell
python -X utf8 tools/test_network_manager.py . build/network-manager-tests
```

Optional fourth argument: `a30` (or any `a1`-`a55`) runs one case. MinGW is expected at `C:/msys64/mingw64/bin`.

Real code: NetworkManager, WifiScanSession, WifiScanTransition, WakePowerGuard, NetworkHostname and WebDeferredActionsState. The stand uses the WebHandlerContext callbacks registered by the real manager. WebHandlersPollDeferred has a small test adapter matching the production dispatch; the HTTP handler and GUI persistence are not executed here. The SDK HTTP stand and hardware checks cover separate layers.

Mocked: RTOS, Wi-Fi driver, storage, DNS and HTTP server. This is not a radio or lwIP test. No device/COM/network connection is opened. Output and logs go only into the supplied build directory.

A47 checks the cold-boot warmup followed by ordinary driver SSID selection, without a synchronous pre-scan or BSSID/channel pinning. A48 drives the registered production callback with AUTH_FAIL/WL_DISCONNECTED and checks the owner's retry delay, bounded budget and background backoff without erasing credentials. A49 reproduces a stale WL_CONNECT_FAILED across a fresh attempt; A50 exercises synchronous connect rejection and a callback before connect returns. The real Arduino callback's reconnect ownership is covered separately by `tools/tests/test_idf_arduino_wifi_overlay.py`.

A51-A53 check informational logging for initial authentication retries only: a 30-second deadline, immediate end after first success, warnings on exhausted retries, and no grace for an explicit reconnect. Retry timing is unchanged.

A51 follows a successful connection through link-loss detection and automatic reconnect inside the original grace window. A54 injects late failures between attempts and during disconnect, including a callback whose state check raced with the owner. A55 distinguishes AUTH_FAIL/ASSOC_FAIL from unrelated disconnect reasons and verifies that exhausted association retries warn. The full-run timeout includes the real 15-second link-loss debounce in A51.

A30 holds driver-result cleanup for 350 ms and measures AP shutdown against actual cleanup completion. A31 holds cancellation in failure beyond the grace period. A32 starts a second driver scan during grace with a deterministic thread barrier. A33 checks the no-scan path. These must preserve 200 ms after completion, not merely after the cancellation request.

A34-A36 cover AP-only DNS lifecycle, admission during wake, and retry after socket failure. A37-A42 cover per-country AP scan channels, at least 120 ms of home-channel airtime, aggregation/deduplication, cancellation during the home interval, SDK and cleanup errors, invalid country metadata, and the unchanged STA-only scan. Driver calls remain mocked. The separate `tools/test_captive_dns.py` executes production UDP ownership with deterministic I/O faults; native `test_captive_dns_reply` covers packet bounds and response encoding.
