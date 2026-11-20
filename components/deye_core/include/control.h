/*
 * Inverter control safety layer.
 *
 * This module decides whether a write may be proposed, tracks each command
 * through an explicit lifecycle and keeps an audit trail. It performs no I/O:
 * the transport submits commands and reports outcomes back here.
 *
 * Safety rules enforced here (see docs/control-safety.md):
 *  - a parameter is writable only if it is individually verified AND the
 *    backend reports write capability for it;
 *  - values are range/step checked; unchanged values are rejected;
 *  - writes require fresh telemetry and are rate limited per parameter;
 *  - only one command may be in flight at a time;
 *  - a transport timeout yields UNCERTAIN, never a retry;
 *  - APPLIED is reported only after read-back confirms the value.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "inverter_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTRL_PARAM_TOU_ENABLE = 0,
    CTRL_PARAM_TOU_SLOT1_SOC,
    CTRL_PARAM_TOU_SLOT1_POWER,
    CTRL_PARAM_TOU_SLOT1_GRID_CHARGE,
    CTRL_PARAM_COUNT,
} ctrl_param_id_t;

typedef enum {
    CTRL_CLASS_READ_ONLY = 0,
    CTRL_CLASS_PERSISTENT, /* stored by the inverter (assume NV memory) */
    CTRL_CLASS_TEMPORARY,
    CTRL_CLASS_SAFETY_CRITICAL,
} ctrl_class_t;

typedef struct {
    ctrl_param_id_t id;
    const char *key;   /* stable machine identifier for the backend */
    const char *label; /* UI label */
    const char *unit;
    float min;
    float max;
    float step;
    float tolerance; /* read-back match tolerance */
    ctrl_class_t cls;
    uint32_t cooldown_ms;
    /* True only after the register semantics were verified against the
     * actual inverter firmware AND authorised for writing. */
    bool verified;
} ctrl_param_desc_t;

typedef enum {
    CTRL_OK = 0,
    CTRL_ERR_UNKNOWN_PARAM,
    CTRL_ERR_READ_ONLY,
    CTRL_ERR_UNVERIFIED,
    CTRL_ERR_UNSUPPORTED, /* backend does not offer this write */
    CTRL_ERR_NO_CURRENT,  /* current value unknown => cannot compare */
    CTRL_ERR_UNCHANGED,
    CTRL_ERR_OUT_OF_RANGE,
    CTRL_ERR_STEP,
    CTRL_ERR_LINK, /* telemetry not fresh / backend offline */
    CTRL_ERR_COOLDOWN,
    CTRL_ERR_BUSY, /* another command is in flight */
    CTRL_ERR_NOT_CONFIRMED,
    CTRL_ERR_BAD_STATE,
} ctrl_result_t;

typedef enum {
    CMD_IDLE = 0,
    CMD_AWAITING_CONFIRM,
    CMD_SUBMITTED,
    CMD_ACCEPTED,  /* backend accepted; waiting for read-back */
    CMD_APPLIED,   /* read-back confirmed */
    CMD_FAILED,    /* definite failure: rejected or read-back disagrees */
    CMD_UNCERTAIN, /* outcome unknown: timeout / connection loss */
    CMD_CANCELLED,
} ctrl_cmd_state_t;

typedef struct {
    uint32_t id; /* correlation id, unique per boot */
    ctrl_param_id_t param;
    float requested;
    float previous;
    ctrl_cmd_state_t state;
    int64_t created_ms;
    int64_t submitted_ms;
    int64_t finished_ms;
    ctrl_result_t reason;
} ctrl_command_t;

#define CTRL_AUDIT_LEN 32

typedef struct {
    ctrl_command_t entries[CTRL_AUDIT_LEN];
    uint16_t head;
    uint16_t used;
} ctrl_audit_t;

typedef struct {
    bool backend_write_api; /* backend exposes an authenticated write API */
    bool param_supported[CTRL_PARAM_COUNT];
} ctrl_capabilities_t;

typedef struct {
    ctrl_capabilities_t caps;
    ctrl_command_t active;
    int64_t last_write_ms[CTRL_PARAM_COUNT];
    bool has_written[CTRL_PARAM_COUNT];
    uint32_t next_id;
    uint32_t readback_timeout_ms;
    ctrl_audit_t audit;
} ctrl_ctx_t;

const ctrl_param_desc_t *ctrl_param(ctrl_param_id_t id);

/* Reads the current value of a parameter from the telemetry model. */
im_value_t ctrl_current_value(const inverter_model_t *m, ctrl_param_id_t id);

void ctrl_init(ctrl_ctx_t *c, const ctrl_capabilities_t *caps);

/* Whether the parameter can be edited at all (drives UI enable state). */
ctrl_result_t ctrl_can_edit(const ctrl_ctx_t *c, ctrl_param_id_t id);

/* Full validation of a proposed value. Does not change state. */
ctrl_result_t ctrl_validate(const ctrl_ctx_t *c, ctrl_param_id_t id, im_value_t current, float proposed, bool link_ok,
                            int64_t now_ms);

/* Creates a command awaiting user confirmation. */
ctrl_result_t ctrl_propose(ctrl_ctx_t *c, ctrl_param_id_t id, im_value_t current, float proposed, bool link_ok,
                           int64_t now_ms);

void ctrl_cancel(ctrl_ctx_t *c, int64_t now_ms);

/* User confirmed: re-validates against the latest state and, if still
 * valid, moves to SUBMITTED. The caller then hands it to the transport. */
ctrl_result_t ctrl_confirm(ctrl_ctx_t *c, im_value_t current, bool link_ok, int64_t now_ms);

/* Transport outcomes. */
void ctrl_on_accepted(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms);
void ctrl_on_rejected(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms);
void ctrl_on_transport_error(ctrl_ctx_t *c, uint32_t cmd_id, int64_t now_ms);

/* Feed every fresh telemetry update; resolves ACCEPTED commands. */
void ctrl_on_telemetry(ctrl_ctx_t *c, const inverter_model_t *m, int64_t now_ms);

/* Explicitly drops a finished command so a new one can be proposed.
 * UNCERTAIN commands are never replayed automatically. */
void ctrl_acknowledge(ctrl_ctx_t *c);

const char *ctrl_result_text(ctrl_result_t r);
const char *ctrl_state_text(ctrl_cmd_state_t s);
const char *ctrl_class_text(ctrl_class_t cls);

#ifdef __cplusplus
}
#endif
