/*
 * t_port_plugin_revive.c — differential harness for the hermes_cli/plugins.py
 * revival in lib/libplugin/plugin.c.
 *
 * plugin.o is NOT in the binary link list, so the port .c is #included here
 * (self-contained; the oracle gate links only lib/libjson/json.o):
 *   bash tests/run_one_oracle.sh plugin_revive oracle_plugin_revive.py "" \
 *        "lib/libjson/json.o"
 *
 * Reads tests/fixtures/plugin_revive.jsonl on stdin (one case per line):
 *   {"fn": "<live python name>", "in": {...}, "state": {...}, "io": {...}}
 * Emits one JSON object per case:
 *   {"case": <echo>, "ret": ..., "extra": {...}, "trace": [...],
 *    "state": {...}, "err": {"cls","msg"}|null}
 * The oracle (tests/oracle_plugin_revive.py) replays the SAME case against
 * AST-extracted live defs with recording seam stubs and diffs ret/extra/
 * trace/state/err. io scripts: a plain value is reused for every call of that
 * seam; {"__list__": [...]} is consumed one element per call (last repeats).
 * A script value {"__raise__": cls, "msg": m} models a raised exception.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/libplugin/plugin.c"

static json_t *g_trace;
static json_t *g_io;

#define MAX_SEAM_NAMES 64
static struct { const char *name; int idx; } g_idx[MAX_SEAM_NAMES];
static int g_idx_count;

static int seam_idx(const char *name, int bump) {
    for (int i = 0; i < g_idx_count; i++) {
        if (strcmp(g_idx[i].name, name) == 0) {
            int v = g_idx[i].idx;
            if (bump) g_idx[i].idx++;
            return v;
        }
    }
    if (g_idx_count < MAX_SEAM_NAMES) {
        g_idx[g_idx_count].name = name;
        g_idx[g_idx_count].idx = bump ? 1 : 0;
        g_idx_count++;
    }
    return 0;
}

/* Universal recording seam stub: consumes the case's io script, records
 * {ev, args, ret} in order, applies the scripted discover_and_load registry
 * effect, and returns the script value (NULL for null). args is owned. */
static json_t *harness_seam(void *ctx, const char *name, const json_t *args) {
    (void)ctx;
    json_t *script = g_io ? json_obj_get(g_io, name) : NULL;
    json_t *val = NULL;
    if (script && script->type == JSON_OBJECT && json_obj_get(script, "__list__")) {
        json_t *lst = json_obj_get(script, "__list__");
        size_t n = json_len(lst);
        int idx = seam_idx(name, 1);
        if (n > 0) val = json_get(lst, (size_t)idx < n ? (size_t)idx : n - 1);
    } else {
        val = script;
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", val ? json_copy(val) : json_null());
    json_append(g_trace, ev);
    if (strcmp(name, "discover_and_load") == 0) {
        int raised = val && val->type == JSON_OBJECT && json_obj_get(val, "__raise__");
        if (raised) {
            g_py_reg.discovered = 0; /* live sweep resets _discovered on failure */
        } else if (val && val->type == JSON_OBJECT) {
            json_t *sd = json_obj_get(val, "set_discovered");
            if (sd && sd->type == JSON_BOOL) g_py_reg.discovered = sd->bool_val ? 1 : 0;
        }
    }
    json_free((json_t *)args);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    if (len > 0 && buf[len - 1] == '\r') len--;
    buf[len] = '\0';
    return buf;
}

/* LIVE Path objects serialise as plain strings; the C seam tags them
 * {"__path__": s}. Normalise recursively before emitting. */
static json_t *prv_unwrap_path(json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_OBJECT && v->c.count == 1 &&
            strcmp(v->c.keys[0], "__path__") == 0) {
        json_t *p = v->c.items[0];
        if (p && p->type == JSON_STRING) return json_copy(p);
    }
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *n = prv_unwrap_path(v->c.items[i]);
            if (n) { json_free(v->c.items[i]); v->c.items[i] = n; }
        }
    } else if (v->type == JSON_OBJECT) {
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *n = prv_unwrap_path(v->c.items[i]);
            if (n) { json_free(v->c.items[i]); v->c.items[i] = n; }
        }
    }
    return NULL;
}

