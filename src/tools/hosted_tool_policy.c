/*
 * hosted_tool_policy.c — C port of upstream Hermes tools/hosted_tool_policy.py.
 *
 * PoP: w11_hostedtool_48c71a_message @ tools/hosted_tool_policy.py:_message
 * PoP: w11_hostedtool_48c71a_tool_refusal @ tools/hosted_tool_policy.py:hosted_tool_refusal
 * PoP: w11_hostedtool_48c71a_feature_enabled @ tools/hosted_tool_policy.py:host_feature_enabled
 * PoP: w11_hostedtool_48c71a_feature_refusal @ tools/hosted_tool_policy.py:host_feature_refusal
 * PoP: w11_hostedtool_48c71a_require @ tools/hosted_tool_policy.py:require_host_feature
 *
 * The _hosted() probe is injected as an explicit `hosted` argument: on Windows the
 * CRT setenv(name,"",1) unsets a variable while Python keeps it, so the oracle
 * feeds the value from the fixture instead of mutating the process environment.
 */
#include "noc_w11_hostedtool_48c71a.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* frozenset HOSTED_TOOL_ALLOWLIST, sorted by _message(): */
static const char *const ALLOWED[] = {
    "execute_code", "patch", "process_manage", "read_file",
    "search_files", "terminal", "write_file",
};
static const size_t N_ALLOWED = sizeof(ALLOWED) / sizeof(ALLOWED[0]);

/* frozenset HOST_FEATURES (membership only; order is irrelevant). */
static const char *const FEATURES[] = {
    "context_files", "subdirectory_hints", "context_references", "workspace_snapshot",
    "backend_probe", "builtin_memory", "session_recall", "background_review",
    "trajectory_export", "file_mutation_bookkeeping", "checkpoints", "media_processing",
    "skills", "skill_inline_shell", "cron_scripts", "shell_hooks",
    "tool_host_artifacts", "mcp_stdio", "media_delivery", "transcription", "delegation",
};
static const size_t N_FEATURES = sizeof(FEATURES) / sizeof(FEATURES[0]);

static int in_list(const char *const *list, size_t n, const char *s) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++)
        if (!strcmp(list[i], s)) return 1;
    return 0;
}

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static json_t *message(const char *what, const char *why) {
    char joined[1024];
    size_t pos = 0;
    joined[0] = '\0';
    for (size_t i = 0; i < N_ALLOWED; i++) {
        int n = snprintf(joined + pos, sizeof(joined) - pos, "%s%s",
                         i ? ", " : "", ALLOWED[i]);
        if (n < 0 || (size_t)n >= sizeof(joined) - pos) break;
        pos += (size_t)n;
    }
    char buf[8192];
    snprintf(buf, sizeof(buf),
             "%s is not available to a hosted agent: %s. A hosted agent's tools are %s, all run in its own sandbox.",
             what ? what : "", why ? why : "", joined);
    return json_string(buf);
}

json_t *w11_hostedtool_48c71a_message(const char *what, const char *why) {
    return message(what, why);
}

json_t *w11_hostedtool_48c71a_tool_refusal(const char *tool_name, int hosted) {
    if (!hosted || in_list(ALLOWED, N_ALLOWED, tool_name)) return json_null();
    return message(tool_name, "only tools that act solely through the agent's sandbox are enabled");
}

/* Python repr() of a plain ASCII feature name for the KeyError message. */
static json_t *unknown_feature(const char *feature) {
    char buf[1024];
    const char *f = feature ? feature : "";
    int needs_repr_escape = 0;
    for (const char *p = f; *p; p++)
        if (*p == '\'' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') {
            needs_repr_escape = 1;
            break;
        }
    if (!needs_repr_escape)
        snprintf(buf, sizeof(buf), "unknown host feature '%s'", f);
    else {
        char esc[512];
        size_t e = 0;
        esc[e++] = '\'';
        for (const char *p = f; *p && e < sizeof(esc) - 4; p++) {
            char c = *p;
            if (c == '\'' || c == '\\') { esc[e++] = '\\'; esc[e++] = c; }
            else if (c == '\n') { esc[e++] = '\\'; esc[e++] = 'n'; }
            else if (c == '\r') { esc[e++] = '\\'; esc[e++] = 'r'; }
            else if (c == '\t') { esc[e++] = '\\'; esc[e++] = 't'; }
            else esc[e++] = c;
        }
        esc[e < sizeof(esc) ? e : sizeof(esc) - 1] = '\'';
        esc[e + 1 < sizeof(esc) ? e + 1 : sizeof(esc) - 1] = '\0';
        snprintf(buf, sizeof(buf), "unknown host feature %s", esc);
    }
    return raise_obj("KeyError", buf);
}

/* Returns 0 and sets *err for an unknown feature; else the enabled bit. */
static int feature_enabled_raw(const char *feature, int hosted, json_t **err) {
    if (!in_list(FEATURES, N_FEATURES, feature)) {
        *err = unknown_feature(feature);
        return 0;
    }
    return !hosted;
}

json_t *w11_hostedtool_48c71a_feature_enabled(const char *feature, int hosted) {
    json_t *err = NULL;
    int v = feature_enabled_raw(feature, hosted, &err);
    if (err) return err;
    return json_bool(v ? true : false);
}

static json_t *feature_refusal_raw(const char *feature, const char *what, int hosted, json_t **err) {
    int enabled = feature_enabled_raw(feature, hosted, err);
    if (*err) return NULL;
    if (enabled) return json_null();
    char why[1024];
    char norm[512];
    size_t n = 0;
    for (const char *p = feature; *p && n + 1 < sizeof(norm); p++)
        norm[n++] = (*p == '_') ? ' ' : *p;
    norm[n] = '\0';
    snprintf(why, sizeof(why), "it uses this host's %s", norm);
    return message(what, why);
}

json_t *w11_hostedtool_48c71a_feature_refusal(const char *feature, const char *what, int hosted) {
    json_t *err = NULL;
    json_t *r = feature_refusal_raw(feature, what, hosted, &err);
    if (err) return err;
    return r ? r : json_null();
}

json_t *w11_hostedtool_48c71a_require(const char *feature, const char *what, int hosted) {
    json_t *err = NULL;
    json_t *refusal = feature_refusal_raw(feature, what, hosted, &err);
    if (err) return err;
    if (refusal && refusal->type != JSON_NULL) {
        const char *m = (refusal->type == JSON_STRING) ? refusal->str_val : "";
        char *copy = m ? strdup(m) : NULL; /* copy BEFORE json_free */
        json_t *out = raise_obj("HostedToolRefused", copy);
        free(copy);
        json_free(refusal);
        return out;
    }
    json_free(refusal);
    return json_null();
}
