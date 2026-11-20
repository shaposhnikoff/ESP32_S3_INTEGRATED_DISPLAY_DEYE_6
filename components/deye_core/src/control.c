#include "control.h"

#include <math.h>
#include <string.h>

/*
 * Candidate parameters. All are visible in telemetry but none is verified:
 * the backend exposes no write API and the persistence behaviour of the
 * underlying registers is unknown. Flip `verified` only after the procedure
 * in docs/writable-parameters.md has been completed for that parameter.
 */
static const ctrl_param_desc_t k_params[CTRL_PARAM_COUNT] = {
    {CTRL_PARAM_TOU_ENABLE, "tou_enabled", "Time of Use", "", 0, 1, 1, 0.5f, CTRL_CLASS_PERSISTENT, 60000, false},
    {CTRL_PARAM_TOU_SLOT1_SOC, "tou_slot1_soc", "ToU slot 1 target SOC", "%", 10, 100, 1, 0.5f, CTRL_CLASS_PERSISTENT,
     60000, false},
    {CTRL_PARAM_TOU_SLOT1_POWER, "tou_slot1_power", "ToU slot 1 power", "W", 0, 6000, 100, 1.0f, CTRL_CLASS_PERSISTENT,
     60000, false},
    {CTRL_PARAM_TOU_SLOT1_GRID_CHARGE, "tou_slot1_grid_charge", "ToU slot 1 grid charging", "", 0, 1, 1, 0.5f,
     CTRL_CLASS_SAFETY_CRITICAL, 300000, false},
};

#ifdef DEYE_CORE_TESTING
/* Host-test hook: lets lifecycle tests run against "verified" copies of the
 * descriptors. Never compiled into firmware. */
static ctrl_param_desc_t s_test_params[CTRL_PARAM_COUNT];
static bool s_test_verified;

void ctrl_test_force_verified(bool on);
void ctrl_test_force_verified(bool on)
{
    s_test_verified = on;
    for (int i = 0; i < CTRL_PARAM_COUNT; i++) {
        s_test_params[i] = k_params[i];
        s_test_params[i].verified = on;
    }
}
#endif

const ctrl_param_desc_t *ctrl_param(ctrl_param_id_t id)
{
    if ((int)id < 0 || id >= CTRL_PARAM_COUNT) {
        return NULL;
    }
#ifdef DEYE_CORE_TESTING
    if (s_test_verified) {
        return &s_test_params[id];
    }
#endif
    return &k_params[id];
}

im_value_t ctrl_current_value(const inverter_model_t *m, ctrl_param_id_t id)
{
    switch (id) {
    case CTRL_PARAM_TOU_ENABLE:
        return m->tou.enabled;
    case CTRL_PARAM_TOU_SLOT1_SOC:
        return m->tou.slots[0].soc_pct;
    case CTRL_PARAM_TOU_SLOT1_POWER:
        return m->tou.slots[0].power_w;
    case CTRL_PARAM_TOU_SLOT1_GRID_CHARGE:
        return m->tou.slots[0].grid_charge;
    default:
        return im_unknown();
    }
}

static void audit_push(ctrl_ctx_t *c, const ctrl_command_t *cmd)
{
    c->audit.entries[c->audit.head] = *cmd;
    c->audit.head = (uint16_t)((c->audit.head + 1) % CTRL_AUDIT_LEN);
    if (c->audit.used < CTRL_AUDIT_LEN) {
        c->audit.used++;
    }
}

void ctrl_init(ctrl_ctx_t *c, const ctrl_capabilities_t *caps)
{
    memset(c, 0, sizeof(*c));
    if (caps != NULL) {
        c->caps = *caps;
    }
    c->next_id = 1;
    c->readback_timeout_ms = 30000;
    c->active.state = CMD_IDLE;
}

