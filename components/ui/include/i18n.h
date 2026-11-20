/*
 * UI string table. English is complete; adding a language means adding a
 * column in i18n.c. Cyrillic (ru/uk) additionally requires a font with
 * Cyrillic glyphs, see docs/ui-design.md.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_EN = 0,
    LANG_COUNT,
} i18n_lang_t;

typedef enum {
    T_OVERVIEW = 0,
    T_BATTERY,
    T_SOLAR,
    T_GRID_LOAD,
    T_GENERATOR,
    T_CHARTS,
    T_CONTROLS,
    T_NETWORK,
    T_DIAGNOSTICS,
    T_SYSTEM,
    T_LIVE,
    T_STALE,
    T_OFFLINE,
    T_WAITING,
    T_NOT_CONFIGURED,
    T_CANCEL,
    T_CONFIRM,
    T_SAVE,
    T_COUNT_,
} i18n_id_t;

void i18n_set_language(i18n_lang_t lang);
const char *tr(i18n_id_t id);

#ifdef __cplusplus
}
#endif
