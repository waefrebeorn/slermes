/*
 * shared_metrics_prepared_compaction.c - C11 port of upstream
 * hermes_cli/observability/shared_metrics_prepared_compaction.py
 * (gate w23_sharedmetr_68ac2c).
 *
 * PoP: w23_sharedmetr_68ac2c_metric @ hermes_cli/observability/shared_metrics_prepared_compaction.py:PREPARED_COMPACTION_METRIC
 * PoP: w23_sharedmetr_68ac2c_truthy @ utils/env.py:is_truthy_value
 * PoP: w23_sharedmetr_68ac2c_prepare_ahead_state @ hermes_cli/observability/shared_metrics_prepared_compaction.py:prepare_ahead_state
 * PoP: w23_sharedmetr_68ac2c_wait_bucket @ hermes_cli/observability/shared_metrics_prepared_compaction.py:wait_bucket
 * PoP: w23_sharedmetr_68ac2c_fields @ hermes_cli/observability/shared_metrics_prepared_compaction.py:prepared_compaction_fields
 *
 * spc_closed() is the module's ``_closed`` and spc_failure_class() is
 * shared_metrics_fields.compression_failure_class("failed", ...): those two are
 * the only places a raw runtime value may reach a metric dimension, so they are
 * reproduced exactly rather than abstracted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sharedmetr_68ac2c.h"

#define SPC_TEXT_CAP 256

/* ── closed sets (transcribed from the quarry) ───────────────────────────── */

static const char *const SPC_EVENTS[] = {
    "auto_disabled", "compaction_failed", "discarded", "internal_error", "pass_cancelled",
    "pass_completed", "pass_failed", "pass_started", "skipped_busy", "used",
};
static const char *const SPC_ORIGINS[] = {"none", "tool_batch", "turn_end"};
static const char *const SPC_REASONS[] = {
    "agent_released", "aux_fallback_main", "compaction_aborted", "fresh_required", "head_moved",
    "host_cancelled", "none", "other_session", "pass_failed_while_waiting", "provider_context",
    "rewound", "running_not_fit", "session_boundary", "shape_changed", "superseded", "text_changed",
    "transcript_shorter", "wait_timeout",
};
/* COMPRESSION_FAILURE_CLASSES minus {none, unknown}: the set the builder admits. */
static const char *const SPC_FAILURE_CLASSES[] = {
    "attempt_superseded", "aux_model_fallback", "commit_fence_cancelled", "empty_post_handoff_window",
    "empty_transcript", "exception", "explicit_interrupt", "feasibility_skip", "insufficient_messages",
    "lock_contended", "no_compressible_window", "no_progress", "other", "pool_saturated", "rollback",
    "session_split_failed", "snapshot_stale", "stall_deterministic_fallback", "stall_interrupted",
    "summary_auth_failure", "summary_empty_content_failure", "summary_generation_aborted",
    "summary_generation_failed", "summary_network_failure", "summary_overload_degraded",
    "summary_overload_failure", "summary_truncated_failure", "would_grow",
};

#define SPC_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Python truthiness of a parsed JSON value: None/False/0/""/[]/{} are false. */
static int spc_falsy(const json_t *v)
{
    if (!v) return 1;
    switch (v->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return v->bool_val ? 0 : 1;
    case JSON_NUMBER: return v->num_val == 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 0 : 1;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count == 0 ? 1 : 0;
    default: return 0;
    }
}

/* Python str() of a scalar JSON value; containers never match a closed set. */
static void spc_py_str(const json_t *v, char *buf, size_t cap)
{
    buf[0] = '\0';
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        if (v->str_val) {
            size_t n = strlen(v->str_val);
            if (n >= cap) n = cap - 1;
            memcpy(buf, v->str_val, n);
            buf[n] = '\0';
        }
        return;
    case JSON_BOOL:
        snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER: {
        double d = v->num_val;
        long long n = (long long)d;
        if ((double)n == d && d > -1e18 && d < 1e18) snprintf(buf, cap, "%lld", n);
        else snprintf(buf, cap, "%g", d);
        return;
    }
    case JSON_ARRAY:
        snprintf(buf, cap, "[]");
        return;
    case JSON_OBJECT:
        snprintf(buf, cap, "{}");
        return;
    default:
        return;
    }
}

static int spc_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

/* str(value or "").strip().lower(), plus "-" -> "_" when dash is set. */
static void spc_normalize(const json_t *v, char *buf, size_t cap, int dash)
{
    size_t i, len, start = 0, end;
    if (spc_falsy(v)) { buf[0] = '\0'; return; }
    spc_py_str(v, buf, cap);
    len = strlen(buf);
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)buf[i];
        if (ch >= 'A' && ch <= 'Z') buf[i] = (char)(ch + ('a' - 'A'));
        else if (dash && ch == '-') buf[i] = '_';
    }
    end = len;
    while (start < end && spc_space(buf[start])) start++;
    while (end > start && spc_space(buf[end - 1])) end--;
    if (start > 0) memmove(buf, buf + start, end - start);
    buf[end - start] = '\0';
}

