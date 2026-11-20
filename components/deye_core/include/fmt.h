/* Display formatting helpers that render unknown values explicitly. */
#pragma once

#include <stddef.h>

#include "inverter_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FMT_UNKNOWN "--"

/* Power: "738 W", "1.25 kW"; magnitude only when `absolute` is true. */
void fmt_power(char *out, size_t n, im_value_t v, bool absolute);
/* Generic fixed-precision value with unit, e.g. "53.1 V". */
void fmt_value(char *out, size_t n, im_value_t v, int decimals, const char *unit);
/* Age like "3 s", "4 min", "2 h". Negative => FMT_UNKNOWN. */
void fmt_age_ms(char *out, size_t n, long long age_ms);
/* Minutes since midnight to "HH:MM". */
void fmt_hhmm(char *out, size_t n, unsigned minutes);

#ifdef __cplusplus
}
#endif
