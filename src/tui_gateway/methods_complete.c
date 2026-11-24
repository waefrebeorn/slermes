/*
 * methods_complete.c - C home for upstream Hermes tui_gateway/methods_complete.py.
 * PoP: w16_methodscom_d1b360_item @ tui_gateway/methods_complete.py:_item
 * PoP: w16_methodscom_d1b360_at_hints @ tui_gateway/methods_complete.py:_AT_DIRECTIVE_HINTS
 * PoP: w16_methodscom_d1b360_builtin_prefixes @ tui_gateway/methods_complete.py:_BUILTIN_AT_PREFIXES
 *
 * Faithful ports of the module's pure completion-item builder and its two static
 * completion tables. The registered RPC handlers own live server/DB state and
 * stay with the live Python owner.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w16_methodscom_d1b360.h"

static json_t *jcopy(const json_t *v) { return v ? json_copy(v) : json_null(); }

struct json_t *w16_methodscom_d1b360_item(const struct json_t *text,
        const struct json_t *meta, const struct json_t *display) {
    json_t *out = json_object();
    json_set(out, "text", jcopy(text));
    /* display if display is not None else text (an explicit empty string stays empty) */
    json_set(out, "display",
             (display && display->type != JSON_NULL) ? json_copy(display) : jcopy(text));
    json_set(out, "meta", jcopy(meta));
    return out;
}

struct json_t *w16_methodscom_d1b360_at_hints(void) {
    static const char *const hints[6][2] = {
        {"@diff", "git diff"},   {"@staged", "staged diff"},
        {"@file:", "attach file"}, {"@folder:", "attach folder"},
        {"@url:", "fetch url"},  {"@git:", "git log"}
    };
    json_t *out = json_array();
    for (int i = 0; i < 6; i++) {
        json_t *row = json_array();
        json_append(row, json_string(hints[i][0]));
        json_append(row, json_string(hints[i][1]));
        json_append(out, row);
    }
    return out;
}

struct json_t *w16_methodscom_d1b360_builtin_prefixes(void) {
    /* sorted(_BUILTIN_AT_PREFIXES) */
    static const char *const pfx[] = {"diff", "file", "folder", "git", "staged", "url"};
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(pfx) / sizeof(pfx[0]); i++)
        json_append(out, json_string(pfx[i]));
    return out;
}
