/* Runs the real UI on a memory display and saves evidence, never hardware QA. */
#include "ui.h"
#include "ui_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t draw_buffer[UI_W * UI_H * 3];
static uint8_t frame[UI_W * UI_H * 3];
static int text_submissions;
static void refresh(void);

static void navigate(ui_screen_id_t id)
{
    lv_obj_t *root = lv_screen_active();
    lv_obj_t *nav = lv_obj_get_child(root, 1);
    static const ui_screen_id_t primary[] = {SCR_OVERVIEW, SCR_BATTERY, SCR_SOLAR, SCR_GRID, SCR_CHARTS, SCR_CONTROLS};
    for (int i = 0; i < 6; i++) {
        if (primary[i] == id) {
            lv_obj_send_event(lv_obj_get_child(nav, i), LV_EVENT_CLICKED, NULL);
            return;
        }
    }
    static const ui_screen_id_t extra[] = {SCR_GENERATOR, SCR_NETWORK, SCR_DIAG, SCR_SYSTEM};
    lv_obj_send_event(lv_obj_get_child(nav, 6), LV_EVENT_CLICKED, NULL);
    lv_obj_t *panel = lv_obj_get_child(root, -1);
    assert(!lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN));
    for (int i = 0; i < 4; i++) {
        if (extra[i] == id) {
            lv_obj_send_event(lv_obj_get_child(panel, i), LV_EVENT_CLICKED, NULL);
            return;
        }
    }
    assert(false);
}

static void text_submitted(const char *text, void *user)
{
    (void)user;
    assert(strcmp(text, "12") == 0);
    text_submissions++;
}

static void test_text_dialog_submits_once(void)
{
    ui_text_input("Double confirm regression", "12", false, true, 8, text_submitted, NULL);
    lv_obj_t *overlay = lv_obj_get_child(lv_layer_top(), -1);
    lv_obj_t *keyboard = lv_obj_get_child(overlay, -1);
    lv_obj_send_event(keyboard, LV_EVENT_READY, NULL);
    lv_obj_send_event(keyboard, LV_EVENT_READY, NULL);
    assert(text_submissions == 1);
    refresh();
}

static void test_pin_blocks_immediate_navigation_and_expires(void)
{
    lv_obj_t *bar = lv_obj_get_child(lv_screen_active(), 0);
    lv_obj_t *title = lv_obj_get_child(bar, 1);
    navigate(SCR_OVERVIEW);
    refresh();
    app_settings_t settings;
    app_settings_get(&settings);
    strlcpy(settings.pin, "1234", sizeof(settings.pin));
    app_settings_set(&settings);
    navigate(SCR_NETWORK);
    assert(strcmp(lv_label_get_text(title), tr(T_OVERVIEW)) == 0);
    lv_obj_t *overlay = lv_obj_get_child(lv_layer_top(), -1);
    lv_textarea_set_text(lv_obj_get_child(overlay, 1), "1234");
    lv_obj_send_event(lv_obj_get_child(overlay, -1), LV_EVENT_READY, NULL);
    refresh();
    assert(strcmp(lv_label_get_text(title), tr(T_NETWORK)) == 0);
    lv_tick_inc(5 * 60 * 1000);
    refresh();
    assert(strcmp(lv_label_get_text(title), tr(T_OVERVIEW)) == 0);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    size_t stride = (size_t)(area->x2 - area->x1 + 1) * 3;
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(frame + ((size_t)y * UI_W + area->x1) * 3, pixels, stride);
        pixels += stride;
    }
    lv_display_flush_ready(display);
}

static void refresh(void)
{
    lv_tick_inc(500);
    lv_timer_handler();
    lv_refr_now(NULL);
}

static void capture_named(const char *directory, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%d %d\n255\n", UI_W, UI_H);
    for (size_t i = 0; i < sizeof(frame); i += 3) {
        const uint8_t rgb[] = {frame[i + 2], frame[i + 1], frame[i]};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    assert(fclose(file) == 0);
}

static void capture_screen(const char *directory, int page)
{
    char name[24];
    snprintf(name, sizeof(name), "screen-%02d", page);
    capture_named(directory, name);
}

static void capture_overview_states(const char *directory)
{
    inverter_model_t *model = app_state_model();
    model->battery.soc_pct = im_val(86);
    model->battery.power_w = im_val(929);
    model->load.power_w = im_val(908);
    model->pv.total_power_w = im_val(0);
    model->grid.connected = im_val(0);
    model->inverter.run_status = IM_RUN_NORMAL;
    assert(ui_capture_select_screen("overview"));
    refresh();
    capture_named(directory, "state-ok");

    model->battery.soc_pct = im_val(62);
    model->battery.power_w = im_val(-1200);
    model->grid.connected = im_val(1);
    assert(ui_capture_select_screen("overview"));
    refresh();
    capture_named(directory, "state-grid");

    model->battery.soc_pct = im_val(24);
    model->battery.power_w = im_val(929);
    model->grid.connected = im_val(0);
    assert(ui_capture_select_screen("overview"));
    refresh();
    capture_named(directory, "state-warn");

    model->battery.soc_pct = im_val(86);
    model->battery.power_w = im_val(0);
    model->load.power_w = im_val(0);
    model->inverter.run_status = IM_RUN_FAULT;
    assert(ui_capture_select_screen("overview"));
    refresh();
    capture_named(directory, "state-fault");

    model->inverter.run_status = IM_RUN_NORMAL;
    lv_tick_inc(3 * 60 * 1000);
    assert(ui_capture_select_screen("overview"));
    refresh();
    capture_named(directory, "state-stale");
    link_on_success(app_state_link(), app_now_ms(), 20, true, true, 1);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    lv_init();
    lv_display_t *display = lv_display_create(UI_W, UI_H);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    app_settings_t settings = {.backend_port = 9101,
                               .poll_interval_s = 3,
                               .stale_after_s = 30,
                               .battery_capacity_wh = 15000,
                               .battery_cutoff_pct = 20};
    strlcpy(settings.backend_host, "mock", sizeof(settings.backend_host));
    app_state_init(&settings);
    FILE *fixture = fopen(argv[1], "rb");
    assert(fixture);
    char body[65536];
    size_t len = fread(body, 1, sizeof(body), fixture);
    assert(!ferror(fixture));
    fclose(fixture);
    assert(im_parse_metrics(body, len, app_state_model()));
    link_on_success(app_state_link(), 0, 20, true, true, 1);
    app_state_wifi()->status = WIFI_ST_CONNECTED;
    app_state_wifi()->rssi = -56;
    ui_start(display);
    refresh();
    for (int i = 0; i < SCR_COUNT; i++) {
        navigate((ui_screen_id_t)i);
        refresh();
        lv_obj_t *bar = lv_obj_get_child(lv_screen_active(), 0);
        const char *title = lv_label_get_text(lv_obj_get_child(bar, 1));
        assert(strcmp(title, tr((i18n_id_t)i)) == 0);
        capture_screen(argv[2], i);
    }
    puts("UI smoke: 10 screens navigated and rendered");
    capture_overview_states(argv[2]);
    test_text_dialog_submits_once();
    test_pin_blocks_immediate_navigation_and_expires();
    assert(!ui_capture_select_screen("controls"));
    assert(!ui_capture_select_screen("network"));
    assert(!ui_capture_select_screen("system"));
    assert(ui_capture_select_screen("solar"));
    puts("UI regressions: double confirm, PIN session and capture restrictions passed");
    lv_deinit();
    return 0;
}
