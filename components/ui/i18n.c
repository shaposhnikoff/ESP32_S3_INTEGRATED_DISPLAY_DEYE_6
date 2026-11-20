#include "i18n.h"

static i18n_lang_t s_lang = LANG_EN;

static const char *const k_strings[T_COUNT_][LANG_COUNT] = {
    [T_OVERVIEW] = {"Overview"},     [T_BATTERY] = {"Battery"},     [T_SOLAR] = {"Solar"},
    [T_GRID_LOAD] = {"Grid & Load"}, [T_GENERATOR] = {"Generator"}, [T_CHARTS] = {"Charts"},
    [T_CONTROLS] = {"Controls"},     [T_NETWORK] = {"Network"},     [T_DIAGNOSTICS] = {"Diagnostics"},
    [T_SYSTEM] = {"System"},         [T_LIVE] = {"LIVE"},           [T_STALE] = {"STALE"},
    [T_OFFLINE] = {"OFFLINE"},       [T_WAITING] = {"WAITING"},     [T_NOT_CONFIGURED] = {"SETUP"},
    [T_CANCEL] = {"Cancel"},         [T_CONFIRM] = {"Confirm"},     [T_SAVE] = {"Save"},
};

void i18n_set_language(i18n_lang_t lang)
{
    if (lang < LANG_COUNT) {
        s_lang = lang;
    }
}

const char *tr(i18n_id_t id)
{
    if (id >= T_COUNT_) {
        return "?";
    }
    const char *s = k_strings[id][s_lang];
    return s ? s : k_strings[id][LANG_EN];
}
