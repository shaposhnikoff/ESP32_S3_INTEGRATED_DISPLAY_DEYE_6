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
    lv_obj_t *nav = lv_obj_get_child(lv_screen_active(), 1);
    lv_obj_t *bar = lv_obj_get_child(lv_screen_active(), 0);
    lv_obj_t *title = lv_obj_get_child(bar, 1);
    lv_obj_send_event(lv_obj_get_child(nav, SCR_OVERVIEW), LV_EVENT_CLICKED, NULL);
    refresh();
    app_settings_t settings;
    app_settings_get(&settings);
    strlcpy(settings.pin, "1234", sizeof(settings.pin));
    app_settings_set(&settings);
    lv_obj_send_event(lv_obj_get_child(nav, SCR_NETWORK), LV_EVENT_CLICKED, NULL);
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

static void capture(const char *directory, int page)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/screen-%02d.ppm", directory, page);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%d %d\n255\n", UI_W, UI_H);
    for (size_t i = 0; i < sizeof(frame); i += 3) {
        const uint8_t rgb[] = {frame[i + 2], frame[i + 1], frame[i]};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    assert(fclose(file) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    lv_init();
    lv_display_t *display = lv_display_create(UI_W, UI_H);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    app_settings_t settings = {.backend_port = 9101, .poll_interval_s = 3, .stale_after_s = 30};
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
    ui_start(display);
    refresh();
    lv_obj_t *nav = lv_obj_get_child(lv_screen_active(), 1);
    for (int i = 0; i < SCR_COUNT; i++) {
        lv_obj_send_event(lv_obj_get_child(nav, i), LV_EVENT_CLICKED, NULL);
        refresh();
        lv_obj_t *bar = lv_obj_get_child(lv_screen_active(), 0);
        const char *title = lv_label_get_text(lv_obj_get_child(bar, 1));
        assert(strcmp(title, tr((i18n_id_t)i)) == 0);
        capture(argv[2], i);
    }
    puts("UI smoke: 10 screens navigated and rendered");
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
