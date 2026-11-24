/*
 * slash_commands_goals.c — C port of upstream Hermes gateway/slash_commands_goals.py.
 *
 * PoP: w16_slashcomma_10adf9_mgr_call @ gateway/slash_commands_goals.py:_mgr_call
 * PoP: w16_slashcomma_10adf9_quiet_bool @ gateway/slash_commands_goals.py:_quiet_bool
 * PoP: w16_slashcomma_10adf9_loop_route @ gateway/slash_commands_goals.py:_handle_loop_command
 */
#include "noc_w16_slashcomma_10adf9.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static int py_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default: return 1;
    }
}

static char *py_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    return dupstr("[]");
}

static int py_truthy(const json_t *v) { return !py_falsy(v); }

json_t *w16_slashcomma_10adf9_mgr_call(const char *prefix, const json_t *result,
                     const char *exc_class, const char *exc_msg) {
    json_t *arr = json_array();
    int caught = exc_class && (!strcmp(exc_class, "RuntimeError") ||
                               !strcmp(exc_class, "ValueError"));
    if (caught) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s: %s", prefix ? prefix : "", exc_msg ? exc_msg : "");
        json_append(arr, json_null());
        json_append(arr, json_string(buf));
    } else {
        json_append(arr, result ? json_copy(result) : json_null());
        json_append(arr, json_null());
    }
    return arr;
}

int w16_slashcomma_10adf9_quiet_bool(const json_t *value, int raises) {
    return (raises) ? 0 : py_truthy(value);
}

json_t *w16_slashcomma_10adf9_loop_route(const json_t *src) {
    json_t *route = json_object();
    const json_t *platform = src ? json_obj_get(src, "platform") : NULL;
    const json_t *pv = NULL;
    char *owned = NULL;
    if (platform && platform->type == JSON_OBJECT &&
        (pv = json_obj_get(platform, "value")) != NULL) {
        /* hasattr(platform, "value") -> platform.value used verbatim.
         * Fixture domain keeps it a string (or null, which is filtered). */
        owned = (pv->type == JSON_STRING) ? dupstr(pv->str_val ? pv->str_val : "")
                                          : py_str_of(pv);
    } else {
        owned = py_str_of(platform && !py_falsy(platform) ? platform : NULL);
        /* str(platform or "") -> "" for a falsy platform */
        if (!platform || py_falsy(platform)) { free(owned); owned = dupstr(""); }
    }
    if (owned && owned[0]) json_set(route, "platform", json_string(owned));
    free(owned);

    static const char *KEYS[] = {"chat_id", "chat_type", "thread_id",
                                 "user_id", "user_name", "profile"};
    for (size_t i = 0; i < sizeof(KEYS)/sizeof(KEYS[0]); i++) {
        const json_t *v = src ? json_obj_get(src, KEYS[i]) : NULL;
        char *s = (v && !py_falsy(v)) ? py_str_of(v) : dupstr("");
        if (s && s[0]) json_set(route, KEYS[i], json_string(s));
        free(s);
    }
    return route;
}

