# Flashing

The validated board exposes its native USB-Serial/JTAG port as
`/dev/cu.usbmodem...` on macOS. It also has a UART USB connector.
If the port is not detected, hold BOOT while connecting, then release.

Docker Desktop on macOS cannot pass USB serial devices into containers, so
flash from the host with the pinned esptool below:

```bash
# First flash / recovery: single merged image at offset 0
uvx --from esptool==4.9.0 esptool.py --chip esp32s3 -p PORT -b 460800 write_flash 0x0 \
  dist/deye-hmi-0.2.0/deye-hmi-0.2.0-full.bin

# Or individual images (see flash_args)
cd build && uvx --from esptool==4.9.0 esptool.py --chip esp32s3 -p PORT -b 460800 --before default_reset \
  --after hard_reset write_flash @flash_args
```

On Linux the container can flash directly:
`docker run --rm --device /dev/ttyUSB0 ... idf.py -p /dev/ttyUSB0 flash monitor`.

Recovery: `esptool --chip esp32s3 -p PORT erase_flash` and flash the full
image again (this also erases Wi-Fi/backend settings).

Use individual images via `@flash_args` to preserve NVS. A merged full image
also covers gaps between partitions and can clear existing settings.
OTA is implemented but has not yet been validated on hardware (release.md).
Press RESET after a serial flash if the board does not restart.
