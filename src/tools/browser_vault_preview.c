/*
 * browser_vault_preview.c - C home for upstream Hermes tools/browser_vault_preview.py.
 * PoP: w15_browservau_9b911b_request @ tools/browser_vault_preview.py:_request
 * PoP: w15_browservau_9b911b_unavailable @ tools/browser_vault_preview.py:_unavailable
 *
 * The renderer callback's raw answer is injected as JSON; the only work here is the
 * string-or-dict parse, the strict `answer.get("success") is True` gate, and the
 * fixed unavailable envelope. IPC, ContextVar pinning and redaction stay in Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_browservau_9b911b.h"

static const char *const UNAVAILABLE =
    "The in-app browser page could not complete the secure vault operation. Keep this chat and its "
    "login page open in an updated Hermes Desktop, then retry.";

static json_t *unavailable_obj(void) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "error_type", json_string("preview_unavailable"));
    json_set(r, "error", json_string(UNAVAILABLE));
    return r;
}

struct json_t *w15_browservau_9b911b_unavailable(void) { return unavailable_obj(); }

struct json_t *w15_browservau_9b911b_request(const struct json_t *raw) {
    json_t *answer = NULL;
    if (raw && raw->type == JSON_STRING) {
        /* Mirror `json.loads(raw) if isinstance(raw, str) else raw`. */
        answer = json_parse(raw->str_val ? raw->str_val : "", NULL);
        if (!answer) return unavailable_obj();
    } else {
        answer = raw ? json_copy(raw) : json_null();
    }
    if (answer && answer->type == JSON_OBJECT) {
        json_t *ok = json_obj_get(answer, "success");
        /* Python `is True`: only a real bool True passes (not 1, not "true"). */
        if (ok && ok->type == JSON_BOOL && ok->bool_val) return answer;
    }
    json_free(answer);
    return unavailable_obj();
}