/* _closed(value, allowed, fallback): a normalized text is a member or nothing. */
static const char *spc_closed(const json_t *v, const char *const *set, size_t n, const char *fallback)
{
    char buf[SPC_TEXT_CAP];
    size_t i;
    if (spc_falsy(v)) return fallback;
    if (v->type == JSON_STRING && v->str_val && strlen(v->str_val) >= SPC_TEXT_CAP) return fallback;
    spc_normalize(v, buf, sizeof(buf), 1);
    if (!buf[0]) return fallback;
    for (i = 0; i < n; i++)
        if (strcmp(buf, set[i]) == 0) return set[i];
    return fallback;
}

/* compression_failure_class("failed", failure_class). */
static const char *spc_failure_class(const json_t *v)
{
    char buf[SPC_TEXT_CAP];
    char *colon;
    size_t i;
    if (!v || v->type == JSON_NULL) return "none";
    /* a non-string drops to None before _norm, so its text is "" -> "unknown" */
    if (v->type != JSON_STRING) return "unknown";
    if (v->str_val && strlen(v->str_val) >= SPC_TEXT_CAP) return "other";
    spc_normalize(v, buf, sizeof(buf), 0);
    colon = strchr(buf, ':');
    if (colon) *colon = '\0';
    if (!buf[0]) return "unknown";
    for (i = 0; i < SPC_COUNT(SPC_FAILURE_CLASSES); i++)
        if (strcmp(buf, SPC_FAILURE_CLASSES[i]) == 0) return SPC_FAILURE_CLASSES[i];
    return "other";
}

const char *w23_sharedmetr_68ac2c_metric(void)
{
    return "hermes.prepared_compaction.count";
}

int w23_sharedmetr_68ac2c_truthy(const json_t *value)
{
    char buf[SPC_TEXT_CAP];
    if (!value || value->type == JSON_NULL) return 0;
    if (value->type == JSON_STRING) {
        spc_normalize(value, buf, sizeof(buf), 0);
        if (!strcmp(buf, "1") || !strcmp(buf, "true") || !strcmp(buf, "yes") || !strcmp(buf, "on")) return 1;
        return 0;
    }
    return spc_falsy(value) ? 0 : 1;
}

const char *w23_sharedmetr_68ac2c_prepare_ahead_state(const json_t *config)
{
    const json_t *compression, *value;
    if (!config || config->type != JSON_OBJECT) return "off";
    compression = json_obj_get(config, "compression");
    if (!compression || compression->type != JSON_OBJECT) return "off";
    value = json_obj_get(compression, "prepare_ahead");
    return w23_sharedmetr_68ac2c_truthy(value) ? "on" : "off";
}

const char *w23_sharedmetr_68ac2c_wait_bucket(const json_t *seconds)
{
    double s;
    if (!seconds || seconds->type == JSON_BOOL) return "none";
    if (seconds->type != JSON_NUMBER) return "none";
    s = seconds->num_val;
    if (s < 0.0) return "none";
    if (s < 1.0) return "lt_1s";
    if (s < 5.0) return "1s_to_5s";
    if (s < 30.0) return "5s_to_30s";
    if (s < 120.0) return "30s_to_120s";
    return "gte_120s";
}

json_t *w23_sharedmetr_68ac2c_fields(const json_t *req)
{
    const json_t *event, *origin, *reason, *failure, *wait;
    const char *name;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    event = json_obj_get(req, "event");
    origin = json_obj_get(req, "origin");
    reason = json_obj_get(req, "reason");
    failure = json_obj_get(req, "failure_class");
    wait = json_obj_get(req, "wait_seconds");

    name = spc_closed(event, SPC_EVENTS, SPC_COUNT(SPC_EVENTS), "");
    if (!name[0]) return json_null();

    out = json_object();
    json_set(out, "event", json_string(name));
    json_set(out, "failure_class", json_string(spc_failure_class(failure)));
    json_set(out, "origin", json_string(spc_closed(origin, SPC_ORIGINS, SPC_COUNT(SPC_ORIGINS), "none")));
    json_set(out, "reason", json_string(spc_closed(reason, SPC_REASONS, SPC_COUNT(SPC_REASONS), "none")));
    json_set(out, "wait_bucket", json_string(w23_sharedmetr_68ac2c_wait_bucket(wait)));
    return out;
}
