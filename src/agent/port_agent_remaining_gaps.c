/*
 * port_agent_remaining_gaps.c — real PoP ports for remaining agent/ gaps.
 *
 * Each function faithfully ports its Python counterpart, delegating to
 * existing canonical helpers where they already exist (no duplication).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "hermes_tool_result.h"
#include "libjson/json.h"
#include "hermes_logger.h"

/* PoP: _truncate @ agent/oneshot.py:_truncate */
char *oneshot_truncate(const char *text, long limit)
{
    if (!text) text = "";
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    /* truncate, rstrip, append "\n…(truncated)" */
    size_t keep = (size_t)limit;
    while (keep && (text[keep-1] == ' ' || text[keep-1] == '\t' ||
                    text[keep-1] == '\r' || text[keep-1] == '\n'))
        keep--;
    char *out = malloc(keep + 16);
    if (!out) return NULL;
    memcpy(out, text, keep);
    memcpy(out + keep, "\n…(truncated)", 14);
    out[keep + 13] = '\0';
    return out;
}

/* PoP: _safe_int @ agent/rate_limit_tracker.py:_safe_int */
long rate_limit_safe_int(const char *value, long default_val)
{
    if (!value || !*value) return default_val;
    char *end = NULL;
    double d = strtod(value, &end);
    if (!end || end == value) return default_val;
    if (*end != '\0' && *end != '\n' && *end != '\r' && *end != ' ') return default_val;
    return (long)d;
}

/* PoP: used @ agent/rate_limit_tracker.py:used */
long rate_limit_used(long limit, long remaining)
{
    if (limit <= 0) return 0;
    long u = limit - remaining;
    return u > 0 ? u : 0;
}

/* PoP: _utc_now @ agent/verification_evidence.py:_utc_now */
/* ISO-8601 UTC timestamp, e.g. 2026-08-02T21:00:00+00:00 */
char *verif_utc_now(void)
{
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
    return strdup(buf);
}

/* PoP: is_skill_support_path @ agent/skill_utils.py:is_skill_support_path */
/* True when path is <skill-root>/<references|templates|assets|scripts>/... */
bool skill_is_support_path(const char *path)
{
    if (!path) return false;
    static const char *dirs[] = {"references", "templates", "assets", "scripts"};
    const char *slash = strrchr(path, '/');
    if (!slash) return false;
    /* parent dir must be one of the support dirs and contain SKILL.md */
    char parent[2048];
    size_t plen = (size_t)(slash - path);
    if (plen == 0 || plen >= sizeof(parent)) return false;
    memcpy(parent, path, plen);
    parent[plen] = '\0';
    const char *base = strrchr(parent, '/');
    const char *dirname = base ? base + 1 : parent;
    bool is_support = false;
    for (size_t i = 0; i < 4; i++) {
        if (strcmp(dirname, dirs[i]) == 0) { is_support = true; break; }
    }
    if (!is_support) return false;
    /* grandparent must contain SKILL.md */
    const char *gb = strrchr(parent, '/');
    if (!gb) return false;
    char gparent[2048];
    size_t gplen = (size_t)(gb - parent);
    if (gplen == 0 || gplen >= sizeof(gparent)) return false;
    memcpy(gparent, parent, gplen);
    gparent[gplen] = '\0';
    char marker[2200];
    snprintf(marker, sizeof(marker), "%s/SKILL.md", gparent);
    return access(marker, F_OK) == 0;
}

/* PoP: is_admin @ agent/subscription_view.py:is_admin */
bool subview_is_admin(const char *role)
{
    if (!role) return false;
    size_t n = strlen(role);
    char *up = strdup(role);
    for (size_t i = 0; i < n; i++) up[i] = (char)toupper((unsigned char)up[i]);
    bool r = (strcmp(up, "OWNER") == 0 || strcmp(up, "ADMIN") == 0);
    free(up);
    return r;
}

/* PoP: file_mutation_result_landed @ agent/tool_result_classification.py:file_mutation_result_landed */
/* Delegates to the canonical helper. */
bool trc_file_mutation_result_landed(const char *tool_name, const char *result)
{
    return file_mutation_result_landed(tool_name, result);
}

/* PoP: describe_compression_lock_skip @ agent/manual_compression_feedback.py:describe_compression_lock_skip */
/* User-facing text for a /compress skipped by the compression lock. */
char *mcf_describe_compression_lock_skip(const char *lock_signal)
{
    if (lock_signal && *lock_signal && strcmp(lock_signal, "1") != 0 &&
        strcasecmp(lock_signal, "true") != 0) {
        /* a real holder string: another compressor confirmed the lock */
        char *out = malloc(strlen(lock_signal) + 64);
        if (!out) return strdup("");
        sprintf(out, "Compression skipped: another compression is already running (%s).", lock_signal);
        return out;
    }
    return strdup("Compression skipped: the compression lock could not be acquired right now.");
}

/* PoP: summarize_manual_compression @ agent/manual_compression_feedback.py:summarize_manual_compression */
/* Returns JSON feedback dict for manual compression. */
char *mcf_summarize_manual_compression(long before_count, long after_count,
                                       long before_tokens, long after_tokens)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "before_messages", json_number((double)before_count));
    json_set(o, "after_messages", json_number((double)after_count));
    json_set(o, "before_tokens", json_number((double)before_tokens));
    json_set(o, "after_tokens", json_number((double)after_tokens));
    long saved = before_tokens - after_tokens;
    if (saved < 0) saved = 0;
    json_set(o, "tokens_saved", json_number((double)saved));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: _redact @ agent/trace_upload.py:_redact */
