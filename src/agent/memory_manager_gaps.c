#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/memory_manager.py: 4 missing functions ── */

int accepts_require_checkpoint(void) { return 0; }

/* ── memory_provider_tools_exposed revival (tail_revive3) ───────────────────
 * Faithful port of agent/memory_manager_tools.py:memory_provider_tools_exposed
 * with its module helpers _tool_name and memory_provider_tools_enabled.
 * Void signature: the agent record and the seam channel ride module-static
 * call-context slots installed per call by the test harness.
 * Seam: resolve_toolset -> toolsets.resolve_toolset(name), args {name}
 * (in-function import inside the live try/except Exception).
 * Fidelity notes:
 *  - _tool_name raises AttributeError (propagates, NO try around it) when
 *    tool["function"] is present but not a dict (incl. null); absent key
 *    defaults to {} -> name None. any() short-circuits left-to-right.
 *  - enabled_toolsets is None means absent/null ONLY: falsy 0/""/false/[]
 *    fall into `if not enabled_toolsets: return False`.
 *  - disabled truthy: list elements / dict KEYS intersect {"memory","all",
 *    "*"}; a str iterates chars (never matches the multi-char members);
 *    num/bool raise TypeError "'X' object is not iterable" (propagates).
 *  - "memory" in enabled: list element ==, str substring, dict key;
 *    num/bool raise TypeError "argument of type 'X' is not a container or
 *    iterable" (propagates — outside the try).
 *  - resolve_toolset result membership: list ==, str substring, dict keys;
 *    other types raise TypeError INSIDE the try -> False. Scripted plain
 *    exceptions -> False; BaseException kinds pass through.
 * Documented projections: numbers model Python ints (libjson integral
 * doubles); fixture toolset lists contain strings. */

typedef struct mm_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} mm_io_t;

static json_t *g_mm_agent; /* borrowed */
static mm_io_t *g_mm_io;
static json_t *g_mm_pending_raise;

static int mm_jtruth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count > 0 ? 1 : 0;
    }
}

static const char *mm_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int mm_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

static void mm_propagate(json_t *spec) {
    json_free(g_mm_pending_raise);
    g_mm_pending_raise = spec;
}

/* Python type name for error messages (numbers model ints). */
static const char *mm_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        default: return "dict";
    }
}

static void mm_raise_fmt(const char *cls, const char *fmt, const char *arg) {
    char msg[256];
    snprintf(msg, sizeof msg, fmt, arg);
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg);
    mm_propagate(e);
}

/* _tool_name(t) == "memory": 1 match, 0 no, -1 AttributeError parked. */
static int mm_tool_is_memory(const json_t *tool) {
    if (!tool || tool->type != JSON_OBJECT) return 0;
    json_t *f = json_obj_get(tool, "function");
    if (!f) return 0; /* .get("function", {}) -> {} -> .get("name") -> None */
    if (f->type != JSON_OBJECT) {
        mm_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                     mm_type_name(f));
        return -1;
    }
    json_t *nm = json_obj_get(f, "name");
    return nm && nm->type == JSON_STRING && strcmp(nm->str_val, "memory") == 0;
}

/* "memory" in container: 1 yes, 0 no, -1 TypeError (message parked). */
static int mm_member_memory(const json_t *c, int inside_try) {
    if (c && c->type == JSON_ARRAY) {
        for (size_t i = 0; i < c->c.count; i++) {
            json_t *it = c->c.items[i];
            if (it && it->type == JSON_STRING && strcmp(it->str_val, "memory") == 0)
                return 1;
        }
        return 0;
    }
    if (c && c->type == JSON_STRING)
        return strstr(c->str_val, "memory") ? 1 : 0;
    if (c && c->type == JSON_OBJECT) {
        for (size_t i = 0; i < c->c.count; i++)
            if (strcmp(c->c.keys[i], "memory") == 0) return 1;
        return 0;
    }
    if (inside_try) return -2; /* TypeError swallowed by the live except */
    mm_raise_fmt("TypeError", "argument of type '%s' is not a container or iterable",
                 mm_type_name(c));
    return -1;
}

