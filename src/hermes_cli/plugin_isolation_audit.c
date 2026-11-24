/*
 * plugin_isolation_audit.c - real C11 port of upstream Hermes
 *   hermes_cli/plugin_isolation_audit.py decision helpers.
 * The AST walk stays in Python; every rule the walker applies (package sets, ctx-method
 * tables, registry-call rule, path filter, verdict and report shaping) is real here.
 * PoP: w19_pluginisol_f4e6f5_hermes_module @ hermes_cli/plugin_isolation_audit.py:_SourceVisitor._hermes
 * PoP: w19_pluginisol_f4e6f5_dedupe @ hermes_cli/plugin_isolation_audit.py:_dedupe
 * PoP: w19_pluginisol_f4e6f5_python_files @ hermes_cli/plugin_isolation_audit.py:_python_files
 * PoP: w19_pluginisol_f4e6f5_ctx_method @ hermes_cli/plugin_isolation_audit.py:_SourceVisitor.visit_Call
 * PoP: w19_pluginisol_f4e6f5_registry_reason @ hermes_cli/plugin_isolation_audit.py:_flag_registry_call
 * PoP: w19_pluginisol_f4e6f5_patch_reason @ hermes_cli/plugin_isolation_audit.py:_flag_write
 * PoP: w19_pluginisol_f4e6f5_report @ hermes_cli/plugin_isolation_audit.py:IsolationReport
 */
#include "noc_w19_pluginisol_f4e6f5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *const HERMES_PACKAGES[] = {
    "agent", "tools", "hermes_cli", "gateway", "cron", "providers", "plugins", "tui_gateway",
    "acp_adapter", "run_agent", "cli", "model_tools", "toolsets", "hermes_state", "hermes_constants",
    "hermes_logging", "hermes_time", "utils", "registration_lifecycle", "hermes_platform", "hermes_agent",
    NULL
};

static const char *const SKIP_DIRS[] = {
    "tests", "test", ".git", "__pycache__", "node_modules", ".venv", "venv", "docs", NULL
};

static const char *const STREAMING_MARKERS[] = {
    "StreamingResponse", "EventSourceResponse", ".websocket(", "WebSocket", NULL
};

typedef struct { const char *name; const char *text; } named_text;

static const named_text UNSUPPORTED_CTX[] = {
    {"register_login_backend", "browser-vault fills hold a server-local context manager"},
    {"register_browser_page_backend", "browser-vault fill scripts carry credentials that stay in the Hermes process"},
    {"register_platform", "platform adapters run on the gateway's event loop with native SDK clients"},
    {"register_platform_handler", "the handler factory receives the platform's native SDK client"},
    {"register_telegram_handler", "the handler factory receives the native Telegram Application"},
    {"register_slack_action_handler", "the callback receives native slack_bolt ack/body objects"},
    {"register_dashboard_auth_provider", "dashboard auth is owned by the launch process, not a profile"},
    {"register_approval_transport", "approval transports hold the live approval request/response channel"},
    {"post_message", "the returned handle is bound to the gateway's event loop and live adapter"},
    {"apost_message", "the returned handle is bound to the gateway's event loop and live adapter"},
    {"fork_session", "the returned handle tracks a fork thread of the live session agent in the Hermes process"},
    {NULL, NULL}
};

static const named_text SKIPPED_CTX[] = {
    {"register_cli_command", "`hermes <command>` subcommands are wired into the local CLI parser"},
    {NULL, NULL}
};

static const named_text DEGRADED_HOOKS[] = {
    {"pre_gateway_dispatch", "receives the live gateway runner and session store"},
    {NULL, NULL}
};

static const named_text REGISTRY_CALLS[] = {
    {"tools.registry.register", ""},
    {"tools.registry.deregister", ""},
    {"gateway.platform_registry.register", ""},
    {"providers.register_provider", ""},
    {NULL, NULL}
};

/* Python equality for the hashable JSON scalars dict.fromkeys relies on (1 == True). */
static int py_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return !a && !b;
    int an = (a->type == JSON_BOOL || a->type == JSON_NUMBER);
    int bn = (b->type == JSON_BOOL || b->type == JSON_NUMBER);
    if (an && bn) {
        double av = (a->type == JSON_BOOL) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = (b->type == JSON_BOOL) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        default: return 0; /* containers are unhashable in Python: never equal here */
    }
}

static const char *lookup(const named_text *tab, const char *name) {
    if (!name) return NULL;
    for (int i = 0; tab[i].name; i++)
        if (!strcmp(tab[i].name, name)) return tab[i].text;
    return NULL;
}

