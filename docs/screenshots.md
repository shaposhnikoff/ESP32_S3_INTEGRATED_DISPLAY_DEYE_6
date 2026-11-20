# Screenshots directly from the ESP32

When `CONFIG_DEYE_SCREENSHOT_HTTP=y`, the display exposes diagnostics on HTTP
port 80 after joining Wi-Fi:

| Request | Result |
|---|---|
| `GET /screenshot.bmp` | Current monitoring screen as RGB565 BMP |
| `GET /screenshot.bmp?screen=overview` | Select Overview and capture it |
| `GET /health` | Display HTTP service status |
| `GET /metrics` | Display uptime and available heap/PSRAM |

Supported screen names: `overview`, `battery`, `solar`, `grid`, `generator`,
`charts`, `diagnostics`. Controls, Network, System and open modal dialogs are
excluded. A screen query changes only the visible monitoring tab.

```bash
curl --fail "http://<display-ip>/screenshot.bmp?screen=battery" -o battery.bmp
# macOS, optional PNG conversion
sips -s format png battery.bmp --out battery.png
```

The image is rendered by LVGL **on the running ESP32**, under its display lock,
using the actual application state. After selecting a tab, capture waits for
the standard theme transition without holding the display lock. Network
transfer also runs after releasing that lock. Only one temporary RGB565 image
is allocated; no continuous capture loop runs.

Files in `docs/screenshots/` are real captures from the 1024x600 board on
2026-11-20 with live inverter telemetry. `build-ui/screens/` contains separate
host simulations and is not published.

This service has no authentication or TLS. Use it on a trusted LAN only, or
disable *Deye HMI -> Enable LAN screenshots and display diagnostics*.