ctrl_result_t ctrl_can_edit(const ctrl_ctx_t *c, ctrl_param_id_t id)
{
    const ctrl_param_desc_t *d = ctrl_param(id);
    if (d == NULL) {
        return CTRL_ERR_UNKNOWN_PARAM;
    }
    if (d->cls == CTRL_CLASS_READ_ONLY) {
        return CTRL_ERR_READ_ONLY;
    }
    if (!d->verified) {
        return CTRL_ERR_UNVERIFIED;
    }
    if (!c->caps.backend_write_api || !c->caps.param_supported[id]) {
        return CTRL_ERR_UNSUPPORTED;
    }
    return CTRL_OK;
}

static bool is_busy(const ctrl_ctx_t *c)
{
    switch (c->active.state) {
    case CMD_AWAITING_CONFIRM:
    case CMD_SUBMITTED:
    case CMD_ACCEPTED:
    case CMD_UNCERTAIN: /* must be acknowledged by the user first */
        return true;
    default:
        return false;
    }
}

ctrl_result_t ctrl_validate(const ctrl_ctx_t *c, ctrl_param_id_t id, im_value_t current, float proposed, bool link_ok,
                            int64_t now_ms)
{
    ctrl_result_t r = ctrl_can_edit(c, id);
    if (r != CTRL_OK) {
        return r;
    }
    const ctrl_param_desc_t *d = ctrl_param(id);
    if (!link_ok) {
        return CTRL_ERR_LINK;
    }
    if (!current.valid) {
        return CTRL_ERR_NO_CURRENT;
    }
    if (isnan(proposed) || proposed < d->min || proposed > d->max) {
        return CTRL_ERR_OUT_OF_RANGE;
    }
    if (d->step > 0) {
        const float k = (proposed - d->min) / d->step;
        if (fabsf(k - roundf(k)) > 1e-3f) {
            return CTRL_ERR_STEP;
        }
    }
    if (fabsf(proposed - current.value) <= d->tolerance) {
        return CTRL_ERR_UNCHANGED;
    }
    if (c->has_written[id] && now_ms - c->last_write_ms[id] < (int64_t)d->cooldown_ms) {
        return CTRL_ERR_COOLDOWN;
    }
    return CTRL_OK;
}

ctrl_result_t ctrl_propose(ctrl_ctx_t *c, ctrl_param_id_t id, im_value_t current, float proposed, bool link_ok,
                           int64_t now_ms)
{
    if (is_busy(c)) {
        return CTRL_ERR_BUSY;
    }
    ctrl_result_t r = ctrl_validate(c, id, current, proposed, link_ok, now_ms);
    if (r != CTRL_OK) {
        return r;
    }
    memset(&c->active, 0, sizeof(c->active));
    c->active.id = c->next_id++;
    c->active.param = id;
    c->active.requested = proposed;
    c->active.previous = current.value;
    c->active.state = CMD_AWAITING_CONFIRM;
    c->active.created_ms = now_ms;
    return CTRL_OK;
}

static void finish(ctrl_ctx_t *c, ctrl_cmd_state_t st, ctrl_result_t reason, int64_t now_ms)
{
    c->active.state = st;
    c->active.reason = reason;
    c->active.finished_ms = now_ms;
    audit_push(c, &c->active);
}

void ctrl_cancel(ctrl_ctx_t *c, int64_t now_ms)
{
    if (c->active.state == CMD_AWAITING_CONFIRM) {
        finish(c, CMD_CANCELLED, CTRL_OK, now_ms);
    }
}

ctrl_result_t ctrl_confirm(ctrl_ctx_t *c, im_value_t current, bool link_ok, int64_t now_ms)
{
    if (c->active.state != CMD_AWAITING_CONFIRM) {
        return CTRL_ERR_NOT_CONFIRMED;
    }
    ctrl_result_t r = ctrl_validate(c, c->active.param, current, c->active.requested, link_ok, now_ms);
    if (r != CTRL_OK) {
        finish(c, CMD_CANCELLED, r, now_ms);
        return r;
    }
    c->active.state = CMD_SUBMITTED;
    c->active.submitted_ms = now_ms;
    /* Rate limit counts from submission: an ambiguous attempt still counts. */
    c->last_write_ms[c->active.param] = now_ms;
    c->has_written[c->active.param] = true;
    return CTRL_OK;
}

static bool is_mine(const ctrl_ctx_t *c, uint32_t cmd_id)
{
    return c->active.id == cmd_id;
}

