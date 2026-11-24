/*
 * ui_preferences.c -- real C11 port of the request-model validation and the
 * HTTP status mapping of upstream Hermes
 *   hermes_cli/web_routers/ui_preferences.py.
 *
 * PoP: w22_uipreferen_dd30c7_model @ hermes_cli/web_routers/ui_preferences.py:PreferencesPatch
 * PoP: w22_uipreferen_dd30c7_routes @ hermes_cli/web_routers/ui_preferences.py:router
 * PoP: w22_uipreferen_dd30c7_get @ hermes_cli/web_routers/ui_preferences.py:get_ui_preferences
 * PoP: w22_uipreferen_dd30c7_patch @ hermes_cli/web_routers/ui_preferences.py:patch_ui_preferences
 *
 * The preferences store itself -- `ui_preferences.read_preferences()` /
 * `write_preferences()`, the document, the busy lock and its file -- is the
 * seam the oracle stubs. This file holds what the route decides around it.
 */
#include "hermes_json.h"
#include "noc_w22_uipreferen_dd30c7.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- pydantic v2 lax bool ------------------------------------------------- */

/* The tokens pydantic's lax bool accepts, plus the two floats it accepts. */
static int ui6_bool_token(const char *s, int *out) {
    char low[16];
    size_t i, n;
    static const char *const TRUE_TOKENS[] = {"1", "on", "t", "true", "y", "yes", NULL};
    static const char *const FALSE_TOKENS[] = {"0", "off", "f", "false", "n", "no", NULL};
    if (!s) return 0;
    n = strlen(s);
    if (n == 0 || n >= sizeof(low)) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 128) return 0;
        low[i] = (char)tolower(c);
    }
    low[n] = '\0';
    for (i = 0; TRUE_TOKENS[i]; i++) {
        if (!strcmp(low, TRUE_TOKENS[i])) { *out = 1; return 1; }
    }
    for (i = 0; FALSE_TOKENS[i]; i++) {
        if (!strcmp(low, FALSE_TOKENS[i])) { *out = 0; return 1; }
    }
    return 0;
}

/* Returns 1 and writes *out when `v` is a valid bool field, 0 when invalid. */
static int ui6_lax_bool(const json_t *v, int *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:
        *out = v->bool_val ? 1 : 0;
        return 1;
    case JSON_NUMBER:
        if (v->num_val == 1.0) { *out = 1; return 1; }
        if (v->num_val == 0.0) { *out = 0; return 1; }
        return 0;
    case JSON_STRING:
        return ui6_bool_token(v->str_val, out);
    default:
        return 0;
    }
}

/* --- PreferencesPatch ----------------------------------------------------- */

static json_t *ui6_invalid(void) {
    json_t *out = json_object();
    json_set(out, "valid", json_bool(0));
    json_set(out, "values", json_null());
    json_set(out, "seed", json_null());
    return out;
}

json_t *w22_uipreferen_dd30c7_model(const json_t *body) {
    json_t *values, *out;
    int seed = 0;
    size_t i;

    if (!body || body->type != JSON_OBJECT) return ui6_invalid();

    values = json_obj_get(body, "values");
    if (!values) {
        /* Field(default_factory=dict) */
        values = json_object();
    } else if (values->type != JSON_OBJECT) {
        return ui6_invalid();
    } else {
        for (i = 0; i < values->c.count; i++) {
            json_t *v = values->c.items[i];
            /* dict[str, Optional[str]]: ints/bools/lists are errors, not str(). */
            if (!v || (v->type != JSON_STRING && v->type != JSON_NULL)) return ui6_invalid();
        }
        values = json_copy(values);
    }

    {
        json_t *seed_node = json_obj_get(body, "seed");
        if (seed_node && !ui6_lax_bool(seed_node, &seed)) {
            json_free(values);
            return ui6_invalid();
        }
    }

    out = json_object();
    json_set(out, "valid", json_bool(1));
    json_set(out, "values", values);
    json_set(out, "seed", json_bool(seed));
    return out;
}

/* --- routes --------------------------------------------------------------- */

json_t *w22_uipreferen_dd30c7_routes(void) {
    static const char *const METHODS[] = {"GET", "PATCH"};
    const char *path = "/api/ui-preferences";
    json_t *arr = json_array();
    int i;
    for (i = 0; i < 2; i++) {
        json_t *row = json_object();
        json_set(row, "method", json_string(METHODS[i]));
        json_set(row, "path", json_string(path));
        json_append(arr, row);
    }
    return arr;
}

/* --- the two handlers ----------------------------------------------------- */

static json_t *ui6_reply(int status, json_t *body, const char *detail) {
    json_t *out = json_object();
    json_set(out, "status", json_number((double)status));
    json_set(out, "body", body ? body : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

json_t *w22_uipreferen_dd30c7_get(const json_t *doc, const char *error_kind,
                                  const char *error_msg) {
    const char *kind = error_kind ? error_kind : "";
    if (!strcmp(kind, "busy")) {
        /* except ui_preferences.PreferencesBusy -> 503 */
        return ui6_reply(503, NULL, error_msg ? error_msg : "");
    }
    if (!strcmp(kind, "error")) {
        /* PreferencesError is NOT caught here: FastAPI's default 500. */
        return ui6_reply(500, NULL, NULL);
    }
    return ui6_reply(200, doc ? json_copy(doc) : json_null(), NULL);
}

json_t *w22_uipreferen_dd30c7_patch(const json_t *body, const char *error_kind,
                                    const char *error_msg) {
    const char *kind = error_kind ? error_kind : "";
    json_t *model = w22_uipreferen_dd30c7_model(body);
    json_t *echo, *wrote;

    if (!json_get_bool(model, "valid", 0)) {
        /* FastAPI request validation: the handler never runs. */
        json_free(model);
        return ui6_reply(422, NULL, NULL);
    }
    if (!strcmp(kind, "busy")) {
        json_free(model);
        return ui6_reply(503, NULL, error_msg ? error_msg : "");
    }
    if (!strcmp(kind, "error")) {
        json_free(model);
        return ui6_reply(400, NULL, error_msg ? error_msg : "");
    }
    wrote = json_object();
    echo = json_obj_get(model, "values");
    json_set(wrote, "wrote_values", echo ? json_copy(echo) : json_object());
    json_set(wrote, "wrote_seed", json_bool(json_get_bool(model, "seed", 0)));
    json_free(model);
    return ui6_reply(200, wrote, NULL);
}
