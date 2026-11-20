/* Copy to network.local.h, fill in your network, then build and flash.
 * network.local.h is ignored by Git. Keep both it and provisioned images private.
 */
#pragma once

#define DEYE_WIFI_SSID ""
#define DEYE_WIFI_PASSWORD ""

#define DEYE_BACKEND_HOST "192.0.2.39"
#define DEYE_BACKEND_PORT 9101
#define DEYE_POLL_INTERVAL_S 3
#define DEYE_STALE_AFTER_S 30

/* Empty address = DHCP. For a static IPv4 address, fill in all four fields. */
#define DEYE_STATIC_IP ""
#define DEYE_NETMASK ""
#define DEYE_GATEWAY ""
#define DEYE_DNS_SERVER ""
#define DEYE_NTP_SERVER "pool.ntp.org"
