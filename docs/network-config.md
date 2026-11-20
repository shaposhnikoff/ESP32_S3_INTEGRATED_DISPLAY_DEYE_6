# Network configuration without the touchscreen

Edit `config/network.local.h`. It is ignored by Git. A complete template is
in `config/network.example.h`; never put a real password into the template.

| Setting | Meaning |
|---|---|
| `DEYE_WIFI_SSID` | Wi-Fi network name, at most 32 bytes |
| `DEYE_WIFI_PASSWORD` | WPA2 password; empty for an open network |
| `DEYE_BACKEND_HOST` | Exporter host/IP only, without `http://` or `/metrics` |
| `DEYE_BACKEND_PORT` | Exporter port, normally 9101 |
| `DEYE_POLL_INTERVAL_S` | Poll interval, 2-60 seconds |
| `DEYE_STALE_AFTER_S` | Stale threshold, 10-3600 seconds |
| `DEYE_STATIC_IP` | Empty for DHCP; otherwise a static IPv4 address |
| `DEYE_NETMASK`, `DEYE_GATEWAY`, `DEYE_DNS_SERVER` | Required with static IPv4; leave all empty for DHCP |
| `DEYE_NTP_SERVER` | Time server used for HTTPS certificate validation |

Save the file, rebuild and flash:

```bash
scripts/idf.sh build
cd build
uvx --from esptool==4.9.0 esptool.py --chip esp32s3 \
  --port /dev/cu.usbmodem21211201 --baud 460800 \
  --before default_reset --after hard_reset write_flash @flash_args
```

Use the actual serial port on your machine. This writes the application and
boot metadata without erasing the NVS partition.

On boot, a new/changed profile is saved to NVS before Wi-Fi starts. The board
then connects automatically. A stored SHA-256 digest prevents rewriting NVS
on every boot. Editing the file and flashing a rebuilt image imports the new
profile automatically. An empty SSID disables profile import and leaves saved
settings intact.

The file and the resulting provisioned firmware contain the Wi-Fi password.
Keep them private. CI builds use the empty public template instead.

Factory reset clears NVS, but a non-empty profile embedded in the running
image will be imported again on the next boot. To remove embedded credentials,
flash a build made with an empty SSID.
