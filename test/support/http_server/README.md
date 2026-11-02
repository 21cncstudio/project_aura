# AP/HTTP SDK regressions

This host suite compiles the selected ESP-IDF HTTP server and session loop,
production Aura transport/handlers/scan worker/guard, and Arduino String.
Windows sockets, FreeRTOS and Wi-Fi driver calls use host shims. It does not
execute NetworkManager, NetworkPlane, the real command queue, ESP32 lwIP or radio.

Run from the firmware checkout (Windows, MinGW):

```powershell
python -X utf8 tools/test_http_server_sdk.py --idf-path D:/21cncstudio/project_aura_aq/tools/idf61-sdk/esp-idf
```

All 34 groups run by default. Use repeated `--case` arguments to select groups.
Each group runs in its own process with a 120-second timeout; build commands,
logs, source hashes and assertion counts are saved under `build/`.

The ownership regressions added after the third independent review are ports of
A26–A28 and an assertion-bearing Z10:

| Group | Property checked |
| --- | --- |
| S29 | A scheduled worker without a lease cannot call SDK stop during Switching. |
| S30 | A worker retains its lease until driver result cleanup returns. |
| S31 | Stop holds the worker mutex until the SDK call returns, preventing concurrent cleanup and lease release. |
| S32 | An interrupted HTTP drain reacquires its lease before a second wake; protected phases perform no session I/O; the complete rejection survives both pauses. |

S29 gates task entry; S30/S31 hold SDK calls at explicit barriers. Their ownership
checks do not infer safety from a particular driver delay. Bounded host waits
let concurrent threads progress; they are not ESP32 performance guarantees.
S32 waits for half-close before requesting wake and explicitly checks the live
lease between cycles. Checking only delivery of the already-sent JSON would
miss a removed reacquisition.

Run the four groups and their deliberate negative controls:

```powershell
python -X utf8 tools/test_http_guard_mutations.py --idf-path D:/21cncstudio/project_aura_aq/tools/idf61-sdk/esp-idf
```

The mutation runner copies inputs into a fresh ignored directory, runs the
unchanged control, then removes one protection per copy. It requires the named
assertion to fail with the expected test exit code. Compilation errors,
timeouts, missing results and unrelated failures are failures of the mutation
check, not successful detections. `--output-dir` must name a new directory;
previous evidence is never removed. The production tree is verified unchanged.

The manager integration scenarios are now maintained separately in
[the NetworkManager host suite](../network_manager/README.md), including
save-transition cases A30–A33. That suite executes the real manager with driver
and HTTP/GUI shims; neither suite establishes full device end-to-end coverage.
The original independent-review evidence remains under
`build/http-ap-recheck3-20261101/`.

S33 checks drain completion diagnostics for a peer FIN and an incomplete sender
that hits the progress timeout. S20 and S22 also check the diagnostic reason for
the total-time and byte limits.

These tests do not remove the 90-second / 8-MiB drain bounds or the 32-KiB per
5000-ms progress requirement. Slow uploads and sender pauses can still lose a
rejection; browser retries can occupy the server for more than one budget.
Hardware qualification of the exact candidate remains separate.

S34 sends four 64-KiB bursts with repeated 2.2-second pauses, reading the response only after the complete request. It protects delivery across TCP recovery pauses; the old 1500-ms progress window fails this scenario. S17 still bounds stalled/trickling peers, and S23 pauses wake for longer than the new window. These are host timings, not a guarantee for arbitrary radio loss.
