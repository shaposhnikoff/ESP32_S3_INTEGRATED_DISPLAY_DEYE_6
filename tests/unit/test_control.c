#include <string.h>

#include "control.h"
#include "test_harness.h"

/* Tests use a copy of the descriptor table semantics through a context whose
 * capabilities are enabled; verification is forced via a test hook below. */

static ctrl_ctx_t g_c;

static void init_with_caps(bool api)
{
    ctrl_capabilities_t caps = {0};
    caps.backend_write_api = api;
    for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
        caps.param_supported[i] = api;
    }
    ctrl_init(&g_c, &caps);
}

static void test_shipped_defaults_block_all_writes(void)
{
    /* As shipped: no parameter is verified and the backend has no write API. */
    init_with_caps(false);
    for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
        CHECK(ctrl_can_edit(&g_c, (ctrl_param_id_t)i) == CTRL_ERR_UNVERIFIED);
        CHECK(!ctrl_param((ctrl_param_id_t)i)->verified);
    }
    CHECK(ctrl_propose(&g_c, CTRL_PARAM_TOU_SLOT1_SOC, im_val(99), 80, true, 0) == CTRL_ERR_UNVERIFIED);
    CHECK(g_c.active.state == CMD_IDLE);
    CHECK(ctrl_can_edit(&g_c, CTRL_PARAM_COUNT) == CTRL_ERR_UNKNOWN_PARAM);
}

static void test_current_value_mapping(void)
{
    inverter_model_t m;
    im_reset(&m);
    CHECK(!ctrl_current_value(&m, CTRL_PARAM_TOU_SLOT1_SOC).valid);
    m.tou.slots[0].soc_pct = im_val(99);
    CHECK(ctrl_current_value(&m, CTRL_PARAM_TOU_SLOT1_SOC).valid);
}

/*
 * The lifecycle below is exercised through ctrl_test_force_verified(), a
 * test-only build hook that marks descriptors verified (see CMakeLists).
 */
void ctrl_test_force_verified(bool on);

static void test_validation_rules(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    const ctrl_param_id_t p = CTRL_PARAM_TOU_SLOT1_SOC;
    CHECK(ctrl_validate(&g_c, p, im_val(99), 80, false, 0) == CTRL_ERR_LINK);
    CHECK(ctrl_validate(&g_c, p, im_unknown(), 80, true, 0) == CTRL_ERR_NO_CURRENT);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 5, true, 0) == CTRL_ERR_OUT_OF_RANGE);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 101, true, 0) == CTRL_ERR_OUT_OF_RANGE);
    CHECK(ctrl_validate(&g_c, p, im_val(99), NAN, true, 0) == CTRL_ERR_OUT_OF_RANGE);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 80.5f, true, 0) == CTRL_ERR_STEP);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 99, true, 0) == CTRL_ERR_UNCHANGED);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 80, true, 0) == CTRL_OK);
    CHECK(ctrl_validate(&g_c, CTRL_PARAM_TOU_SLOT1_POWER, im_val(6000), 5050, true, 0) == CTRL_ERR_STEP);
    init_with_caps(false);
    CHECK(ctrl_validate(&g_c, p, im_val(99), 80, true, 0) == CTRL_ERR_UNSUPPORTED);
    ctrl_test_force_verified(false);
}

static void test_happy_path_requires_readback(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    const ctrl_param_id_t p = CTRL_PARAM_TOU_SLOT1_SOC;
    CHECK(ctrl_propose(&g_c, p, im_val(99), 80, true, 1000) == CTRL_OK);
    CHECK(g_c.active.state == CMD_AWAITING_CONFIRM);
    const uint32_t id = g_c.active.id;
    /* a second (e.g. double-tap) proposal is rejected while pending */
    CHECK(ctrl_propose(&g_c, p, im_val(99), 70, true, 1001) == CTRL_ERR_BUSY);
    CHECK(ctrl_confirm(&g_c, im_val(99), true, 2000) == CTRL_OK);
    CHECK(g_c.active.state == CMD_SUBMITTED);
    /* duplicate confirm does nothing */
    CHECK(ctrl_confirm(&g_c, im_val(99), true, 2001) == CTRL_ERR_NOT_CONFIRMED);
    ctrl_on_accepted(&g_c, id, 2100);
    CHECK(g_c.active.state == CMD_ACCEPTED); /* HTTP acceptance is not success */

    inverter_model_t m;
    im_reset(&m);
    m.tou.slots[0].soc_pct = im_val(99);
    ctrl_on_telemetry(&g_c, &m, 5000);
    CHECK(g_c.active.state == CMD_ACCEPTED);
    m.tou.slots[0].soc_pct = im_val(80);
    ctrl_on_telemetry(&g_c, &m, 8000);
    CHECK(g_c.active.state == CMD_APPLIED);
    CHECK(g_c.audit.used == 1);
    CHECK(g_c.audit.entries[0].state == CMD_APPLIED);
    ctrl_acknowledge(&g_c);
    CHECK(g_c.active.state == CMD_IDLE);
    /* cooldown measured from submission */
    CHECK(ctrl_propose(&g_c, p, im_val(80), 70, true, 30000) == CTRL_ERR_COOLDOWN);
    CHECK(ctrl_propose(&g_c, p, im_val(80), 70, true, 62001) == CTRL_OK);
    ctrl_test_force_verified(false);
}

