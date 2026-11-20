FROM espressif/idf:v5.5.1

# Preserve registry constraints on Kconfig-triggered configure reruns.
COPY patches/esp-idf-managed-components.patch /tmp/idf-managed-components.patch
RUN patch --batch --fuzz=0 -p1 -d /opt/esp/idf < /tmp/idf-managed-components.patch
