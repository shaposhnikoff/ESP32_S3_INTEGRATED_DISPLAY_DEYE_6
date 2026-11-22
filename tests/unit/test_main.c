#include <string.h>

#include "test_harness.h"

int g_failures = 0;
int g_checks = 0;

#ifndef FIXTURE_DIR
#error "FIXTURE_DIR must be defined"
#endif

char *load_fixture(const char *name, size_t *len)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FIXTURE_DIR, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open fixture %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "cannot read fixture %s\n", path);
        exit(2);
    }
    buf[n] = '\0';
    fclose(f);
    *len = (size_t)n;
    return buf;
}

void suite_prom_parser(void);
void suite_inverter_model(void);
void suite_link_state(void);
void suite_history(void);
void suite_control(void);
void suite_fmt(void);
void suite_outage_dial(void);
void suite_outage_json(void);

int main(void)
{
    printf("prom_parser\n");
    suite_prom_parser();
    printf("inverter_model\n");
    suite_inverter_model();
    printf("link_state\n");
    suite_link_state();
    printf("history\n");
    suite_history();
    printf("control\n");
    suite_control();
    printf("fmt\n");
    suite_fmt();
    printf("outage_dial\n");
    suite_outage_dial();
    printf("outage_json\n");
    suite_outage_json();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
