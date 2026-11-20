/* Generator: the exporter currently provides no generator measurements. */
#include <stdio.h>

#include "fmt.h"
#include "ui_internal.h"

static lv_obj_t *s_v, *s_f, *s_p, *s_avail;
static lv_obj_t *s_tou;

static void create(lv_obj_t *page)
{
    ui_flex_row(page, UI_PAD);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    const int32_t w = (UI_CONTENT_W - 3 * UI_PAD) / 2;

    lv_obj_t *m = ui_card(page, "Generator input", w, 330);
    s_avail = ui_kv(m, "Measurements");
    s_v = ui_kv(m, "Voltage");
    s_f = ui_kv(m, "Frequency");
    s_p = ui_kv(m, "Power");
    ui_note(m, "The backend exporter does not publish generator-port metrics. Values stay '--' and are "
               "never estimated. Support requires the exporter to add them.");

    lv_obj_t *c = ui_card(page, "Generator charging (from Time of Use)", w, 330);
    s_tou = ui_note(c, "");
    lv_obj_set_style_text_font(s_tou, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_tou, g_pal.text, 0);
    ui_note(c, "Read from deye_tou_charge{source=\"gen\"}. Read-only; see Controls.");
}

static void update(const app_snapshot_t *s, bool changed)
{
    if (!changed) {
        return;
    }
    const im_generator_t *g = &s->model.generator;
    char buf[32];
    lv_label_set_text(s_avail, g->reported ? "Reported" : "Not reported by backend");
    fmt_value(buf, sizeof(buf), g->voltage_v, 1, "V");
    lv_label_set_text(s_v, buf);
    fmt_value(buf, sizeof(buf), g->frequency_hz, 2, "Hz");
    lv_label_set_text(s_f, buf);
    fmt_power(buf, sizeof(buf), g->power_w, false);
    lv_label_set_text(s_p, buf);

    char txt[256];
    size_t off = 0;
    bool any = false;
    for (int i = 0; i < IM_TOU_SLOTS && off < sizeof(txt); i++) {
        const im_tou_slot_t *sl = &s->model.tou.slots[i];
        if (!sl->present) {
            continue;
        }
        any = true;
        char t[8];
        fmt_hhmm(t, sizeof(t), sl->start_minutes);
        const char *st = !sl->gen_charge.valid ? "unknown" : (sl->gen_charge.value > 0.5f ? "ON" : "off");
        off += (size_t)snprintf(txt + off, sizeof(txt) - off, "Slot %d (from %s): %s\n", i + 1, t, st);
    }
    lv_label_set_text(s_tou, any ? txt : "No Time of Use data");
}

const ui_screen_t scr_generator = {T_GENERATOR, LV_SYMBOL_SETTINGS, false, create, update};