static int member(const char *const *tab, const char *name) {
    if (!name) return 0;
    for (int i = 0; tab[i]; i++)
        if (!strcmp(tab[i], name)) return 1;
    return 0;
}

int w19_pluginisol_f4e6f5_hermes_module(const char *module) {
    if (!module) return 0;
    const char *dot = strchr(module, '.');
    size_t n = dot ? (size_t)(dot - module) : strlen(module);
    char top[256];
    if (n >= sizeof(top)) n = sizeof(top) - 1;
    memcpy(top, module, n);
    top[n] = '\0';
    return member(HERMES_PACKAGES, top);
}

json_t *w19_pluginisol_f4e6f5_dedupe(json_t *items) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *v = json_get(items, i);
        int seen = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            json_t *w = json_get(out, j);
            if (py_equal(v, w)) { seen = 1; break; }
        }
        if (!seen) json_append(out, json_copy(v));
    }
    return out;
}

/* rel[:-1] directories vs _SKIP_DIRS / leading dot; rel[-1] vs the test-file rules. */
json_t *w19_pluginisol_f4e6f5_python_files(json_t *paths) {
    json_t *out = json_array();
    if (!paths || paths->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(paths); i++) {
        json_t *v = json_get(paths, i);
        if (!v || v->type != JSON_STRING) continue;
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s", v->str_val ? v->str_val : "");
        /* split on "/" */
        char *parts[256];
        int nparts = 0;
        char *p = buf;
        while (nparts < 255) {
            parts[nparts++] = p;
            char *slash = strchr(p, '/');
            if (!slash) break;
            *slash = '\0';
            p = slash + 1;
        }
        if (nparts == 0) continue;
        const char *base = parts[nparts - 1];
        int skip = 0;
        for (int k = 0; k < nparts - 1; k++) {
            if (parts[k][0] == '.' || member(SKIP_DIRS, parts[k])) { skip = 1; break; }
        }
        if (skip) continue;
        size_t blen = strlen(base);
        if (blen < 3 || strcmp(base + blen - 3, ".py")) continue;
        if (!strncmp(base, "test_", 5) || !strcmp(base, "conftest.py")) continue;
        json_append(out, json_copy(v));
    }
    return out;
}

json_t *w19_pluginisol_f4e6f5_ctx_method(const char *name, const char *rel, long long line) {
    json_t *out = json_object();
    const char *unsupported = lookup(UNSUPPORTED_CTX, name);
    const char *skipped = lookup(SKIPPED_CTX, name);
    int registers = 0;
    if (name) {
        if (!strncmp(name, "register_", 9)) registers = 1;
        else if (!strcmp(name, "subscribe") || !strcmp(name, "dispatch_tool")
                 || !strcmp(name, "inject_message") || !strcmp(name, "emit")) registers = 1;
    }
    char buf[2048];
    if (unsupported) {
        snprintf(buf, sizeof(buf), "%s:%lld ctx.%s(): %s", rel ? rel : "", line, name, unsupported);
        json_set(out, "reason", json_string(buf));
    } else {
        json_set(out, "reason", json_null());
    }
    if (skipped) {
        snprintf(buf, sizeof(buf), "ctx.%s() is skipped in the host (%s)", name, skipped);
        json_set(out, "note", json_string(buf));
    } else {
        json_set(out, "note", json_null());
    }
    json_set(out, "registers", json_bool(registers));
    return out;
}

char *w19_pluginisol_f4e6f5_hook_note(const char *hook) {
    const char *text = lookup(DEGRADED_HOOKS, hook);
    if (!text) return NULL;
    char buf[1024];
    snprintf(buf, sizeof(buf), "hook %s %s; those fields are placeholders in the host", hook, text);
    return dupstr(buf);
}

char *w19_pluginisol_f4e6f5_registry_reason(const char *target, int model_provider,
                                            const char *rel, long long line) {
    if (!target || !*target) return NULL;
    const char *lastdot = strrchr(target, '.');
    char module[1024];
    const char *attr = "";
    if (lastdot) {
        size_t n = (size_t)(lastdot - target);
        if (n >= sizeof(module)) n = sizeof(module) - 1;
        memcpy(module, target, n);
        module[n] = '\0';
        attr = lastdot + 1;
    } else {
        module[0] = '\0';
        attr = target;
    }
    if (model_provider && !strcmp(attr, "register_provider") && !strcmp(module, "providers")) return NULL;
    int direct = lookup(REGISTRY_CALLS, target) != NULL;
    if (!direct) {
        size_t mlen = strlen(module);
        if (mlen >= 9 && !strcmp(module + mlen - 9, "_registry")
            && (!strcmp(attr, "register") || !strcmp(attr, "register_provider"))) direct = 1;
    }
    if (!direct) return NULL;
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "%s:%lld calls %s() directly instead of through ctx (registers in the host, not in Hermes)",
             rel ? rel : "", line, target);
    return dupstr(buf);
}

