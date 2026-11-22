# 7B outage fetch and display diagnosis (2026-11-22)

| Hypothesis | Expected observation | Experiment and result | Conclusion |
|---|---|---|---|
| The JSON body is too large | No PSRAM buffer or truncated response | The board allocated a 128 KB PSRAM buffer and later read all 62,222 bytes | Rejected |
| The outage task cannot start in internal RAM | No fetch attempts | `/metrics` reported stage `-2`; the 8 KB task stack could not be created | Confirmed; moved the stack to PSRAM |
| HTTPS needs more internal RAM | Request fails while opening TLS | The task started but HTTP open returned `0x7002`; TLS buffers were moved to PSRAM and HTTPS returned 200 | Confirmed |
| The dial renderer causes the reboot | Panic after schedule publication | Serial showed successful publication; the diagnostic `/metrics` call then passed a null update pointer and caused `StoreProhibited` | Rejected; fixed the diagnostic call |
| Constructing unused pages exhausts internal RAM | Very low heap watermark | With ten pages, the minimum was 251 bytes after capture; with Overview only it was 22,695 bytes | Confirmed; retain single-screen layout |

The final device reported one successful fetch, two confirmed intervals for
today, 35.6 KB free internal heap, and 2.69 MB free PSRAM after capture.
The USB serial port later disappeared and uptime reset because the board was
unplugged and moved by the owner. The device fetched the schedule successfully
again after being powered in its new location.
