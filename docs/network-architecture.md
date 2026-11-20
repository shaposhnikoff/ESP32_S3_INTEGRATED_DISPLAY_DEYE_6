# Network architecture

* Wi-Fi STA only. Credentials in NVS namespace `hmi`; the driver's own storage
  is set to RAM so there is a single copy to erase.
* WPA2 minimum auth threshold when a password is set (no silent downgrade to an
  open AP with the same SSID). Open networks need explicit confirmation.
* Reconnect: on disconnect a one-shot timer retries with 2 s * 2^n, capped at
  64 s; reset after an IP is obtained.
* Backend: `http://<host>:<port>/metrics`, host/port set on the device (no
  compiled-in default for release builds).
* Polling backoff separate from Wi-Fi backoff: see backend-integration.md.
* Plain HTTP is used because the exporter offers nothing else. Data is
  read-only and non-secret; this is acceptable on a trusted LAN and documented
  as a limitation. A future write API must use authentication (and TLS where
  the backend supports it).
* OTA: HTTPS with the ESP-IDF CA bundle; HTTP only with the
  `CONFIG_DEYE_OTA_ALLOW_HTTP` development option.
* SNTP starts after the first IP address (`pool.ntp.org`); HTTPS OTA requires
  a synchronized clock. NTP/DNS access is needed after a cold boot.
* Transport abstraction: only `telemetry.c` knows HTTP. A WebSocket/MQTT
  transport would replace `fetch()` and keep `im_parse_metrics` and
  `link_state` unchanged.
