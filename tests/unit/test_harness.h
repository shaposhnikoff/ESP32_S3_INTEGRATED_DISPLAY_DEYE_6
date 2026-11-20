/* Tiny dependency-free test harness. */
#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

extern int g_failures;
extern int g_checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        g_checks++;                                                                                                    \
        if (!(cond)) {                                                                                                 \
            g_failures++;                                                                                              \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);                                   \
        }                                                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(fabs((double)(a) - (double)(b)) <= (eps))

#define RUN(fn)                                                                                                        \
    do {                                                                                                               \
        printf("  %s\n", #fn);                                                                                         \
        fn();                                                                                                          \
    } while (0)

char *load_fixture(const char *name, size_t *len);
