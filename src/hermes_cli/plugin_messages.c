/*
 * plugin_messages.c — C port of upstream Hermes hermes_cli/plugin_messages.py.
 *
 * PoP: w13_pluginmess_f27478_err @ hermes_cli/plugin_messages.py:_err
 * PoP: w13_pluginmess_f27478_props @ hermes_cli/plugin_messages.py:PluginMessage.ok/error
 * PoP: w13_pluginmess_f27478_repr @ hermes_cli/plugin_messages.py:PluginMessage.__repr__
 * PoP: w13_pluginmess_f27478_refused @ hermes_cli/plugin_messages.py:PluginMessage._refused
 * PoP: w13_pluginmess_f27478_settle @ hermes_cli/plugin_messages.py:PluginMessage._settle
 * PoP: w13_pluginmess_f27478_adopt @ hermes_cli/plugin_messages.py:PluginMessage._adopt
 *
 * Pure handle state transitions; no gateway loop or asyncio interaction.
 */
#include "noc_w13_pluginmess_f27478.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static json_t *val_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Python repr() for the scalar shapes used by __repr__. */
static char *py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[128];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        char *out = (char *)malloc(2 * n + 4);
        if (!out) return strdup("''");
        size_t j = 0;
        out[j++] = '\'';
        for (size_t i = 0; i < n; i++) {
            if (s[i] == '\'' || s[i] == '\\') out[j++] = '\\';
            out[j++] = s[i];
        }
        out[j++] = '\'';
        out[j] = '\0';
        return out;
    }
    char *s = json_dumps(v, 0);
    return s ? s : strdup("None");
}

json_t *w13_pluginmess_f27478_err(const struct json_t *code, const struct json_t *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "error", val_or_null(code));
    if (truthy(detail)) json_set(o, "detail", val_or_null(detail));
    return o;
}

static int state_target_null(const json_t *state) {
    return state && truthy(json_obj_get((json_t *)state, "target_null"));
}

static json_t *state_result(const json_t *state) {
    return json_obj_get((json_t *)state, "result");
}

static json_t *state_mid(const json_t *state) {
    return json_obj_get((json_t *)state, "message_id");
}

static json_t *make_state(const json_t *target_null, const json_t *result, const json_t *mid) {
    json_t *o = json_object();
    json_set(o, "target_null", val_or_null(target_null));
    json_set(o, "result", val_or_null(result));
    json_set(o, "message_id", val_or_null(mid));
    return o;
}

json_t *w13_pluginmess_f27478_props(const struct json_t *state) {
    json_t *r = state_result(state);
    json_t *o = json_object();
    json_set(o, "ok", json_bool(truthy(json_obj_get(r, "ok")) ? 1 : 0));
    json_set(o, "error", val_or_null(json_obj_get(r, "error")));
    return o;
}

json_t *w13_pluginmess_f27478_repr(const struct json_t *state) {
    json_t *r = state_result(state);
    int ok = truthy(json_obj_get(r, "ok"));
    char *mid = py_repr(state_mid(state));
    char *err = py_repr(json_obj_get(r, "error"));
    char buf[2048];
    snprintf(buf, sizeof(buf), "<PluginMessage ok=%s message_id=%s error=%s>",
             ok ? "True" : "False", mid ? mid : "None", err ? err : "None");
    free(mid);
    free(err);
    return json_string(buf);
}

json_t *w13_pluginmess_f27478_refused(const struct json_t *state) {
    if (!state_target_null(state)) return json_null();
    json_t *r = state_result(state);
    json_t *e = json_obj_get(r, "error");
    json_t *code = truthy(e) ? e : json_string("no_conversation");
    json_t *detail = json_obj_get(r, "detail");
    return w13_pluginmess_f27478_err(code, detail ? detail : json_string(""));
}

json_t *w13_pluginmess_f27478_settle(const struct json_t *state, const struct json_t *result) {
    json_t *mid = state_mid(state);
    if (result && truthy(json_obj_get(result, "ok")) && truthy(json_obj_get(result, "message_id")))
        mid = json_obj_get(result, "message_id");
    json_t *t = json_obj_get((json_t *)state, "target_null");
    return make_state(t, result, mid);
}

json_t *w13_pluginmess_f27478_adopt(const struct json_t *state, const struct json_t *other) {
    (void)state; /* target/result/message_id all come from `other` */
    json_t *t = json_obj_get((json_t *)other, "target_null");
    json_t *r = state_result(other);
    json_t *mid = state_mid(other);
    return make_state(t, r, mid);
}
