# Build

The default helper builds the local `deye-hmi-idf:5.5.1` image from the pinned
`espressif/idf:v5.5.1` base. Its SDK CMake patch excludes already-managed
components from the local-root dependency list on configure reruns.
Without that filter, a clean Kconfig-dependent configure can reinterpret
registry components as local overrides and replace the pinned LVGL release.
The Component Manager also uses an explicitly selected Python YAML emitter:
The validated ARM64 image lacks the optional C emitter; AMD64 CI produced
different scalar wrapping. Selecting one emitter keeps the lock byte-identical without relaxing
the CI lock check. The build context contains only the Dockerfile and patches.

`IDF_IMAGE` may select a prebuilt compatible image; setting it skips the local
image build. A stock, unpatched image does not provide the same build contract.

Requirements: Docker (or a native ESP-IDF v5.5.1 install). The pinned image
`espressif/idf:v5.5.1` is used locally and in CI.

```bash
scripts/idf.sh set-target esp32s3   # once per fresh checkout
scripts/idf.sh build
scripts/idf.sh size
scripts/package_release.sh          # dist/deye-hmi-<version>/ + tarball
```

Host checks:

```bash
scripts/host_tests.sh               # needs cmake, cc, uv
scripts/format.sh --check           # needs uv
scripts/static_analysis.sh          # needs cppcheck
```

Dependencies are pinned in `main/idf_component.yml` and resolved hashes in
`dependencies.lock` (commit it; CI fails if it changes). Firmware version is
`version.txt`.

For network settings, copy `config/network.example.h` to the ignored
`config/network.local.h`, edit it and rebuild. See network-config.md.
Other build options use `idf.py menuconfig` -> *Deye HMI*.
An arbitrary `sdkconfig.local` is not automatically loaded by this project.
