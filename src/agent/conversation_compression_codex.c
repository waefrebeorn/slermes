/*
 * conversation_compression_codex.c -- real C11 port of upstream Hermes
 *   agent/conversation_compression_codex.py.
 *
 * PoP: w20_conversati_9e4fef_auto_mode @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_tokens_label @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_cooldown_remaining @ agent/conversation_compression_codex.py:_codex_compaction_cooldown_remaining
 * PoP: w20_conversati_9e4fef_skip_decision @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_failed @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_failure_message @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_failure_error @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_status_text @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * PoP: w20_conversati_9e4fef_plan @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 *
 * The call itself is bound to a live agent, a live Codex session and the
 * compaction heartbeat/telemetry sinks; the port reproduces its decision, its
 * three log lines and every observable side effect with those capabilities
 * supplied as data instead of being invoked.
 */
#include "hermes_json.h"
#include "noc_w20_conversati_9e4fef.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONV9_COOLDOWN_SECONDS 600

static char *conv9_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int conv9_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int conv9_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* str() of a decoded JSON scalar (None -> "None", True -> "True"). */
static char *conv9_str_py(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return conv9_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return conv9_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return conv9_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (conv9_is_py_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return conv9_dup(buf);
    default:
        return conv9_dup("");
    }
}

/* Python's f"{n:,}". */
static char *conv9_thousands(long long value) {
    char digits[32], out[48];
    int neg = value < 0, n = 0, i, w = 0, right;
    unsigned long long mag = neg ? (unsigned long long)(-(value + 1)) + 1ULL : (unsigned long long)value;
    do {
        digits[n++] = (char)('0' + (int)(mag % 10ULL));
        mag /= 10ULL;
    } while (mag);
    if (neg) out[w++] = '-';
    for (i = n - 1; i >= 0; i--) {
        out[w++] = digits[i];
        right = i;                        /* digits still to the right of this one */
        if (right > 0 && right % 3 == 0) out[w++] = ',';
    }
    out[w] = '\0';
    return conv9_dup(out);
}

/* --- auto_mode ------------------------------------------------------------ */

