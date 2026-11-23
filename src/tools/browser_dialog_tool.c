/*
 * browser_dialog_tool.c — C home for upstream Hermes tools/browser_dialog_tool.py.
 * PoP: w12_browserdia_3b0b37_browser_dialog @ tools/browser_dialog_tool.py:browser_dialog
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w12_browserdia_3b0b37.h"

static int w12_py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

char *w12_browserdia_3b0b37_browser_dialog(const char *action,
                                           const char *prompt_text,
                                           const char *dialog_id,
                                           const char *task_id,
                                           const json_t *supervisor,
                                           const json_t *respond_result) {
    json_t *out = json_object();
    char *serialized;
    (void)prompt_text; (void)dialog_id; (void)task_id; /* live args; effect is in the injected result */

    if (!w12_py_truthy(supervisor)) {
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string(
            "No CDP supervisor is attached to this task: no browser session has been started yet. "
            "Call browser_exec or /browser connect first."));
    } else if (w12_py_truthy(respond_result ? json_obj_get(respond_result, "ok") : NULL)) {
        json_t *dialog = respond_result ? json_obj_get(respond_result, "dialog") : NULL;
        json_set(out, "success", json_bool(true));
        json_set(out, "action", json_string(action ? action : ""));
        /* result.get("dialog", {}) -- a present null stays null. */
        json_set(out, "dialog", dialog ? json_copy(dialog) : json_object());
    } else {
        json_t *err = respond_result ? json_obj_get(respond_result, "error") : NULL;
        json_set(out, "success", json_bool(false));
        if (!err) json_set(out, "error", json_string("unknown error"));
        else json_set(out, "error", json_copy(err));
    }
    serialized = json_dumps(out, 0);
    json_free(out);
    return serialized;
}
