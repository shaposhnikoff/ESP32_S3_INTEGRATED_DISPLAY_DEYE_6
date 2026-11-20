/* C ABI bridge for actual HTTP bodies received by the Python integration tests. */
#include "inverter_model.h"
#include "link_state.h"
#include <stdio.h>

const char *telemetry_probe(const char *body, size_t len)
{
    static char result[256];
    inverter_model_t model;
    const bool accepted = im_parse_metrics(body, len, &model);
    const link_config_t cfg = {30000, 60000, 2000, 60000};
    link_state_t link;
    link_init(&link, &cfg);
    if (accepted) {
        link_on_success(&link, 0, 1, model.meta.scrape_success.valid && model.meta.scrape_success.value > 0.5f,
                        model.meta.data_age_s.valid, model.meta.data_age_s.value);
    }
    snprintf(result, sizeof(result), "{\"accepted\":%s,\"soc\":%.1f,\"load\":%.1f,\"status\":%d,\"fresh\":%s}",
             accepted ? "true" : "false", (double)model.battery.soc_pct.value, (double)model.load.power_w.value,
             (int)model.inverter.run_status, link_freshness(&link, 0) == LINK_FRESH ? "true" : "false");
    return result;
}
