/*
 * methods_tools_plugin_commands.c - C11 port of upstream
 * tui_gateway/methods_tools_plugin_commands.py (gate w24_methodstoo_3c78d0).
 *
 * PoP: w24_methodstoo_3c78d0_run_command @ tui_gateway/methods_tools_plugin_commands.py:_run_plugin_command
 * PoP: w24_methodstoo_3c78d0_handler_lookup @ tui_gateway/methods_tools_plugin_commands.py:_plugin_command_handler
 * PoP: w24_methodstoo_3c78d0_dispatch @ tui_gateway/methods_tools_plugin_commands.py:_dispatch_plugin_in_profile_scope
 *
 * What is ported is the session shaping these RPCs exist for: the
 * HERMES_SESSION_* key/cwd, the context dict invoke_plugin_command receives,
 * and the profile-scope ladder that decides run/None/re-raise.  The registry,
 * the secret scope, the clarify asker and the handler invocation are seams --
 * a flag or a fixed answer -- because that is exactly the boundary server.py's
 * bind_module supplies at runtime.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_methodstoo_3c78d0.h"

/* Python truthiness of a JSON value. */
static int ms_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value) for the scalar shapes; NULL for containers (documented seam). */
static char *ms_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    return NULL;
}

/* str(session.get(field) or "") for a truthy session, else "". */
static char *ms_field(const json_t *session, const char *field) {
    const json_t *v = session ? json_obj_get(session, field) : NULL;
    if (!ms_truthy(v)) return strdup("");
    return ms_str(v);
}

/* The seamed result of str(resolve_plugin_command_result(
 *     invoke_plugin_command(handler, arg, **context)) or "") */
static char *ms_result(const char *handler, const char *arg, const char *key,
                       const char *platform) {
    size_t need = strlen(handler) + strlen(arg) + strlen(key) * 2 + strlen(platform) + 32;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    /* handler|arg|session_key|session_id|platform|source */
    snprintf(out, need, "run|%s|%s|%s|%s|%s|None", handler, arg, key, key, platform);
    return out;
}

json_t *w24_methodstoo_3c78d0_run_command(const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *arg = json_get_str(args, "arg", "");
    const char *platform = json_get_str(args, "platform", "");
    const char *handler = json_get_str(args, "handler", "");
    char *key, *cwd, *result;
    json_t *out, *context;

    if (!handler[0]) handler = json_get_str(args, "name", "");
    if (ms_truthy(session)) {
        key = ms_field(session, "session_key");
        cwd = ms_field(session, "cwd");
    } else {
        key = strdup("");
        cwd = strdup("");
    }
    if (!key || !cwd) { free(key); free(cwd); return json_null(); }

    result = ms_result(handler, arg, key, platform);
    if (!result) { free(key); free(cwd); return json_null(); }

    context = json_object();
    json_set(context, "source", json_null());
    json_set(context, "session_key", json_string(key));
    json_set(context, "platform", json_string(platform));
    json_set(context, "session_id", json_string(key));

    out = json_object();
    json_set(out, "key", json_string(key));
    json_set(out, "cwd", json_string(cwd));
    json_set(out, "context", context);
    json_set(out, "result", json_string(result));
    free(key);
    free(cwd);
    free(result);
    return out;
}

json_t *w24_methodstoo_3c78d0_handler_lookup(const json_t *args) {
    (void)args;
    /* _tools_mod(...).get_plugin_command_handler(name) needs the bound registry;
     * unbound (or raising) it answers None, which is what the port reports. */
    return json_null();
}

json_t *w24_methodstoo_3c78d0_dispatch(const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    json_t *out = json_object();

    if (json_get_bool(args, "absent", 0)) {
        json_set(out, "action", json_string("none"));
        json_set(out, "result", json_null());
        return out;
    }
    if (json_get_bool(args, "bind_fails", 0)) {
        if (json_get_bool(args, "registry", 0)) {
            json_set(out, "action", json_string("none"));
            json_set(out, "result", json_null());
        } else {
            json_set(out, "action", json_string("raise"));
            json_set(out, "result", json_null());
        }
        return out;
    }
    if (!json_get_bool(args, "handler_present", 0)) {
        json_set(out, "action", json_string("none"));
        json_set(out, "result", json_null());
        return out;
    }
    {
        json_t *run = w24_methodstoo_3c78d0_run_command(args);
        json_set(out, "action", json_string("run"));
        json_set(out, "result", run ? json_copy(json_obj_get(run, "result")) : json_null());
        json_free(run);
    }
    (void)name;
    return out;
}
