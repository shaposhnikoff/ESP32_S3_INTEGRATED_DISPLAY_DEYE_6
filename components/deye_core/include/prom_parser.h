/*
 * Minimal, allocation-free parser for the Prometheus text exposition format
 * (version 0.0.4). Only what the Deye exporter emits is required, but the
 * parser tolerates comments, blank lines, escaped label values, special float
 * values and optional timestamps.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name; /* not NUL-terminated */
    size_t name_len;
    const char *labels; /* raw label body between braces, may be NULL */
    size_t labels_len;
    double value;
} prom_sample_t;

typedef struct {
    size_t samples;   /* successfully parsed sample lines */
    size_t malformed; /* sample lines that could not be parsed */
} prom_parse_stats_t;

/* Return false from the callback to stop parsing early. */
typedef bool (*prom_sample_cb)(const prom_sample_t *sample, void *ctx);

/* Parses `len` bytes of exposition text. Never reads past `len`. */
prom_parse_stats_t prom_parse(const char *text, size_t len, prom_sample_cb cb, void *ctx);

/* True if the sample name equals the NUL-terminated `name`. */
bool prom_name_is(const prom_sample_t *sample, const char *name);

/*
 * Copies the unescaped value of label `key` into `out` (always
 * NUL-terminated when out_len > 0). Returns false if the label is absent or
 * does not fit.
 */
bool prom_label_get(const prom_sample_t *sample, const char *key, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