void ctrl_on_accepted(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms)
{
    (void)now_ms;
    if (is_mine(c, cmd_id) && c->active.state == CMD_SUBMITTED) {
        c->active.state = CMD_ACCEPTED;
    }
}

void ctrl_on_rejected(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms)
{
    if (is_mine(c, cmd_id) && (c->active.state == CMD_SUBMITTED || c->active.state == CMD_ACCEPTED)) {
        finish(c, CMD_FAILED, CTRL_OK, now_ms);
    }
}

void ctrl_on_transport_error(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms)
{
    if (is_mine(c, cmd_id) && (c->active.state == CMD_SUBMITTED || c->active.state == CMD_ACCEPTED)) {
        finish(c, CMD_UNCERTAIN, CTRL_ERR_LINK, now_ms);
    }
}

void ctrl_on_telemetry(ctrl_ctx_t *c, const inverter_model_t *m, int64_t now_ms)
{
    if (c->active.state != CMD_ACCEPTED) {
        return;
    }
    const ctrl_param_desc_t *d = ctrl_param(c->active.param);
    im_value_t cur = ctrl_current_value(m, c->active.param);
    if (cur.valid && fabsf(cur.value - c->active.requested) <= d->tolerance) {
        finish(c, CMD_APPLIED, CTRL_OK, now_ms);
        return;
    }
    if (now_ms - c->active.submitted_ms > (int64_t)c->readback_timeout_ms) {
        /* Accepted but never observed: we do not know what the inverter did. */
        finish(c, cur.valid ? CMD_FAILED : CMD_UNCERTAIN, CTRL_OK, now_ms);
    }
}

void ctrl_acknowledge(ctrl_ctx_t *c)
{
    switch (c->active.state) {
    case CMD_APPLIED:
    case CMD_FAILED:
    case CMD_UNCERTAIN:
    case CMD_CANCELLED:
        c->active.state = CMD_IDLE;
        break;
    default:
        break;
    }
}

const char *ctrl_result_text(ctrl_result_t r)
{
    switch (r) {
    case CTRL_OK:
        return "OK";
    case CTRL_ERR_UNKNOWN_PARAM:
        return "Unknown parameter";
    case CTRL_ERR_READ_ONLY:
        return "Read-only";
    case CTRL_ERR_UNVERIFIED:
        return "Not verified for writing";
    case CTRL_ERR_UNSUPPORTED:
        return "Backend has no write API";
    case CTRL_ERR_NO_CURRENT:
        return "Current value unknown";
    case CTRL_ERR_UNCHANGED:
        return "Value unchanged";
    case CTRL_ERR_OUT_OF_RANGE:
        return "Out of range";
    case CTRL_ERR_STEP:
        return "Invalid step";
    case CTRL_ERR_LINK:
        return "Telemetry not live";
    case CTRL_ERR_COOLDOWN:
        return "Cooldown active";
    case CTRL_ERR_BUSY:
        return "Another command pending";
    case CTRL_ERR_NOT_CONFIRMED:
        return "Nothing to confirm";
    default:
        return "Invalid state";
    }
}

const char *ctrl_state_text(ctrl_cmd_state_t s)
{
    switch (s) {
    case CMD_IDLE:
        return "Idle";
    case CMD_AWAITING_CONFIRM:
        return "Awaiting confirmation";
    case CMD_SUBMITTED:
        return "Submitted";
    case CMD_ACCEPTED:
        return "Accepted, verifying";
    case CMD_APPLIED:
        return "Applied (verified)";
    case CMD_FAILED:
        return "Failed";
    case CMD_UNCERTAIN:
        return "Outcome uncertain";
    default:
        return "Cancelled";
    }
}

const char *ctrl_class_text(ctrl_class_t cls)
{
    switch (cls) {
    case CTRL_CLASS_READ_ONLY:
        return "Read-only";
    case CTRL_CLASS_PERSISTENT:
        return "Persistent (inverter NV memory assumed)";
    case CTRL_CLASS_TEMPORARY:
        return "Temporary";
    default:
        return "Safety-critical";
    }
}
