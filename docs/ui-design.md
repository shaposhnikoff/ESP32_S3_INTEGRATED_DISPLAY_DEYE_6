# UI design

1024x600 landscape. Overview fills the area above a 68 px bottom navigation
bar. The other screens keep a 56 px status bar and use a scrollable 1024x476
content area. Six tabs are visible in the bottom bar; Generator, Network,
Diagnostics and System open from More.

Principles applied:

* Dark theme default (`ui_theme.c`), light theme selectable (applied after
  restart, to avoid restyling every object live).
* Touch targets >= 48 px (buttons 52 px).
* Unknown values render as `--`, never 0. The overview retains the last known
  SOC in gray when telemetry is stale, labels the header `NO DATA`, and hides
  stale power values. The Data tile shows freshness and age.
* The overview header prioritizes missing data, inverter fault/alarm, low SOC
  (25% or below), grid connection and confirmed battery discharge. Every
  state uses text and an icon as well as color.
* The battery card shows SOC and a 25/50/75% segmented bar. The inverter card
  shows run status and load. Six compact tiles show link age, battery power,
  estimated runtime or time to full, solar power, grid state and Wi-Fi RSSI.
  Today's energy totals are on Charts.
* More preserves PIN checks for Controls, Network and System. Leaving a
  protected screen on session expiry returns to Overview as before.
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

Flicker risk on the 5B (21 MHz PCLK, PSRAM framebuffers) is mitigated by the
vendor settings (bounce buffers, XIP from PSRAM, IRAM-safe ISR). Not observed
on hardware yet.
