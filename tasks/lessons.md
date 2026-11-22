# Lessons

* Keep the user's outage group and physical display model in the acceptance
  criteria: group 4.1 and Waveshare 7B are the configured targets.
* Confirm production data on the device before treating a host schedule fixture
  as complete. A source URL and a successful host parser are not device proof.
* On RGB ESP32-S3 displays, distinguish internal RAM from PSRAM. A 62 KB JSON
  body can fit in PSRAM while an 8 KB internal task stack fails to allocate.
* Exercise diagnostics after state changes. A metrics endpoint that works
  before schedule publication may fail after a non-null snapshot appears.
