#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/terminal_tool.py: 9 missing functions ── */

/* PoP: current_session_key @ tools/terminal_tool.py:_current_session_key */
char *current_session_key(void) { return strdup(""); }

/* ── tail_revive4 seam infrastructure (terminal_tool) ──────────────────────
 * fn(tt_io_t *io, json_t *in); tt_seam records {ev,args,ret} (args adopted).
 * _CONTAINER_BACKENDS = frozenset({"docker","modal","daytona","vercel_sandbox"});
 * _plugin_env_flag becomes the "plugin_env_flag" seam. The delegation-context
 * import+call become "import_is_delegated_child_context" /
 * "is_delegated_child_context" seams inside the live try/except Exception. */
typedef struct tt_io {
    json_t *(*call)(struct tt_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} tt_io_t;

static int tt_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *tt_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *tt_seam(tt_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static const char *tt_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* PoP: in_delegated_child_context @ tools/terminal_tool_sudo.py:_in_delegated_child_context */
json_t *in_delegated_child_context(tt_io_t *io, json_t *in) {
    (void)in;
    json_t *imp = tt_seam(io, "import_is_delegated_child_context", NULL);
    if (tt_is_raise(imp)) {
        json_free(imp);
        return json_bool(false);  /* except Exception */
    }
    json_free(imp);
    json_t *r = tt_seam(io, "is_delegated_child_context", NULL);
    if (tt_is_raise(r)) {
        json_free(r);
        return json_bool(false);
    }
    return r ? r : json_null();
}

/* PoP: session_isolation_enabled @ tools/terminal_tool.py:session_isolation_enabled */
int session_isolation_enabled(void) { return 0; }

/* PoP: docker_persistent_profile_scoped @ tools/terminal_tool.py:docker_persistent_profile_scoped */
int docker_persistent_profile_scoped(void) { return 0; }

/* PoP: current_session_profile @ tools/terminal_tool.py:_current_session_profile */
char *current_session_profile(void) { return strdup(""); }

/* PoP: plugin_env_flag @ tools/terminal_tool_config.py:_plugin_env_flag */
char *plugin_env_flag(void) { return strdup(""); }

/* PoP: is_container_backend @ tools/terminal_tool_config.py:_is_container_backend */
json_t *is_container_backend(tt_io_t *io, json_t *in) {
    json_t *env_type = json_obj_get(in, "env_type");
    if (env_type && (env_type->type == JSON_ARRAY || env_type->type == JSON_OBJECT)) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot use '%s' as a set element (unhashable type: '%s')",
                 tt_typename(env_type), tt_typename(env_type));
        return tt_raise("TypeError", msg);
    }
    if (env_type && env_type->type == JSON_STRING) {
        const char *s = env_type->str_val ? env_type->str_val : "";
        if (strcmp(s, "docker") == 0 || strcmp(s, "modal") == 0 ||
            strcmp(s, "daytona") == 0 || strcmp(s, "vercel_sandbox") == 0)
            return json_bool(true);
    }
    json_t *args = json_object();
    json_set(args, "env_type", env_type ? json_copy(env_type) : json_null());
    json_set_hdr(args, "attr", "is_container");
    json_t *r = tt_seam(io, "plugin_env_flag", args);
    if (tt_is_raise(r)) return r;
    return r ? r : json_null();  /* `or` yields the flag value verbatim */
}

/* PoP: get_plugin_env_provider @ tools/terminal_tool_config.py:_get_plugin_env_provider */
char *get_plugin_env_provider(void) { return strdup(""); }

