# Control safety

## Current status

**All inverter writes are disabled.** Reasons:

1. The backend exposes no write API (api-contract.md).
2. No parameter's register semantics or persistence behaviour has been
   verified on this inverter firmware.

The Controls screen shows every candidate parameter read-only with the reason
(`Not verified for writing`).

## Classification

| Class | Meaning | Examples |
|---|---|---|
| Read-only telemetry | measurements | power, SOC, voltages |
| Persistent setting | stored by the inverter; **assumed non-volatile memory** | ToU enable, slot SOC, slot power |
| Temporary control | RAM-only effect | none identified |
| Safety-critical | affects charging source / grid interaction | ToU grid charging |

## Enforced rules (`components/deye_core/src/control.c`)

| Rule | Mechanism | Test |
|---|---|---|
| unverified / unsupported params never writable | `ctrl_can_edit` | `test_shipped_defaults_block_all_writes` |
| range and step validation, NaN rejected | `ctrl_validate` | `test_validation_rules` |
| unchanged value not written | tolerance compare | `test_validation_rules`, `test_confirm_revalidates` |
| current value must be known | `CTRL_ERR_NO_CURRENT` | `test_validation_rules` |
| fresh telemetry required | `link_allows_control` | `test_confirm_revalidates` |
| explicit confirmation dialog | `AWAITING_CONFIRM` state | `test_happy_path_requires_readback` |
| re-validation at confirm time | `ctrl_confirm` | `test_confirm_revalidates` |
| one command in flight; double tap rejected | `CTRL_ERR_BUSY` | `test_happy_path_requires_readback` |
| per-parameter cooldown from submission (60 s / 300 s) | `last_write_ms` | `test_happy_path_requires_readback` |
| HTTP acceptance is not success | `ACCEPTED` until read-back | `test_happy_path_requires_readback` |
| read-back disagreement -> FAILED | `ctrl_on_telemetry` | `test_readback_disagrees` |
| timeout / Wi-Fi loss -> UNCERTAIN, never retried | `ctrl_on_transport_error` | `test_timeout_is_uncertain_and_never_replayed` |
| UNCERTAIN blocks new commands until acknowledged | `is_busy` | same |
| stale outcomes for other ids ignored | correlation id | `test_cancel_and_mismatched_ids` |
| bounded audit log (32) | ring buffer | `test_audit_ring_bounded` |
| no writes at boot, no replay after reboot | context is RAM-only and starts IDLE | by construction |
| no writes from sliders | value entered via keypad, then confirm dialog | by construction (no sliders write) |

The display cannot bypass inverter or BMS protections: it can only ever ask
the backend to set a documented parameter within the inverter's own limits.

## Security

* Controls, Network and System screens can be protected by a 4-8 digit PIN
  (5 min unlock window). The PIN is stored in NVS in plain text: the board has
  no secure element and flash encryption is not enabled in this build.
* Wi-Fi password is never displayed or logged.
