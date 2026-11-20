#include "fmt.h"

#include <math.h>
#include <stdio.h>

void fmt_power(char *out, size_t n, im_value_t v, bool absolute)
{
    if (!v.valid) {
        snprintf(out, n, FMT_UNKNOWN " W");
        return;
    }
    float w = absolute ? fabsf(v.value) : v.value;
    if (fabsf(w) >= 10000.0f) {
        snprintf(out, n, "%.1f kW", (double)(w / 1000.0f));
    } else if (fabsf(w) >= 1000.0f) {
        snprintf(out, n, "%.2f kW", (double)(w / 1000.0f));
    } else {
        snprintf(out, n, "%.0f W", (double)w);
    }
}

void fmt_value(char *out, size_t n, im_value_t v, int decimals, const char *unit)
{
    const char *sep = (unit != NULL && unit[0] != '\0' && unit[0] != '%') ? " " : "";
    if (unit == NULL) {
        unit = "";
    }
    if (!v.valid) {
        snprintf(out, n, FMT_UNKNOWN "%s%s", sep, unit);
        return;
    }
    snprintf(out, n, "%.*f%s%s", decimals, (double)v.value, sep, unit);
}

void fmt_age_ms(char *out, size_t n, long long age_ms)
{
    if (age_ms < 0) {
        snprintf(out, n, FMT_UNKNOWN);
    } else if (age_ms < 60000) {
        snprintf(out, n, "%lld s", age_ms / 1000);
    } else if (age_ms < 3600000) {
        snprintf(out, n, "%lld min", age_ms / 60000);
    } else {
        snprintf(out, n, "%lld h", age_ms / 3600000);
    }
}

void fmt_hhmm(char *out, size_t n, unsigned minutes)
{
    snprintf(out, n, "%02u:%02u", (minutes / 60) % 24, minutes % 60);
}
