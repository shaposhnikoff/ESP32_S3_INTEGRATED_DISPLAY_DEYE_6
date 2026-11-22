# UI design

The Waveshare 7B uses a 1024x600 landscape canvas. The current firmware builds
only Overview. A legend replaces the bottom navigation, leaving more internal
RAM available for Wi-Fi, TLS and screenshots.
The single-screen display does not apply the legacy NVS backlight timeout.

Principles applied:

* Dark theme default (`ui_theme.c`).
* Unknown values render as `--`, never 0. The overview retains the last known
  SOC in gray when telemetry is stale, labels the header `NO DATA`, and hides
  stale power values. The Data tile shows freshness and age.
* The overview header prioritizes missing data, inverter fault, low SOC
  (warning below 40%, critical below 20%), and measured grid connection. Every
  state uses text and an icon as well as color.
* The left card has a 24-hour Europe/Kyiv dial with hourly ticks, 00/06/12/18
  labels, a white current-time hand, SOC and today's confirmed outage arcs.
  With no fresh schedule, it uses
  a neutral ring and explicitly says the schedule is unavailable. The right
  card shows inverter state and load. Six tiles show link age, battery power,
  estimated runtime, solar power, measured grid state and Wi-Fi RSSI.
  There is no energy-total tile on this screen.
* Incremental updates: screens are built once; a 500 ms LVGL timer updates
  labels from a snapshot; every 5 s a re-render happens even without new data
  so ageing is visible.
* No network or flash-heavy work in the LVGL task besides small NVS commits on
  explicit user action.

Fonts: built-in Montserrat 14-48 plus subsetted Montserrat ExtraBold for the
overview's large status, SOC and values (SIL OFL, see `components/ui/fonts/`).
Localisation: string table in
`components/ui/i18n.c` with a language enum; Russian/Ukrainian need a
generated Cyrillic font (e.g. `lv_font_conv` with Montserrat + range
0x400-0x4FF) and translated columns. Not done.

The 7B driver uses 24 MHz RGB timing, PSRAM framebuffers and ten-line partial
rendering buffers. The live 7B screen and schedule were physically verified;
long-running stability still needs testing.