char *w19_pluginisol_f4e6f5_patch_reason(const char *rel, long long line,
                                         const char *owner, const char *attr) {
    if (!owner) return NULL;
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "%s:%lld patches %s.%s (a Hermes module attribute; the host's copy is not the one Hermes runs)",
             rel ? rel : "", line, owner, attr ? attr : "");
    return dupstr(buf);
}

char *w19_pluginisol_f4e6f5_setattr_reason(const char *rel, long long line, const char *owner) {
    if (!owner) return NULL;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s:%lld setattr() on %s (patches a Hermes module)",
             rel ? rel : "", line, owner);
    return dupstr(buf);
}

/* str(kind or "").strip().lower() then the manifest-kind table. */
char *w19_pluginisol_f4e6f5_manifest_kind_reason(const char *kind) {
    char buf[256];
    size_t n = 0;
    if (kind) {
        while (*kind == ' ' || *kind == '\t' || *kind == '\n' || *kind == '\r' || *kind == '\f' || *kind == '\v') kind++;
        size_t len = strlen(kind);
        while (len > 0) {
            char c = kind[len - 1];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') len--;
            else break;
        }
        for (size_t i = 0; i < len && n < sizeof(buf) - 1; i++) {
            char c = kind[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            buf[n++] = c;
        }
    }
    buf[n] = '\0';
    if (!strcmp(buf, "platform"))
        return dupstr("kind 'platform': gateway platform adapters run in the Hermes process");
    return NULL;
}

const char *w19_pluginisol_f4e6f5_streaming_marker(const char *source) {
    if (!source) return NULL;
    for (int i = 0; STREAMING_MARKERS[i]; i++)
        if (strstr(source, STREAMING_MARKERS[i])) return STREAMING_MARKERS[i];
    return NULL;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w19_pluginisol_f4e6f5_report(const char *verdict, json_t *reasons,
                                     json_t *notes, json_t *ctx_methods) {
    const char *v = verdict ? verdict : "";
    int host_ready = (!strcmp(v, "host") || !strcmp(v, "portable"));

    json_t *dict = json_object();
    json_set(dict, "verdict", json_string(v));
    json_set(dict, "host_ready", json_bool(host_ready));
    json_set(dict, "reasons", reasons ? json_copy(reasons) : json_array());
    json_set(dict, "notes", notes ? json_copy(notes) : json_array());

    /* ctx_methods: sorted(set(...)) */
    const char *items[512];
    int n = 0;
    if (ctx_methods && ctx_methods->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ctx_methods) && n < 512; i++) {
            json_t *e = json_get(ctx_methods, i);
            if (e && e->type == JSON_STRING && e->str_val) items[n++] = e->str_val;
        }
    }
    qsort(items, (size_t)n, sizeof(items[0]), cmp_str);
    json_t *sorted = json_array();
    for (int i = 0; i < n; i++) {
        int dup = (i > 0 && !strcmp(items[i], items[i - 1]));
        if (!dup) json_append(sorted, json_string(items[i]));
    }
    json_set(dict, "ctx_methods", sorted);

    char summary[4096];
    if (!strcmp(v, "portable")) {
        snprintf(summary, sizeof(summary), "portable plugin (no Python): runs anywhere");
    } else if (!strcmp(v, "host")) {
        snprintf(summary, sizeof(summary), "runs in the plugin host (plugins.isolation: host)");
    } else {
        size_t o = (size_t)snprintf(summary, sizeof(summary), "in-process only: ");
        int first = 1;
        if (reasons && reasons->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(reasons); i++) {
                json_t *e = json_get(reasons, i);
                if (!e || e->type != JSON_STRING) continue;
                if (o >= sizeof(summary) - 1) break;
                o += (size_t)snprintf(summary + o, sizeof(summary) - o, "%s%s",
                                      first ? "" : "; ", e->str_val ? e->str_val : "");
                first = 0;
            }
        }
    }

    json_t *out = json_object();
    json_set(out, "dict", dict);
    json_set(out, "summary", json_string(summary));
    return out;
}