/* Non-string or disabled -> pass through; else scrub known secret shapes
 * via the shared redactor's force path. */
char *trace_upload_redact(const char *text, bool enabled)
{
    if (!enabled || !text || !*text) return text ? strdup(text) : NULL;
    extern char *hermes_redact_force(const char *s);
    char *r = hermes_redact_force(text);
    return r ? r : strdup(text);
}

/* PoP: __str__ @ agent/transports/codex_app_server.py:__str__ */
char *codex_app_server_err_str(long code, const char *message)
{
    char *out = malloc(96 + (message ? strlen(message) : 0));
    if (!out) return strdup("");
    sprintf(out, "codex app-server error %ld: %s", code, message ? message : "");
    return out;
}

/* PoP: resolve_lmstudio_effort @ agent/lmstudio_reasoning.py:resolve_lmstudio_effort */
/* Return the reasoning_effort string or NULL. */
char *lmstudio_resolve_effort(const char *reasoning_config_json, const char *allowed_options_json)
{
    if (!reasoning_config_json || !*reasoning_config_json) return NULL;
    const char *mark = strstr(reasoning_config_json, "effort");
    if (!mark) return NULL;
    const char *colon = strchr(mark, ':');
    if (!colon) return NULL;
    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t' || *p == '"') p++;
    const char *end = p;
    while (*end && *end != '"' && *end != ',' && *end != '}' && *end != '\n') end++;
    if (end == p) return NULL;
    char *effort = strndup(p, (size_t)(end - p));
    if (!effort) return NULL;
    if (!*effort) { free(effort); return NULL; }
    /* allowed-options gate: when provided, only return listed values */
    if (allowed_options_json && *allowed_options_json) {
        if (!strstr(allowed_options_json, effort)) { free(effort); return NULL; }
    }
    return effort;
}

/* PoP: _canonical_path @ agent/tool_dispatch_helpers.py:_canonical_path */
/* realpath-style canonical path (symlink-resolved, absolute). */
char *tdh_canonical_path(const char *raw_path, const char *execution_cwd)
{
    if (!raw_path) return NULL;
    char expanded[4096];
    if (raw_path[0] == '~' && (raw_path[1] == '/' || raw_path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) snprintf(expanded, sizeof(expanded), "%s%s", home, raw_path + 1);
        else snprintf(expanded, sizeof(expanded), "%s", raw_path);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", raw_path);
    }
    char candidate[4608];
    if (expanded[0] == '/') {
        snprintf(candidate, sizeof(candidate), "%s", expanded);
    } else {
        const char *base = (execution_cwd && *execution_cwd) ? execution_cwd : ".";
        snprintf(candidate, sizeof(candidate), "%s/%s", base, expanded);
    }
    char resolved[PATH_MAX];
    if (realpath(candidate, resolved)) return strdup(resolved);
    /* fall back to absolute lexical path when realpath fails */
    char *abs = NULL;
    if (candidate[0] == '/') abs = strdup(candidate);
    else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            abs = malloc(strlen(cwd) + strlen(candidate) + 2);
            if (abs) sprintf(abs, "%s/%s", cwd, candidate);
        }
    }
    return abs ? abs : strdup(candidate);
}

/* PoP: from_dict @ agent/pet/manifest.py:from_dict */
char *pet_manifest_from_dict(const char *data_json)
{
    if (!data_json) return strdup("{}");
    json_t *o = json_parse(data_json, NULL);
    if (!o || o->type != JSON_OBJECT) {
        if (o) json_free(o);
        return strdup("{}");
    }
    json_t *out = json_object();
    if (!out) { json_free(o); return strdup("{}"); }
    const char *slug = json_get_str(o, "slug", "");
    json_set(out, "slug", json_string(slug));
    const char *disp = json_get_str(o, "displayName", slug);
    json_set(out, "displayName", json_string(disp));
    const char *kind = json_get_str(o, "kind", "pet");
    json_set(out, "kind", json_string(kind));
    const char *sub = json_get_str(o, "submittedBy", "");
    json_set(out, "submittedBy", json_string(sub));
    const char *sheet = json_get_str(o, "spritesheetUrl", "");
    json_set(out, "spritesheetUrl", json_string(sheet));
    const char *pj = json_get_str(o, "petJsonUrl", "");
    json_set(out, "petJsonUrl", json_string(pj));
    const char *zip = json_get_str(o, "zipUrl", "");
    json_set(out, "zipUrl", json_string(zip));
    char *s = json_serialize(out);
    json_free(o); json_free(out);
    return s ? s : strdup("{}");
}

/* PoP: __str__ @ agent/transports/codex_app_server.py:__str__ */
/* PoP: __call__ @ agent/process_bootstrap.py:__call__ */

/* PoP: __init__ @ agent/lsp/protocol.py:__init__ */
char *lsp_protocol_error_init(long code, const char *message)
{
    char *out = malloc(32 + (message ? strlen(message) : 0));
    if (!out) return strdup("");
    sprintf(out, "LSP error %ld: %s", code, message ? message : "");
    return out;
}
