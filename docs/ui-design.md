# UI design

1024x600 landscape. Layout: 56 px status bar, 140 px navigation rail with 10
tabs (~52 px tall each), 884x544 content area.

Principles applied:

* Dark theme default (`ui_theme.c`), light theme selectable (applied after
  restart, to avoid restyling every object live).
* Touch targets >= 48 px (buttons 52 px).
* Unknown values render as `--`, never 0. Non-live values are drawn in the
  muted colour; the status bar shows `LIVE / STALE / OFFLINE / WAITING /
  SETUP` as **text + icon + colour**, and the data age.
* Alarms: inverter ALARM/FAULT and "exporter lost inverter" appear as red text
  with a warning icon in the status bar.
* Energy flow: line width scales with power vs rated power; idle paths are
  dashed grey. Arrows are shown only where the direction is known: PV -> inverter,
  inverter -> house, battery by verified sign; grid shows `?` and
  "Direction unverified" until the sign convention is configured.
* Incremental updates: screens are built once; a 500 ms LVGL timer updates
  labels from a snapshot; every 5 s a re-render happens even without new data
  so ageing is visible.
* No network or flash-heavy work in the LVGL task besides small NVS commits on
  explicit user action.

Fonts: built-in Montserrat 14-48 (Latin only). Localisation: string table in
`components/ui/i18n.c` with a language enum; Russian/Ukrainian need a
generated Cyrillic font (e.g. `lv_font_conv` with Montserrat + range
0x400-0x4FF) and translated columns. Not done.

Flicker risk on the 5B (21 MHz PCLK, PSRAM framebuffers) is mitigated by the
vendor settings (bounce buffers, XIP from PSRAM, IRAM-safe ISR). Not observed
on hardware yet.