static void emit_str(json_t *out, const char *key, const char *v) {
    json_set(out, key, v ? json_string(v) : json_null());
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        char *err = NULL;
        json_t *casej = json_parse(line, &err);
        free(line);
        if (!casej) {
            fprintf(stderr, "harness: bad case JSON: %s\n", err ? err : "?");
            free(err);
            return 1;
        }
        const char *fn = json_get_str(casej, "fn", "");
        json_t *in = json_obj_get(casej, "in");
        json_t *state = json_obj_get(casej, "state");
        g_io = json_obj_get(casej, "io");
        g_trace = json_array();
        g_idx_count = 0;
        py_plugin_reset();
        py_plugin_install_state(state);
        py_plugin_set_seam(harness_seam, NULL);

        json_t *out = json_object();
        json_set(out, "case", json_copy(casej));
        json_t *extra = json_object();
        json_t *ret = NULL;
        char *tok = NULL;

        if (strcmp(fn, "_env_enabled") == 0) {
            ret = json_bool(plugin_env_enabled(json_get_str(in, "name", "")));
        } else if (strcmp(fn, "_install_plugin_debug_handler") == 0) {
            if (json_get_bool(in, "force", false)) py_install_debug_handler(1);
            else plugin_install_debug_handler();
        } else if (strcmp(fn, "_tool_override_allowed") == 0) {
            ret = json_bool(plugin_tool_override_allowed(json_get_str(in, "name", "")));
        } else if (strcmp(fn, "_platform_name_from_manifest") == 0) {
            json_t *mf = json_obj_get(in, "manifest");
            py_plugin_install_call_ctx(NULL, NULL, NULL, mf);
            const char *s = plugin_platform_name_from_manifest(mf);
            emit_str(out, "ret", s);
            ret = NULL; /* already emitted */
        } else if (strcmp(fn, "_load_directory_module") == 0) {
            json_t *mf = json_obj_get(in, "manifest");
            py_plugin_install_call_ctx(NULL, NULL, NULL, mf);
            const char *mn = json_get_str(in, "module_name", NULL);
            free(g_py_reg.load_module_name);
            g_py_reg.load_module_name = mn ? py_strdup(mn) : NULL;
            tok = (char *)plugin_load_directory_module(json_get_str(in, "dir", ""));
            emit_str(out, "ret", tok);
            ret = NULL;
        } else if (strcmp(fn, "_load_entrypoint_module") == 0) {
            json_t *mf = json_obj_get(in, "manifest");
            py_plugin_install_call_ctx(NULL, NULL, NULL, mf);
            tok = (char *)plugin_load_entrypoint_module(json_get_str(in, "path", ""));
            emit_str(out, "ret", tok);
            ret = NULL;
        } else if (strcmp(fn, "has_hook") == 0) {
            ret = json_bool(plugin_has_hook(json_get_str(in, "name", "")));
        } else if (strcmp(fn, "has_middleware") == 0) {
            ret = json_bool(plugin_has_middleware(json_get_str(in, "name", "")));
        } else if (strcmp(fn, "find_plugin_skill") == 0) {
            json_t *hit = plugin_find_skill(json_get_str(in, "name", ""));
            if (hit) {
                prv_unwrap_path(hit);
                json_t *path = json_obj_get(hit, "__path__");
                json_set(out, "ret", path ? json_copy(path) : hit);
                ret = NULL;
            }
        } else if (strcmp(fn, "remove_plugin_skill") == 0) {
            ret = json_bool(plugin_remove_skill(json_get_str(in, "name", "")));
        } else if (strcmp(fn, "get_pre_tool_call_directive") == 0) {
            py_plugin_install_call_ctx(json_obj_get(in, "args"),
                                       json_obj_get(in, "hook_kwargs"), NULL, NULL);
            const char *action = plugin_get_pre_tool_call_directive(json_get_str(in, "tool_name", ""));
            emit_str(out, "ret", action);
            emit_str(extra, "message", plugin_pre_tool_directive_message());
            ret = NULL;
        } else if (strcmp(fn, "get_pre_tool_call_block_message") == 0) {
            py_plugin_install_call_ctx(json_obj_get(in, "args"),
                                       json_obj_get(in, "hook_kwargs"), NULL, NULL);
            const char *msg = plugin_get_pre_tool_call_block_message(json_get_str(in, "tool_name", ""));
            emit_str(out, "ret", msg);
            emit_str(extra, "message", plugin_pre_tool_directive_message());
            ret = NULL;
        } else if (strcmp(fn, "resolve_pre_tool_block") == 0) {
            py_plugin_install_call_ctx(json_obj_get(in, "args"),
                                       json_obj_get(in, "hook_kwargs"), NULL, NULL);
            bool blocked = plugin_resolve_pre_tool_block(json_get_str(in, "tool_name", ""));
            ret = json_bool(blocked);
            emit_str(extra, "message", plugin_resolve_pre_tool_block_message());
        } else if (strcmp(fn, "get_pre_verify_continue_message") == 0) {
            py_plugin_install_call_ctx(NULL, NULL, json_obj_get(in, "kwargs"), NULL);
            const char *msg = plugin_get_pre_verify_continue_message(json_get_str(in, "tool_name", ""));
            emit_str(out, "ret", msg);
            ret = NULL;
        } else if (strcmp(fn, "_ensure_plugins_discovered") == 0) {
            ret = json_bool(plugin_ensure_discovered());
        } else if (strcmp(fn, "get_plugin_context_engine") == 0) {
            const char *s = plugin_get_context_engine(json_get_str(in, "name", ""));
            emit_str(out, "ret", s);
            ret = NULL;
        } else if (strcmp(fn, "get_plugin_command_handler") == 0) {
            tok = (char *)plugin_get_command_handler(json_get_str(in, "name", ""));
            emit_str(out, "ret", tok);
            ret = NULL;
        } else {
            fprintf(stderr, "harness: unknown fn '%s'\n", fn);
            json_free(casej);
            json_free(out);
            json_free(extra);
            json_free(g_trace);
            return 1;
        }

        if (py_plugin_exc_set()) {
            /* raised: ret/extra carry no value on either side */
            if (ret) { json_free(ret); }
            json_set(out, "ret", json_null());
            json_free(extra);
            extra = json_object();
            json_t *e = json_object();
            json_set(e, "cls", json_string(py_plugin_exc_cls()));
            json_set(e, "msg", json_string(py_plugin_exc_msg()));
            json_set(out, "err", e);
        } else {
            /* Some branches pre-emit a string ret (emit_str) and set ret=NULL
             * to mean "already emitted" — do NOT clobber it with json_null. */
            if (!json_obj_get(out, "ret")) {
                json_set(out, "ret", ret ? ret : json_null());
            } else if (ret) {
                json_free(ret);
            }
            json_set(out, "err", json_null());
        }
        json_set(out, "extra", extra);
        json_set(out, "trace", g_trace);
        json_t *state_out = py_plugin_state_snapshot();
        prv_unwrap_path(state_out);
        json_set(out, "state", state_out);
        char *s = json_serialize(out);
        if (!s) {
            fprintf(stderr, "harness: serialize failed for fn '%s'\n", fn);
            return 1;
        }
        puts(s);
        free(s);
        free(tok);
        json_free(out);
        json_free(casej);
        g_trace = NULL;
        g_io = NULL;
    }
    return 0;
}