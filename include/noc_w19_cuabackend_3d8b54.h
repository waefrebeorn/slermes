/* noc_w19_cuabackend_3d8b54.h - public C API for tools/computer_use/cua_backend_input.py port.
 * The cua-driver transport (_action/_session probes) is injected: each entry point takes one JSON
 * request describing the caller args and the live session's capabilities, and answers the
 * ActionResult verdict as JSON. */
#ifndef NOC_W19_CUABACKEND_3D8B54_H
#define NOC_W19_CUABACKEND_3D8B54_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_cuabackend_3d8b54_refuse @ tools/computer_use/cua_backend_input.py:_refuse
 * ActionResult(ok=False, action, message, **fields); a NULL code / delivery_mode is omitted. */
json_t *w19_cuabackend_3d8b54_refuse(const char *action, const char *message,
                                     const char *code, const char *delivery_mode);

/* PoP: w19_cuabackend_3d8b54_target_args @ tools/computer_use/cua_backend_input.py:_InputMixin._target_args
 * (refusal, base args) for an input action against the sticky target. */
json_t *w19_cuabackend_3d8b54_target_args(const char *action, const json_t *pid,
                                          const json_t *window_id, int need_window);

/* PoP: w19_cuabackend_3d8b54_apply_delivery @ tools/computer_use/cua_backend_input.py:_InputMixin._apply_delivery
 * {"refusal": null|verdict, "args": {...}} - background needs no flag, foreground only on a
 * schema that advertises it, anything else is a bad_delivery_mode refusal. */
json_t *w19_cuabackend_3d8b54_apply_delivery(const char *action, const char *delivery_mode,
                                             int supports_delivery_mode);

/* PoP: w19_cuabackend_3d8b54_parse_key_combo @ tools/computer_use/cua_backend_parse.py:_parse_key_combo
 * "cmd+s" / "ctrl-alt-t" -> {"key": <last non-modifier or null>, "modifiers": [...]}. */
json_t *w19_cuabackend_3d8b54_parse_key_combo(const char *keys);

/* PoP: w19_cuabackend_3d8b54_run_input_action @ tools/computer_use/cua_backend_input.py:_InputMixin._run_input_action
 * One delivery rung: delivery_mode refusal, the bring_to_front ladder, then the action. */
json_t *w19_cuabackend_3d8b54_run_input_action(const json_t *req);

/* PoP: w19_cuabackend_3d8b54_click @ tools/computer_use/cua_backend_input.py:_InputMixin.click */
json_t *w19_cuabackend_3d8b54_click(const json_t *req);
/* PoP: w19_cuabackend_3d8b54_drag @ tools/computer_use/cua_backend_input.py:_InputMixin.drag */
json_t *w19_cuabackend_3d8b54_drag(const json_t *req);
/* PoP: w19_cuabackend_3d8b54_scroll @ tools/computer_use/cua_backend_input.py:_InputMixin.scroll */
json_t *w19_cuabackend_3d8b54_scroll(const json_t *req);
/* PoP: w19_cuabackend_3d8b54_type_text @ tools/computer_use/cua_backend_input.py:_InputMixin.type_text */
json_t *w19_cuabackend_3d8b54_type_text(const json_t *req);
/* PoP: w19_cuabackend_3d8b54_key @ tools/computer_use/cua_backend_input.py:_InputMixin.key */
json_t *w19_cuabackend_3d8b54_key(const json_t *req);
/* PoP: w19_cuabackend_3d8b54_set_value @ tools/computer_use/cua_backend_input.py:_InputMixin.set_value */
json_t *w19_cuabackend_3d8b54_set_value(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CUABACKEND_3D8B54_H */
