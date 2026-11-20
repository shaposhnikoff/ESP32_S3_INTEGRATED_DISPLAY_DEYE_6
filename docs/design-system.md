# Design system

Implemented in `components/ui/ui_theme.c` and `ui_widgets.c`.

## Palette (dark / light)

| Token | Dark | Light | Use |
|---|---|---|---|
| bg | #0E1318 | #EEF1F4 | screen |
| card | #172029 | #FFFFFF | cards, bars |
| border | #263241 | #D0D7DE | card borders, grid lines |
| text / muted | #E6EDF3 / #8B98A5 | #1F2328 / #57606A | values / labels, non-live values |
| accent | #3BA7FF | #0969DA | selection, inverter |
| ok / warn / err | #3FB950 / #D29922 / #F85149 | #1A7F37 / #9A6700 / #CF222E | status |
| pv / battery / grid / load | #F2C14E / #3FB950 / #58A6FF / #BC8CFF | darker variants | energy series |

## Typography

Montserrat 48 (hero values), 24-28 (node values, titles), 20 (key values),
16 (labels, buttons), 14 (notes, nav).

## Components

| Component | Function |
|---|---|
| Card | `ui_card` |
| Key/value row | `ui_kv` |
| Button (52 px) | `ui_button`, `ui_set_enabled` |
| Note | `ui_note` |
| Status pill / alarm banner / connection indicator | status bar in `ui_main.c` |
| SOC indicator | `lv_bar` in Overview/Battery |
| Power node + flow link | `scr_overview.c` |
| Chart with min/max/empty state | `ui_chart_create`, `ui_chart_fill` |
| Confirmation dialog | `ui_confirm` |
| Info dialog | `ui_info` |
| Keypad / keyboard entry | `ui_text_input` (numeric or text, password mode) |
| Toggle group (series/range) | `scr_charts.c` |
| Settings rows | dropdown rows in `scr_system.c` |

No sliders are used for inverter values (writes must not follow drag events).