static void test_readback_disagrees(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    ctrl_propose(&g_c, CTRL_PARAM_TOU_SLOT1_SOC, im_val(99), 80, true, 0);
    ctrl_confirm(&g_c, im_val(99), true, 0);
    ctrl_on_accepted(&g_c, g_c.active.id, 10);
    inverter_model_t m;
    im_reset(&m);
    m.tou.slots[0].soc_pct = im_val(99);
    ctrl_on_telemetry(&g_c, &m, 31000);
    CHECK(g_c.active.state == CMD_FAILED);
    ctrl_test_force_verified(false);
}

static void test_timeout_is_uncertain_and_never_replayed(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    ctrl_propose(&g_c, CTRL_PARAM_TOU_SLOT1_SOC, im_val(99), 80, true, 0);
    ctrl_confirm(&g_c, im_val(99), true, 0);
    const uint32_t id = g_c.active.id;
    ctrl_on_transport_error(&g_c, id, 5000); /* e.g. Wi-Fi drop mid-write */
    CHECK(g_c.active.state == CMD_UNCERTAIN);
    /* Nothing new can be proposed until the user acknowledges the outcome. */
    CHECK(ctrl_propose(&g_c, CTRL_PARAM_TOU_ENABLE, im_val(1), 0, true, 100000) == CTRL_ERR_BUSY);
    /* Stale outcomes for the same id are ignored */
    ctrl_on_accepted(&g_c, id, 6000);
    CHECK(g_c.active.state == CMD_UNCERTAIN);
    ctrl_acknowledge(&g_c);
    CHECK(g_c.active.state == CMD_IDLE);
    ctrl_test_force_verified(false);
}

static void test_confirm_revalidates(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    ctrl_propose(&g_c, CTRL_PARAM_TOU_SLOT1_SOC, im_val(99), 80, true, 0);
    /* link dropped while the dialog was open */
    CHECK(ctrl_confirm(&g_c, im_val(99), false, 1000) == CTRL_ERR_LINK);
    CHECK(g_c.active.state == CMD_CANCELLED);
    ctrl_acknowledge(&g_c);
    /* value changed elsewhere to the requested one => unchanged */
    ctrl_propose(&g_c, CTRL_PARAM_TOU_SLOT1_SOC, im_val(99), 80, true, 2000);
    CHECK(ctrl_confirm(&g_c, im_val(80), true, 3000) == CTRL_ERR_UNCHANGED);
    ctrl_test_force_verified(false);
}

static void test_cancel_and_mismatched_ids(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    ctrl_propose(&g_c, CTRL_PARAM_TOU_ENABLE, im_val(1), 0, true, 0);
    ctrl_cancel(&g_c, 10);
    CHECK(g_c.active.state == CMD_CANCELLED);
    ctrl_acknowledge(&g_c);
    ctrl_propose(&g_c, CTRL_PARAM_TOU_ENABLE, im_val(1), 0, true, 20);
    ctrl_confirm(&g_c, im_val(1), true, 30);
    ctrl_on_rejected(&g_c, g_c.active.id + 99, 40);
    CHECK(g_c.active.state == CMD_SUBMITTED);
    ctrl_on_rejected(&g_c, g_c.active.id, 50);
    CHECK(g_c.active.state == CMD_FAILED);
    ctrl_test_force_verified(false);
}

static void test_audit_ring_bounded(void)
{
    ctrl_test_force_verified(true);
    init_with_caps(true);
    for (int i = 0; i < CTRL_AUDIT_LEN + 5; i++) {
        ctrl_propose(&g_c, CTRL_PARAM_TOU_ENABLE, im_val(1), 0, true, i);
        ctrl_cancel(&g_c, i);
        ctrl_acknowledge(&g_c);
    }
    CHECK(g_c.audit.used == CTRL_AUDIT_LEN);
    ctrl_test_force_verified(false);
}

void suite_control(void)
{
    RUN(test_shipped_defaults_block_all_writes);
    RUN(test_current_value_mapping);
    RUN(test_validation_rules);
    RUN(test_happy_path_requires_readback);
    RUN(test_readback_disagrees);
    RUN(test_timeout_is_uncertain_and_never_replayed);
    RUN(test_confirm_revalidates);
    RUN(test_cancel_and_mismatched_ids);
    RUN(test_audit_ring_bounded);
}
