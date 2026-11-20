#include <string.h>

#include "prom_parser.h"
#include "test_harness.h"

typedef struct {
    int n;
    double last;
    char label[32];
} collect_t;

static bool collect(const prom_sample_t *s, void *ctx)
{
    collect_t *c = ctx;
    c->n++;
    c->last = s->value;
    if (prom_name_is(s, "m_labels")) {
        prom_label_get(s, "b", c->label, sizeof(c->label));
    }
    return true;
}

static void test_basic_lines(void)
{
    const char *t = "# HELP x y\n# TYPE x gauge\n\nx 1.5\r\n  y{a=\"1\"} -2e3 1700000000000\n";
    collect_t c = {0};
    prom_parse_stats_t st = prom_parse(t, strlen(t), collect, &c);
    CHECK(st.samples == 2);
    CHECK(st.malformed == 0);
    CHECK(c.n == 2);
    CHECK_NEAR(c.last, -2000.0, 1e-9);
}

static void test_escaped_labels(void)
{
    const char *t = "m_labels{a=\"x,}y\",b=\"q\\\"uote\"} 3\n";
    collect_t c = {0};
    prom_parse_stats_t st = prom_parse(t, strlen(t), collect, &c);
    CHECK(st.samples == 1);
    CHECK(strcmp(c.label, "q\"uote") == 0);
}

static void test_special_values(void)
{
    const char *t = "a NaN\nb +Inf\nc -Inf\n";
    collect_t c = {0};
    prom_parse_stats_t st = prom_parse(t, strlen(t), collect, &c);
    CHECK(st.samples == 3);
    CHECK(isinf(c.last) && c.last < 0);
}

static void test_malformed(void)
{
    const char *t = "1bad 3\nnovalue\nx{a=\"1\" 2\nx 1abc\ngood 4\n";
    collect_t c = {0};
    prom_parse_stats_t st = prom_parse(t, strlen(t), collect, &c);
    CHECK(st.samples == 1);
    CHECK(st.malformed == 4);
    CHECK_NEAR(c.last, 4.0, 1e-9);
}

static void test_no_trailing_newline_and_bounds(void)
{
    const char buf[] = "a 1\nb 22XXXX";
    collect_t c = {0};
    /* length excludes the XXXX garbage after "b 22" */
    prom_parse_stats_t st = prom_parse(buf, 8, collect, &c);
    CHECK(st.samples == 2);
    CHECK_NEAR(c.last, 22.0, 1e-9);
}

static bool capture_first(const prom_sample_t *s, void *ctx)
{
    *(prom_sample_t *)ctx = *s;
    return false;
}

static void test_label_get_limits(void)
{
    const char *t = "m{k=\"0123456789\"} 1\nn 2\n";
    prom_sample_t s;
    prom_parse_stats_t st = prom_parse(t, strlen(t), capture_first, &s);
    CHECK(st.samples == 1); /* stopped early by callback */
    char small[4];
    char big[16];
    CHECK(!prom_label_get(&s, "k", small, sizeof(small)));
    CHECK(prom_label_get(&s, "k", big, sizeof(big)));
    CHECK(strcmp(big, "0123456789") == 0);
    CHECK(!prom_label_get(&s, "missing", big, sizeof(big)));
}

void suite_prom_parser(void)
{
    RUN(test_basic_lines);
    RUN(test_escaped_labels);
    RUN(test_special_values);
    RUN(test_malformed);
    RUN(test_no_trailing_newline_and_bounds);
    RUN(test_label_get_limits);
}
