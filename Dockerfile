FROM espressif/idf:v5.5.1
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Preserve registry constraints on Kconfig-triggered configure reruns.
COPY patches/esp-idf-managed-components.patch /tmp/idf-managed-components.patch
RUN patch --batch --fuzz=0 -p1 -d /opt/esp/idf < /tmp/idf-managed-components.patch

# Use one YAML emitter on ARM64 and AMD64 so the lock stays byte-identical.
COPY patches/component-manager-yaml.patch /tmp/component-manager-yaml.patch
RUN . "$IDF_PATH/export.sh" > /dev/null \
    && patch --batch --fuzz=0 "$(python -c 'from idf_component_tools.lock import manager; print(manager.__file__)')" \
       < /tmp/component-manager-yaml.patch
