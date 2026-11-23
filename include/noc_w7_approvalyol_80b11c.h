#ifndef NOC_W7_APPROVALYOL_80B11C_H
#define NOC_W7_APPROVALYOL_80B11C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure decision logic of tools/approval_yolo.py. The in-memory approval set
 * (tools.approval) is supplied by the caller as injected booleans; every
 * exported symbol is prefixed with the gate id. */

/* PoP: w7_approvalyol_80b11c_with_session_yolo @ tools/approval_yolo.py:with_session_yolo
 * Mirrors `{**(model_config or {}), "yolo_mode": True}` when enabled, else
 * model_config unchanged. Caller owns the returned node. */
json_t *w7_approvalyol_80b11c_with_session_yolo(const json_t *model_config, int enabled);

/* PoP: w7_approvalyol_80b11c_toggle_session_yolo @ tools/approval_yolo.py:toggle_session_yolo
 * has_enabled says whether the `enabled` argument was supplied; persisted and
 * current_enabled feed the None-coalescing branch. Returns the new state. */
int w7_approvalyol_80b11c_toggle_session_yolo(int has_enabled, int enabled_given,
                                              int persisted, int current_enabled);

/* PoP: w7_approvalyol_80b11c_restore_session_yolo @ tools/approval_yolo.py:restore_session_yolo
 * True when this call would re-arm the persisted bypass. */
int w7_approvalyol_80b11c_restore_session_yolo(int key_empty, int persisted,
                                               int frozen, int current_enabled);

/* PoP: w7_approvalyol_80b11c_apply_launch_yolo @ tools/approval_yolo.py:apply_launch_yolo
 * True when this call would seed (arm) the launch --yolo for the session. */
int w7_approvalyol_80b11c_apply_launch_yolo(int key_empty, int frozen, int already_applied);

/* PoP: w7_approvalyol_80b11c_transfer_session_yolo @ tools/approval_yolo.py:transfer_session_yolo
 * True when the live bypass would move from old_key to new_key. */
int w7_approvalyol_80b11c_transfer_session_yolo(int old_empty, int new_empty,
                                                int same_key, int old_enabled);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_APPROVALYOL_80B11C_H */
