#include <string.h>

#include "fmt.h"
#include "test_harness.h"

static void test_fmt(void)
{
    char b[32];
    fmt_power(b, sizeof(b), im_unknown(), false);
    CHECK(strcmp(b, "-- W") == 0);
    fmt_power(b, sizeof(b), im_val(738), false);
    CHECK(strcmp(b, "738 W") == 0);
    fmt_power(b, sizeof(b), im_val(-1250), true);
    CHECK(strcmp(b, "1.25 kW") == 0);
    fmt_power(b, sizeof(b), im_val(12345), false);
    CHECK(strcmp(b, "12.3 kW") == 0);
    fmt_value(b, sizeof(b), im_val(53.05f), 1, "V");
    CHECK(strcmp(b, "53.0 V") == 0 || strcmp(b, "53.1 V") == 0);
    fmt_value(b, sizeof(b), im_val(94), 0, "%");
    CHECK(strcmp(b, "94%") == 0);
    fmt_value(b, sizeof(b), im_unknown(), 0, "%");
    CHECK(strcmp(b, "--%") == 0);
    fmt_age_ms(b, sizeof(b), -1);
    CHECK(strcmp(b, "--") == 0);
    fmt_age_ms(b, sizeof(b), 4200);
    CHECK(strcmp(b, "4 s") == 0);
    fmt_age_ms(b, sizeof(b), 3 * 60000);
    CHECK(strcmp(b, "3 min") == 0);
    fmt_hhmm(b, sizeof(b), 1380);
    CHECK(strcmp(b, "23:00") == 0);
}

void suite_fmt(void)
{
    RUN(test_fmt);
}