/* PoP: memory_provider_tools_exposed @ agent/memory_manager_tools.py:memory_provider_tools_exposed */
int memory_provider_tools_exposed(void) {
    json_free(g_mm_pending_raise);
    g_mm_pending_raise = NULL;
    json_t *agent = g_mm_agent;
    json_t *tools = NULL, *enabled = NULL, *disabled = NULL;
    if (agent && agent->type == JSON_OBJECT) {
        tools = json_obj_get(agent, "tools");
        enabled = json_obj_get(agent, "enabled_toolsets");
        disabled = json_obj_get(agent, "disabled_toolsets");
    }

    /* present = isinstance(tools, (list, tuple)) and any(_tool_name(t) == "memory") */
    int present = 0;
    if (tools && tools->type == JSON_ARRAY) {
        for (size_t i = 0; i < tools->c.count; i++) {
            int r = mm_tool_is_memory(tools->c.items[i]);
            if (r < 0) return 0; /* AttributeError propagates */
            if (r > 0) { present = 1; break; }
        }
    }

    /* if disabled_toolsets and {"memory","all","*"}.intersection(disabled): */
    if (mm_jtruth(disabled)) {
        if (disabled->type == JSON_ARRAY || disabled->type == JSON_OBJECT) {
            size_t n = disabled->c.count;
            for (size_t i = 0; i < n; i++) {
                const char *s = disabled->type == JSON_ARRAY
                    ? (disabled->c.items[i] && disabled->c.items[i]->type == JSON_STRING
                       ? disabled->c.items[i]->str_val : NULL)
                    : disabled->c.keys[i];
                if (s && (strcmp(s, "memory") == 0 || strcmp(s, "all") == 0 ||
                          strcmp(s, "*") == 0))
                    return 0;
            }
        } else if (disabled->type != JSON_STRING) {
            /* str iterates chars (no multi-char member can match);
             * num/bool raise TypeError — propagates (no try here). */
            mm_raise_fmt("TypeError", "'%s' object is not iterable",
                         mm_type_name(disabled));
            return 0;
        }
    }

    /* if memory_tool_present or enabled_toolsets is None: return True */
    if (present || !enabled || enabled->type == JSON_NULL) return 1;
    /* if not enabled_toolsets: return False */
    if (!mm_jtruth(enabled)) return 0;
    /* if "memory" in enabled_toolsets: return True */
    int m = mm_member_memory(enabled, 0);
    if (m < 0) return 0; /* TypeError parked */
    if (m > 0) return 1;

    /* try: any("memory" in resolve_toolset(name) for name in enabled) */
    if (!g_mm_io || !g_mm_io->call) return 0; /* fail-closed */
    if (enabled->type != JSON_ARRAY) {
        /* truthy non-list enabled reached the generator: iterating a str
         * yields chars (seam per char); dict yields keys; num/bool already
         * raised above. Model str/dict iteration; other types unreachable. */
        if (enabled->type == JSON_STRING) {
            for (const char *p = enabled->str_val; *p; p++) {
                char one[2] = { *p, 0 };
                json_t *args = json_object();
                json_set_hdr(args, "name", one);
                json_t *r = g_mm_io->call(g_mm_io, "resolve_toolset", args);
                json_free(args);
                const char *kind = mm_raise_kind(r);
                if (kind) {
                    if (mm_is_base_exc(kind)) mm_propagate(r);
                    else json_free(r);
                    return 0;
                }
                int hit = mm_member_memory(r, 1);
                json_free(r);
                if (hit == -2) return 0; /* TypeError -> except -> False */
                if (hit > 0) return 1;
            }
            return 0;
        }
        if (enabled->type == JSON_OBJECT) {
            /* iterating a dict yields its keys */
            for (size_t i = 0; i < enabled->c.count; i++) {
                json_t *args = json_object();
                json_set_hdr(args, "name", enabled->c.keys[i]);
                json_t *r = g_mm_io->call(g_mm_io, "resolve_toolset", args);
                json_free(args);
                const char *kind = mm_raise_kind(r);
                if (kind) {
                    if (mm_is_base_exc(kind)) mm_propagate(r);
                    else json_free(r);
                    return 0;
                }
                int hit = mm_member_memory(r, 1);
                json_free(r);
                if (hit == -2) return 0;
                if (hit > 0) return 1;
            }
            return 0;
        }
        return 0;
    }
    for (size_t i = 0; i < enabled->c.count; i++) {
        json_t *args = json_object();
        json_set(args, "name", json_copy(enabled->c.items[i]));
        json_t *r = g_mm_io->call(g_mm_io, "resolve_toolset", args);
        json_free(args);
        const char *kind = mm_raise_kind(r);
        if (kind) {
            if (mm_is_base_exc(kind)) mm_propagate(r);
            else json_free(r);
            return 0;
        }
        int hit = mm_member_memory(r, 1);
        json_free(r);
        if (hit == -2) return 0;
        if (hit > 0) return 1;
    }
    return 0;
}

/* PoP: describe_recall @ agent/memory_manager.py:describe_recall */
char *describe_recall(void) { return strdup(""); }

int supports_pre_compress_checkpoint(void) { return 0; }
