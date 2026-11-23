/*
 * approval_yolo.c -- faithful C11 port of upstream Hermes tools/approval_yolo.py.
 *
 * PoP: w7_approvalyol_80b11c_with_session_yolo    @ tools/approval_yolo.py:with_session_yolo
 * PoP: w7_approvalyol_80b11c_toggle_session_yolo  @ tools/approval_yolo.py:toggle_session_yolo
 * PoP: w7_approvalyol_80b11c_restore_session_yolo @ tools/approval_yolo.py:restore_session_yolo
 * PoP: w7_approvalyol_80b11c_apply_launch_yolo    @ tools/approval_yolo.py:apply_launch_yolo
 * PoP: w7_approvalyol_80b11c_transfer_session_yolo @ tools/approval_yolo.py:transfer_session_yolo
 *
 * Upstream's bypass storage lives in tools.approval (an in-memory per-key
 * set) and persistence is an injected callback. Only the decision logic is
 * here; the store's current state is supplied as booleans by the caller, which
 * keeps the port pure and deterministic.
 */
#include "noc_w7_approvalyol_80b11c.h"

json_t *w7_approvalyol_80b11c_with_session_yolo(const json_t *model_config, int enabled) {
    if (!enabled) {
        return model_config ? json_copy(model_config) : json_null();
    }
    json_t *out = json_object();
    if (model_config && model_config->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(model_config); i++) {
            const char *k = json_object_get_key_at(model_config, i);
            json_t *v = json_object_get_at(model_config, i);
            if (k) json_set(out, k, json_copy(v));
        }
    }
    json_set(out, "yolo_mode", json_bool(1));
    return out;
}

int w7_approvalyol_80b11c_toggle_session_yolo(int has_enabled, int enabled_given,
                                              int persisted, int current_enabled) {
    int enabled = has_enabled ? enabled_given
                              : !(persisted || current_enabled);
    return enabled ? 1 : 0;
}

int w7_approvalyol_80b11c_restore_session_yolo(int key_empty, int persisted,
                                               int frozen, int current_enabled) {
    if (key_empty || !persisted || frozen || current_enabled) return 0;
    return 1;
}

int w7_approvalyol_80b11c_apply_launch_yolo(int key_empty, int frozen, int already_applied) {
    if (key_empty || frozen || already_applied) return 0;
    return 1;
}

int w7_approvalyol_80b11c_transfer_session_yolo(int old_empty, int new_empty,
                                                int same_key, int old_enabled) {
    if (old_empty || new_empty || same_key || !old_enabled) return 0;
    return 1;
}
