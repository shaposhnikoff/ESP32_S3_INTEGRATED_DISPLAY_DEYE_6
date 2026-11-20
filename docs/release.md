# Release

1. Update `version.txt` and add `docs/release-notes/<version>.md`.
2. Tag `v<version>` and push. CI builds, validates the image header (project
   name, version, hash), packages `dist/` and publishes a GitHub release with
   the full image, the OTA image (`deye_hmi.bin`) and `SHA256SUMS`.

## OTA

* Host `deye_hmi.bin` on an HTTPS server with a publicly trusted certificate
  (the ESP-IDF CA bundle is used), e.g. the GitHub release asset URL.
* Device: System -> Set URL -> Install -> confirm. Progress is shown; the
  display restarts into the new slot.
* Wi-Fi starts SNTP against `pool.ntp.org` on the first IP address. HTTPS OTA
  waits up to 10 seconds for the clock; without time synchronization it fails
  explicitly instead of bypassing certificate date validation.
* Rollback: the new image must run 60 s without crashing before it is marked
  valid; otherwise the bootloader returns to the previous slot.
* Plain HTTP URLs are rejected unless the firmware was built with
  `CONFIG_DEYE_OTA_ALLOW_HTTP=y` (development only).
* Images are not signed (secure boot not enabled); integrity relies on TLS
  plus the ESP-IDF image hash check.
