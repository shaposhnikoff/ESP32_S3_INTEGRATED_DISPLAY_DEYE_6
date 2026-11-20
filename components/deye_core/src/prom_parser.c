#include "prom_parser.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool is_name_char(char c, bool first)
{
    if (isalpha((unsigned char)c) || c == '_' || c == ':') {
        return true;
    }
    return !first && isdigit((unsigned char)c);
}

static const char *skip_spaces(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    return p;
}

/* Finds the closing brace of a label block, honouring quoted values. */
static const char *find_label_end(const char *p, const char *end)
{
    bool in_quotes = false;
    while (p < end) {
        if (in_quotes) {
            if (*p == '\\' && p + 1 < end) {
                p += 2;
                continue;
            }
            if (*p == '"') {
                in_quotes = false;
            }
        } else if (*p == '"') {
            in_quotes = true;
        } else if (*p == '}') {
            return p;
        }
        p++;
    }
    return NULL;
}

static bool parse_value(const char *p, const char *end, double *out)
{
    char buf[48];
    size_t n = 0;
    while (p < end && *p != ' ' && *p != '\t' && n < sizeof(buf) - 1) {
        buf[n++] = *p++;
    }
    if (n == 0 || (p < end && *p != ' ' && *p != '\t')) {
        return false;
    }
    buf[n] = '\0';
    if (strcmp(buf, "NaN") == 0) {
        *out = NAN;
        return true;
    }
    if (strcmp(buf, "+Inf") == 0 || strcmp(buf, "Inf") == 0) {
        *out = INFINITY;
        return true;
    }
    if (strcmp(buf, "-Inf") == 0) {
        *out = -INFINITY;
        return true;
    }
    char *conv_end = NULL;
    double v = strtod(buf, &conv_end);
    if (conv_end == buf || *conv_end != '\0') {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_line(const char *p, const char *end, prom_sample_t *s)
{
    const char *name = p;
    if (p >= end || !is_name_char(*p, true)) {
        return false;
    }
    while (p < end && is_name_char(*p, false)) {
        p++;
    }
    s->name = name;
    s->name_len = (size_t)(p - name);
    s->labels = NULL;
    s->labels_len = 0;

    if (p < end && *p == '{') {
        const char *close = find_label_end(p + 1, end);
        if (close == NULL) {
            return false;
        }
        s->labels = p + 1;
        s->labels_len = (size_t)(close - (p + 1));
        p = close + 1;
    }
    if (p >= end || (*p != ' ' && *p != '\t')) {
        return false;
    }
    p = skip_spaces(p, end);
    if (!parse_value(p, end, &s->value)) {
        return false;
    }
    return true; /* an optional timestamp after the value is ignored */
}

prom_parse_stats_t prom_parse(const char *text, size_t len, prom_sample_cb cb, void *ctx)
{
    prom_parse_stats_t stats = {0};
    if (text == NULL) {
        return stats;
    }
    const char *p = text;
    const char *end = text + len;
    while (p < end) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        const char *line_end = eol ? eol : end;
        const char *q = skip_spaces(p, line_end);
        /* trim trailing CR / whitespace */
        const char *t = line_end;
        while (t > q && (t[-1] == '\r' || t[-1] == ' ' || t[-1] == '\t')) {
            t--;
        }
        if (q < t && *q != '#') {
            prom_sample_t s;
            if (parse_line(q, t, &s)) {
                stats.samples++;
                if (cb != NULL && !cb(&s, ctx)) {
                    return stats;
                }
            } else {
                stats.malformed++;
            }
        }
        p = eol ? eol + 1 : end;
    }
    return stats;
}

bool prom_name_is(const prom_sample_t *sample, const char *name)
{
    size_t n = strlen(name);
    return sample->name_len == n && memcmp(sample->name, name, n) == 0;
}

bool prom_label_get(const prom_sample_t *sample, const char *key, char *out, size_t out_len)
{
    if (sample->labels == NULL || out == NULL || out_len == 0) {
        return false;
    }
    out[0] = '\0';
    const size_t key_len = strlen(key);
    const char *p = sample->labels;
    const char *end = p + sample->labels_len;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == ',')) {
            p++;
        }
        const char *k = p;
        while (p < end && *p != '=') {
            p++;
        }
        if (p >= end) {
            return false;
        }
        const size_t klen = (size_t)(p - k);
        p++; /* '=' */
        if (p >= end || *p != '"') {
            return false;
        }
        p++;
        const bool match = (klen == key_len && memcmp(k, key, klen) == 0);
        size_t n = 0;
        bool overflow = false;
        while (p < end && *p != '"') {
            char c = *p;
            if (c == '\\' && p + 1 < end) {
                p++;
                c = (*p == 'n') ? '\n' : *p;
            }
            if (match) {
                if (n + 1 < out_len) {
                    out[n++] = c;
                } else {
                    overflow = true;
                }
            }
            p++;
        }
        if (p >= end) {
            return false;
        }
        p++; /* closing quote */
        if (match) {
            out[n] = '\0';
            return !overflow;
        }
    }
    return false;
}
