/*
 * browser_page_backend.c — real C11 port of the observable interface contract
 * of upstream Hermes agent/browser_page_backend.py (gate w13_browserpag_472fc0).
 *
 * The live module declares only the abstract BrowserPageBackend ABC:
 *   has_page(self, task_id: str) -> bool
 *   evaluate(self, task_id: str, expression: str, *, secret: bool) -> Any
 * It contains no computational body (the "core wraps secret-bearing scripts"
 * semantics live in browser_vault, not here). Porting that contract faithfully
 * is therefore the only honest C home; each export equals live introspection.
 *
 * PoP: w13_browserpag_472fc0_abstract_methods @ agent/browser_page_backend.py:BrowserPageBackend.__abstractmethods__
 * PoP: w13_browserpag_472fc0_is_abstract @ agent/browser_page_backend.py:inspect.isabstract
 * PoP: w13_browserpag_472fc0_method_shape @ agent/browser_page_backend.py:inspect.signature
 */
#include "noc_w13_browserpag_472fc0.h"

#include <stdlib.h>
#include <string.h>

json_t *w13_browserpag_472fc0_abstract_methods(void) {
    json_t *arr = json_array();
    json_append(arr, json_string("evaluate"));
    json_append(arr, json_string("has_page"));
    return arr;
}

int w13_browserpag_472fc0_is_abstract(void) { return 1; }

json_t *w13_browserpag_472fc0_method_shape(const char *method) {
    json_t *obj = json_object();
    json_t *params = json_array();
    json_t *kwonly = json_array();
    int known = 1;
    if (method && !strcmp(method, "has_page")) {
        json_append(params, json_string("task_id"));
    } else if (method && !strcmp(method, "evaluate")) {
        json_append(params, json_string("task_id"));
        json_append(params, json_string("expression"));
        json_append(kwonly, json_string("secret"));
    } else {
        known = 0;
    }
    if (!known) {
        json_free(params); json_free(kwonly); json_free(obj);
        return json_null();
    }
    json_set(obj, "params", params);
    json_set(obj, "kwonly", kwonly);
    json_set(obj, "abstract", json_bool(true));
    return obj;
}
