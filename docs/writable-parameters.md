# Writable parameters

| Key | Label | Range / step | Class | Cooldown | Read-back source | Verified |
|---|---|---|---|---|---|---|
| `tou_enabled` | Time of Use | 0/1 | persistent | 60 s | `deye_tou_enabled` | **no** |
| `tou_slot1_soc` | ToU slot 1 target SOC | 10-100 %, 1 | persistent | 60 s | `deye_tou_slot_capacity_percent{slot="1"}` | **no** |
| `tou_slot1_power` | ToU slot 1 power | 0-6000 W, 100 | persistent | 60 s | `deye_tou_slot_power_watts{slot="1"}` | **no** |
| `tou_slot1_grid_charge` | ToU slot 1 grid charging | 0/1 | safety-critical | 300 s | `deye_tou_charge{slot="1",source="grid"}` | **no** |

These are candidates because their current value is visible in telemetry
(read-back is possible). Ranges are conservative guesses, not inverter
limits. No register addresses are assumed.

## Enablement procedure (per parameter)

1. Obtain the write API contract from the backend owner (endpoint,
   authentication, request/response, error and idempotency semantics).
2. Identify the Modbus register and confirm semantics and limits against the
   Deye register document for this firmware.
3. Determine persistence (RAM vs EEPROM/flash). If unknown, keep cooldown >= 60 s.
4. Perform one manual write under supervision, with the user present, and
   verify read-back plus inverter behaviour.
5. Implement the transport in `scr_controls.c::confirm_cb` (submit, then
   `ctrl_on_accepted/rejected/transport_error`), set
   `backend_write_api = true` and the parameter's `param_supported`, flip
   `verified = true` in `k_params`, add a test.
6. Record the evidence in hardware-validation.md.

None of these steps has been performed.