char *w20_conversati_9e4fef_auto_mode(const char *raw) {
    char *lowered, *p;
    if (!raw || !raw[0]) return conv9_dup("native");
    lowered = conv9_dup(raw);
    if (!lowered) return NULL;
    for (p = lowered; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    if (!strcmp(lowered, "native") || !strcmp(lowered, "hermes") || !strcmp(lowered, "off")) {
        return lowered;
    }
    free(lowered);
    return conv9_dup("native");
}

/* --- tokens label --------------------------------------------------------- */

char *w20_conversati_9e4fef_tokens_label(const json_t *approx_tokens) {
    if (!conv9_truthy(approx_tokens)) return conv9_dup("unknown");
    if (approx_tokens->type == JSON_BOOL) return conv9_dup(approx_tokens->bool_val ? "1" : "unknown");
    if (approx_tokens->type == JSON_NUMBER && conv9_is_py_int(approx_tokens)) {
        return conv9_thousands((long long)approx_tokens->num_val);
    }
    return conv9_dup("unknown");
}

/* --- cooldown ------------------------------------------------------------- */

json_t *w20_conversati_9e4fef_cooldown_remaining(const json_t *agent) {
    json_t *compressor, *getter, *state, *remaining;
    double value = 0.0;
    if (!agent || agent->type != JSON_OBJECT) return json_number(0.0);
    compressor = json_obj_get(agent, "compressor");
    if (!compressor || compressor->type != JSON_OBJECT) return json_number(0.0);
    getter = json_obj_get(compressor, "has_getter");
    if (!conv9_truthy(getter)) return json_number(0.0);          /* not callable */
    if (conv9_truthy(json_obj_get(compressor, "raises"))) return json_number(0.0);
    state = json_obj_get(compressor, "state");
    if (!conv9_truthy(state)) return json_number(0.0);           /* `... if state else 0.0` */
    if (state->type != JSON_OBJECT) return json_number(0.0);     /* .get would AttributeError */
    remaining = json_obj_get(state, "remaining_seconds");
    if (!conv9_truthy(remaining)) return json_number(0.0);       /* `or 0.0` */
    switch (remaining->type) {
    case JSON_NUMBER: value = remaining->num_val; break;
    case JSON_BOOL:   value = remaining->bool_val ? 1.0 : 0.0; break;
    case JSON_STRING: {
        char *end = NULL;
        const char *p = remaining->str_val ? remaining->str_val : "";
        if (!*p) return json_number(0.0);
        value = strtod(p, &end);
        if (end == p || *end != '\0') return json_number(0.0);   /* ValueError */
        break;
    }
    default: return json_number(0.0);                            /* TypeError */
    }
    if (!isfinite(value)) return json_number(0.0);
    return json_number(value < 0.0 ? 0.0 : value);
}

/* --- skip decision -------------------------------------------------------- */

json_t *w20_conversati_9e4fef_skip_decision(const json_t *in) {
    int force, has_session;
    const char *mode = "native";
    double cooldown = 0.0;
    char reason[128];

    if (!in || in->type != JSON_OBJECT) in = NULL;
    force = in ? conv9_truthy(json_obj_get(in, "force")) : 0;
    has_session = in ? conv9_truthy(json_obj_get(in, "has_session")) : 0;
    if (in) {
        const char *m = json_get_str(in, "auto_mode", "native");
        mode = m ? m : "native";
        cooldown = json_get_num(in, "cooldown", 0.0);
    }
    if (cooldown < 0.0) cooldown = 0.0;

    if (!force && strcmp(mode, "hermes")) {
        json_t *out = json_object();
        snprintf(reason, sizeof(reason), "mode=%s force=false", mode);
        json_set(out, "reason", json_string(reason));
        json_set(out, "status", json_string("skipped"));
        snprintf(reason, sizeof(reason), "codex_auto_%s", mode);
        json_set(out, "class", json_string(reason));
        return out;
    }
    if (!force && cooldown > 0.0) {
        json_t *out = json_object();
        snprintf(reason, sizeof(reason), "failure cooldown active for %.0fs", cooldown);
        json_set(out, "reason", json_string(reason));
        json_set(out, "status", json_string("blocked"));
        json_set(out, "class", json_string("blocked:cooldown"));
        return out;
    }
    if (!has_session) {
        json_t *out = json_object();
        json_set(out, "reason", json_string("no active codex thread"));
        json_set(out, "status", json_string("skipped"));
        json_set(out, "class", json_string("codex_no_thread"));
        return out;
    }
    return json_null();
}

/* --- result predicates ---------------------------------------------------- */

int w20_conversati_9e4fef_failed(const json_t *result) {
    if (!result || result->type != JSON_OBJECT) return 0;
    if (conv9_truthy(json_obj_get(result, "interrupted"))) return 1;
    if (conv9_truthy(json_obj_get(result, "error"))) return 1;
    return 0;
}

char *w20_conversati_9e4fef_failure_message(const json_t *result) {
    char *err = conv9_str_py(result && result->type == JSON_OBJECT
                             ? json_obj_get(result, "error") : NULL);
    char *out;
    size_t n = strlen("\xe2\x9a\xa0 Codex app-server compaction failed: ")
             + strlen(err ? err : "") + 1;
    out = (char *)malloc(n);
    if (out) snprintf(out, n, "\xe2\x9a\xa0 Codex app-server compaction failed: %s", err ? err : "");
    free(err);
    return out;
}

char *w20_conversati_9e4fef_failure_error(const json_t *result) {
    json_t *err = result && result->type == JSON_OBJECT ? json_obj_get(result, "error") : NULL;
    if (!conv9_truthy(err)) return conv9_dup("compaction interrupted");
    return conv9_str_py(err);
}

const char *w20_conversati_9e4fef_status_text(int failed) {
    return failed ? "context compression failed" : "context compression completed";
}

/* --- one whole call ------------------------------------------------------- */

static void conv9_push_log(json_t *logs, const char *line) {
    json_append(logs, json_string(line));
}

static json_t *conv9_record(const char *status, const char *cls, const char *method) {
    json_t *rec = json_object();
    json_set(rec, "commit_status", json_string(status ? status : ""));
    json_set(rec, "failure_class", cls ? json_string(cls) : json_null());
    json_set(rec, "route", json_string("codex_app_server"));
    json_set(rec, "method", json_string(method ? method : "none"));
    return rec;
}

json_t *w20_conversati_9e4fef_plan(const json_t *in) {
    json_t *out, *logs, *result, *skip;
    char *mode, *tokens, *sid;
    const char *raw_mode, *raise_type, *task_id;
    int force, has_session, message_count, has_update, cooldown_recorder;
    double cooldown;
    char line[512];

    if (!in || in->type != JSON_OBJECT) in = NULL;

    force = in ? conv9_truthy(json_obj_get(in, "force")) : 0;
    has_session = in ? conv9_truthy(json_obj_get(in, "has_session")) : 0;
    has_update = in ? conv9_truthy(json_obj_get(in, "has_update_from_response")) : 0;
    cooldown_recorder = in ? conv9_truthy(json_obj_get(in, "cooldown_recorder")) : 0;
    cooldown = in ? json_get_num(in, "cooldown", 0.0) : 0.0;
    if (cooldown < 0.0) cooldown = 0.0;
    raw_mode = in ? json_get_str(in, "auto_mode", "native") : "native";
    raise_type = in ? json_get_str(in, "raise_type", "RuntimeError") : "RuntimeError";
    task_id = in ? json_get_str(in, "task_id", "default") : "default";
    mode = w20_conversati_9e4fef_auto_mode(raw_mode);
    tokens = w20_conversati_9e4fef_tokens_label(in ? json_obj_get(in, "approx_tokens") : NULL);
    {
        const char *s = in ? json_get_str(in, "session_id", "") : "";
        sid = conv9_dup((s && s[0]) ? s : "none");
    }
    result = in ? json_obj_get(in, "result") : NULL;
    if (result && result->type != JSON_OBJECT) result = NULL;
    message_count = 0;
    if (in) {
        json_t *messages = json_obj_get(in, "messages");
        if (messages && messages->type == JSON_ARRAY) message_count = (int)json_len(messages);
    }

    out = json_object();
    logs = json_array();
    json_set(out, "auto_mode", json_string(mode));
    json_set(out, "session_id", json_string(sid));
    json_set(out, "tokens", json_string(tokens));
    json_set(out, "record", json_null());
    json_set(out, "heartbeat_stop", json_null());
    json_set(out, "warning", json_null());
    json_set(out, "cooldown_record", json_null());
    json_set(out, "retired", json_bool(0));
    json_set(out, "closed", json_bool(0));
    json_set(out, "bookkeeping", json_bool(0));
    json_set(out, "usage", json_bool(0));
    json_set(out, "dedup_reset", json_null());
    json_set(out, "exception", json_null());

    /* -- the skipper ------------------------------------------------------- */
    {
        json_t *decision_in = json_object();
        json_set(decision_in, "force", json_bool(force));
        json_set(decision_in, "auto_mode", json_string(mode));
        json_set(decision_in, "cooldown", json_number(cooldown));
        json_set(decision_in, "has_session", json_bool(has_session));
        skip = w20_conversati_9e4fef_skip_decision(decision_in);
        json_free(decision_in);
    }

    if (skip && skip->type == JSON_OBJECT) {
        const char *reason = json_get_str(skip, "reason", "");
        const char *status = json_get_str(skip, "status", "");
        const char *cls = json_get_str(skip, "class", "");
        snprintf(line, sizeof(line),
                 "codex app-server compaction skipped: %s (session=%s messages=%d tokens=~%s)",
                 reason, sid, message_count, tokens);
        conv9_push_log(logs, line);
        json_set(out, "record", conv9_record(status, cls, "none"));
        json_set(out, "logs", logs);
        json_free(skip);
        free(mode);
        free(tokens);
        free(sid);
        return out;
    }
    json_free(skip);

    snprintf(line, sizeof(line),
             "codex app-server compaction started: session=%s messages=%d tokens=~%s",
             sid, message_count, tokens);
    conv9_push_log(logs, line);

    /* -- compact_thread ---------------------------------------------------- */
    if (!result) {
        /* raised before any result was produced */
        char cls[160];
        json_set(out, "heartbeat_stop", json_string("context compression failed"));
        snprintf(cls, sizeof(cls), "exception:%s", raise_type ? raise_type : "RuntimeError");
        json_set(out, "record", conv9_record("aborted", cls, "none"));
        json_set(out, "exception", json_string(raise_type ? raise_type : "RuntimeError"));
        json_set(out, "logs", logs);
        free(mode);
        free(tokens);
        free(sid);
        return out;
    }

    {
        int failed = w20_conversati_9e4fef_failed(result);
        int retire = conv9_truthy(json_obj_get(result, "should_retire"));
        json_set(out, "heartbeat_stop", json_string(w20_conversati_9e4fef_status_text(failed)));
        json_set(out, "retired", json_bool(retire ? 1 : 0));
        json_set(out, "closed", json_bool(retire ? 1 : 0));
        if (failed) {
            char *msg = w20_conversati_9e4fef_failure_message(result);
            char *err = w20_conversati_9e4fef_failure_error(result);
            json_set(out, "warning", json_string(msg ? msg : ""));
            if (cooldown_recorder) {
                json_t *rec = json_array();
                json_append(rec, json_number(CONV9_COOLDOWN_SECONDS));
                json_append(rec, json_string(err ? err : ""));
                json_set(out, "cooldown_record", rec);
            }
            json_set(out, "record", conv9_record("failed", "codex_compaction_failed", "none"));
            free(msg);
            free(err);
            json_set(out, "logs", logs);
            free(mode);
            free(tokens);
            free(sid);
            return out;
        }
        /* success: bookkeeping, dedup reset, done log, committed record */
        json_set(out, "bookkeeping", json_bool(1));
        json_set(out, "usage", json_bool(has_update ? 1 : 0));
        {
            const char *sid_attr = in ? json_get_str(in, "session_id", "") : "";
            json_t *reset = json_object();
            json_set(reset, "task_id", json_string(task_id ? task_id : ""));
            json_set(reset, "session_id", json_string(sid_attr ? sid_attr : ""));
            json_set(out, "dedup_reset", reset);
        }
        snprintf(line, sizeof(line),
                 "codex app-server compaction done: session=%s thread=%s turn=%s",
                 sid,
                 json_get_str(result, "thread_id", ""),
                 json_get_str(result, "turn_id", ""));
        conv9_push_log(logs, line);
        json_set(out, "record", conv9_record("committed", NULL, "provider"));
    }

    json_set(out, "logs", logs);
    free(mode);
    free(tokens);
    free(sid);
    return out;
}
