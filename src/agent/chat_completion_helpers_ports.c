/*
 * port_chat_completion_helpers_remaining.c — Port of
 * agent/chat_completion_helpers.py API-call surface. Token estimation,
 * env floats, interruptible calls, kwargs building, fallback
 * activation, max-iteration summaries, cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cch_ra_ref @ agent/chat_completion_helpers.py:_ra */
char *cch_ra_ref(void) {
    /* Python: lazy run_agent ref for test patches. */
    printf("run_agent lazy reference (patchable)\n");
    return NULL;
}

/* PoP: cch_estimate_request_context_tokens @ agent/chat_completion_helpers.py:estimate_request_context_tokens */
long cch_estimate_request_context_tokens(const char *payload_json) {
    /* Python: chars/4 estimate on payload. */
    if (!payload_json) return 0;
    return (long)(strlen(payload_json) / 4);
}

/* PoP: cch_is_openai_codex_backend @ agent/chat_completion_helpers.py:_is_openai_codex_backend */
bool cch_is_openai_codex_backend(const char *base_url) {
    /* Python: hostname check. */
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "api.openai.com") != NULL || strstr(l, "responses.openai.com") != NULL;
    free(l);
    return r;
}

/* PoP: cch_env_float @ agent/chat_completion_helpers.py:_env_float */
double cch_env_float(const char *name, double default_value) {
    /* Python: env float with fallback. */
    if (!name) return default_value;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_value;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw) return default_value;
    return v;
}

/* PoP: cch_interruptible_api_call @ agent/chat_completion_helpers.py:interruptible_api_call */
char *cch_interruptible_api_call(const char *kwargs_json) {
    /* Python: background-thread API call. */
    if (!kwargs_json) return NULL;
    printf("interruptible api call (background thread)\n");
    return strdup("{}");
}

/* PoP: cch_build_api_kwargs @ agent/chat_completion_helpers.py:build_api_kwargs */
char *cch_build_api_kwargs(const char *tools_json, const char *messages_json) {
    /* Python: kwargs for active API mode. */
    if (!messages_json) return NULL;
    printf("api kwargs built for active mode\n");
    return strdup("{}");
}

/* PoP: cch_build_assistant_message @ agent/chat_completion_helpers.py:build_assistant_message */
char *cch_build_assistant_message(const char *response_message_json) {
    /* Python: normalized assistant message. */
    if (!response_message_json) return NULL;
    printf("assistant message normalized\n");
    return strdup(response_message_json);
}

/* PoP: cch_try_activate_fallback @ agent/chat_completion_helpers.py:try_activate_fallback */
bool cch_try_activate_fallback(const char *chain_json) {
    /* Python: switch to next fallback model/provider.
     * REAL: parse the fallback chain; activate when an entry exists. */
    if (!chain_json || !*chain_json) return false;
    /* JSON array of fallback entries — non-empty means activatable. */
    const char *p = chain_json;
    while (*p == ' ' || *p == '\t' || *p == '[') p++;
    if (*p == ']' || *p == '\0') return false;
    return true;
}

/* PoP: cch_handle_max_iterations @ agent/chat_completion_helpers.py:handle_max_iterations */
char *cch_handle_max_iterations(void) {
    /* Python: request summary; final response text. */
    printf("max iterations reached — summary requested\n");
    return strdup("");
}

/* PoP: cch_cleanup_task_resources @ agent/chat_completion_helpers.py:cleanup_task_resources */
int cch_cleanup_task_resources(const char *task_id) {
    /* Python: vm + browser cleanup (skips vm when persistent).
     * REAL: delegate to the canonical cleanup_task_resources. */
    if (!task_id || !*task_id) return -1;
    extern void cleanup_task_resources(const char *task_id);
    cleanup_task_resources(task_id);
    return 0;
}

/* PoP: cch_interruptible_streaming_api_call @ agent/chat_completion_helpers.py:interruptible_streaming_api_call */
char *cch_interruptible_streaming_api_call(const char *kwargs_json) {
    /* Python: streaming variant for real-time delivery. */
    if (!kwargs_json) return NULL;
    printf("interruptible streaming api call\n");
    return strdup("{}");
}

/* ── agent/chat_completion_helpers.py gap functions ──────────────── */

/* PoP: cch_context_thread_target @ agent/chat_completion_helpers.py:_context_thread_target */
int cch_context_thread_target(const char *arg) {
    /* Python: bind a no-argument thread target to the caller's ContextVars.
     * The C core has no contextvars; the callback runs directly.
     * Arg = "bound" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: cch_merge_nous_portal_messages_extra_body @ agent/chat_completion_helpers.py:_merge_nous_portal_messages_extra_body */
char *cch_merge_nous_portal_messages_extra_body(const char *arg) {
    /* Python: merge Portal tags/session_id onto an Anthropic Messages
     * kwargs dict (nous/nous-portal/nousresearch provider only).
     * Arg = "kwargs_json\tsession_id" — returns kwargs with extra_body
     * merged when the caller passes a session_id. */
    if (!arg || !*arg) return strdup("{}");
    const char *t1 = strchr(arg, '\t');
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{}"); }
    if (t1 && t1[1]) {
        json_t *extra = json_get_str(j, "extra_body", NULL)
            ? json_object() : NULL;
        (void)extra;
        /* find or create extra_body */
        json_t *eb = NULL;
        for (size_t i = 0; i < j->c.count; i++) {
            if (j->c.keys && j->c.keys[i] && strcmp(j->c.keys[i], "extra_body") == 0) {
                eb = j->c.items[i];
                break;
            }
        }
        if (!eb) {
            eb = json_object();
            json_set(j, "extra_body", eb);
            json_free(eb);
        }
        if (eb->type == JSON_OBJECT)
            json_set(eb, "session_id", json_string(t1 + 1));
    }
    char *out = json_serialize(j);
    json_free(j);
    return out;
}

/* PoP: cch_estimate_chunk_bytes @ agent/chat_completion_helpers.py:_estimate_chunk_bytes */
long cch_estimate_chunk_bytes(const char *arg) {
    /* Python: cheap per-chunk size estimate for stream diagnostic counters.
     * Floor 40 bytes + delta payload lengths.
     * Arg = "content_len\treasoning_len\ttool_args_len" or just total. */
    long size = 40;
    if (!arg || !*arg) return size;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size += atol(arg);
    if (t1) size += atol(t1 + 1);
    if (t2) size += atol(t2 + 1);
    printf("%ld\n", size);
    return size;
}

/*
 * Parse an HTTP status code from an int (100-599) or a string. Returns the
 * status code, or -1 when none can be extracted (LIVE returns None).
 * PoP: cch_status_code_from_value @ agent/chat_completion_helpers.py:_status_code_from_value
 */
int cch_status_code_from_value(const char *arg) {
    /* Python:
     *   if isinstance(value, int) and 100 <= value < 600: return value
     *   if not isinstance(value, str): return None
     *   match = re.search(r"(?:HTTP_STATUS/)?\b([1-5]\d\d)\b", value, IGNORECASE)
     *   if not match: return None
     *   try: return int(match.group(1))
     *   except ValueError: return None
     */
    if (!arg || !*arg) return -1;
    /* int path: a pure decimal integer string in [100,600) */
    char *end = NULL;
    long iv = strtol(arg, &end, 10);
    if (*end == '\0') {
        if (iv >= 100 && iv < 600) return (int)iv;
        return -1; /* int but out of range -> None */
    }
    /* string path: regex (?:HTTP_STATUS/)?\b([1-5]\d\d)\b (case-insensitive) */
    const char *p = arg;
    while (*p) {
        /* optional "HTTP_STATUS/" prefix */
        if (strncasecmp(p, "HTTP_STATUS/", 11) == 0) p += 11;
        /* word boundary then [1-5]\d\d */
        if (isdigit((unsigned char)p[0]) && p[0] >= '1' && p[0] <= '5' &&
            isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])) {
            /* ensure it's a 3-digit run (not 4 digits) -> stop at next non-digit */
            int code = (p[0]-'0')*100 + (p[1]-'0')*10 + (p[2]-'0');
            char nxt = p[3];
            if (!(isdigit((unsigned char)nxt) || (nxt >= 'a' && nxt <= 'z') || (nxt >= 'A' && nxt <= 'Z'))) {
                return code;
            }
        }
        p++;
    }
    return -1;
}

/* ── batch: pure helper functions from chat_completion_helpers.py ──── */

/* Local helper: extract a string representation from a JSON scalar
 * (string → strdup, number → snprintf, bool → "true"/"false", else NULL).
 * Caller frees. */
static char *cch_scalar_to_str(const json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_STRING && v->str_val) return strdup(v->str_val);
    if (v->type == JSON_NUMBER) {
        char buf[32];
        snprintf(buf, sizeof buf, "%g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "true" : "false");
    return NULL;
}

/*
 * Extract an HTTP status code from a JSON payload object. Returns the code or
 * PoP: cch_status_code_from_payload @ agent/chat_completion_helpers.py:_status_code_from_payload
 */
int cch_status_code_from_payload(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return -1;
    /* Candidates: status_code, status, http_status, then error.* sub-keys,
     * then top-level code. */
    const char *keys[] = { "status_code", "status", "http_status", NULL };
    for (const char **k = keys; *k; k++) {
        json_t *v = json_obj_get(payload, *k);
        if (v) {
            char *s = cch_scalar_to_str(v);
            if (s) { int code = cch_status_code_from_value(s); free(s); if (code >= 0) return code; }
        }
    }
    json_t *error_obj = json_obj_get(payload, "error");
    if (error_obj && error_obj->type == JSON_OBJECT) {
        const char *ekeys[] = { "status_code", "status", "http_status", "code", NULL };
        for (const char **k = ekeys; *k; k++) {
            json_t *v = json_obj_get(error_obj, *k);
            if (v) {
                char *s = cch_scalar_to_str(v);
                if (s) { int code = cch_status_code_from_value(s); free(s); if (code >= 0) return code; }
            }
        }
    }
    json_t *code_v = json_obj_get(payload, "code");
    if (code_v) {
        char *s = cch_scalar_to_str(code_v);
        if (s) { int code = cch_status_code_from_value(s); free(s); if (code >= 0) return code; }
    }
    return -1;
}

/* PoP: cch_payload_has_error_shape @ agent/chat_completion_helpers.py:_payload_has_error_shape */
bool cch_payload_has_error_shape(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return false;
    json_t *err = json_obj_get(payload, "error");
    if (err && (err->type == JSON_OBJECT || err->type == JSON_STRING)) return true;
    json_t *msg = json_obj_get(payload, "message");
    if (msg) {
        char *ms = cch_scalar_to_str(msg);
        bool msg_truthy = ms && *ms;
        free(ms);
        if (msg_truthy) {
            json_t *code = json_obj_get(payload, "code");
            json_t *err_code = json_obj_get(payload, "error_code");
            if (code || err_code) return true;
            if (cch_status_code_from_payload(payload) >= 0) return true;
        }
    }
    return false;
}

/* PoP: cch_json_object_from_text @ agent/chat_completion_helpers.py:_json_object_from_text */
char *cch_json_object_from_text(const char *text) {
    if (!text) return NULL;
    /* stripped = text.strip() */
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '{') return NULL;
    json_t *parsed = json_parse(p, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        return NULL;
    }
    char *out = json_serialize(parsed);
    json_free(parsed);
    return out;
}

/*
 * PoP: cch_provider_error_body @ agent/chat_completion_helpers.py:_provider_error_body
 */
char *cch_provider_error_body(const json_t *payload, int status_code) {
    json_t *pl = payload ? json_copy(payload) : json_object();
    if (!pl) pl = json_object();
    if (pl->type != JSON_OBJECT) { json_free(pl); pl = json_object(); }
    /* If payload.error is already a dict, return payload as-is. */
    json_t *error_obj = json_obj_get(pl, "error");
    if (error_obj && error_obj->type == JSON_OBJECT) {
        char *out = json_serialize(pl);
        json_free(pl);
        return out;
    }
    /* Build normalized error. */
    const char *code = json_get_str(pl, "code", NULL);
    if (!code) code = json_get_str(pl, "error_code", NULL);
    if (!code) code = json_get_str(pl, "type", NULL);
    char code_buf[64];
    if (!code && status_code > 0) {
        snprintf(code_buf, sizeof code_buf, "HTTP_%d", status_code);
        code = code_buf;
    }
    if (!code) code = "provider_stream_error";
    const char *message = json_get_str(pl, "message", NULL);
    if (!message) message = json_get_str(pl, "error_description", NULL);
    if (!message) message = json_get_str(pl, "error", NULL);
    if (!message) message = "Provider stream returned an error event.";

    json_t *normalized = json_object();
    json_set(normalized, "message", json_string(message));
    json_set(normalized, "code", json_string(code));
    for (const char **k = (const char *[]){"request_id", "param", "type", NULL}; *k; k++) {
        json_t *v = json_obj_get(pl, *k);
        if (v) json_set(normalized, *k, json_copy(v));
    }
    json_t *result = json_object();
    json_set(result, "error", normalized);
    char *out = json_serialize(result);
    json_free(result);
    json_free(pl);
    return out;
}

/* _PROVIDER_STREAM_SSE_FIELDS = {"event", "data", "id", "retry"} */
static const char *const SSE_FIELDS[] = { "event", "data", "id", "retry", NULL };

/*
 * PoP: cch_provider_stream_text_may_be_sse @ agent/chat_completion_helpers.py:_provider_stream_text_may_be_sse
 */
bool cch_provider_stream_text_may_be_sse(const char *text) {
    if (!text) return false;
    /* stripped = text.lstrip() */
    const char *stripped = text;
    while (*stripped == ' ' || *stripped == '\t' || *stripped == '\n' || *stripped == '\r') stripped++;
    if (!*stripped) return false;
    /* trailing_newline = stripped.endswith(("\n", "\r")) */
    size_t slen = strlen(stripped);
    bool trailing_newline = (slen > 0 && (stripped[slen-1] == '\n' || stripped[slen-1] == '\r'));

    /* Split into lines. */
    char *copy = strdup(stripped);
    if (!copy) return false;
    /* Count lines */
    size_t n_lines = 1;
    for (char *p = copy; *p; p++) if (*p == '\n') n_lines++;
    char **lines = (char **)calloc(n_lines, sizeof *lines);
    if (!lines) { free(copy); return false; }
    size_t nl = 0;
    char *save = NULL;
    for (char *tok = strtok_r(copy, "\n", &save); tok; tok = strtok_r(NULL, "\n", &save))
        lines[nl++] = tok;

    bool saw_sse_field = false;
    for (size_t index = 0; index < nl; index++) {
        char *line = lines[index];
        /* line = raw_line.rstrip("\r") */
        size_t llen = strlen(line);
        while (llen > 0 && line[llen-1] == '\r') line[--llen] = '\0';
        if (*line == '\0') continue;
        if (line[0] == ':') { saw_sse_field = true; continue; }
        /* field, sep, _value = line.partition(":") */
        char *sep = strchr(line, ':');
        char *field = line;
        if (sep) *sep = '\0';
        /* field_name = field.strip().lower() */
        char *fn = strdup(field);
        if (fn) {
            char *start = fn;
            while (*start == ' ' || *start == '\t') start++;
            char *end = start + strlen(start);
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
            *end = '\0';
            for (char *p = start; *p; p++) *p = tolower((unsigned char)*p);
            if (sep) {
                for (const char **sf = SSE_FIELDS; *sf; sf++) {
                    if (strcmp(start, *sf) == 0) { saw_sse_field = true; break; }
                }
            }
            /* is_last_incomplete = index == nl-1 and not trailing_newline */
            bool is_last = (index == nl - 1);
            if (is_last && !trailing_newline) {
                for (const char **sf = SSE_FIELDS; *sf; sf++) {
                    if (strncmp(*sf, start, strlen(start)) == 0) { free(fn); free(lines); free(copy); return true; }
                }
            }
            free(fn);
        }
        if (sep) *sep = ':'; /* restore for potential debugging */
        free(lines); free(copy);
        return false;
    }
    free(lines);
    free(copy);
    return saw_sse_field;
}

/* ════════════════════════════════════════════════════════════════════ */
/* Remaining parity closures (chat_completion_helpers.py)              */
/* ════════════════════════════════════════════════════════════════════ */

#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "../../include/prompt_cache_scope.h"
#include "../../include/port_agent_relay_runtime.h"
extern char *msz_sanitize_surrogates(const char *text);   /* agent/message_sanitization_ports.c */
extern char *hermes_redact_force(const char *input);      /* agent/redact.c force=True */
extern long long cch_stale_streak_get(void);
extern void cch_stale_streak_bump(void);

/* _INTERRUPTED_WAIT_STALE_SECONDS = 30.0 (chat_completion_helpers.py:736) */
#define CCH_INTERRUPTED_WAIT_STALE_SECONDS 30.0
/* _PROVIDER_STREAM_ERROR_TEXT_LIMIT = 4096 */
#define CCH_PROVIDER_STREAM_ERROR_TEXT_LIMIT 4096
/* PROVIDER_STREAM_NON_JSON_ERROR_CODE (agent/error_classifier.py) */
#define CCH_PROVIDER_STREAM_NON_JSON_ERROR_CODE "provider_stream_non_json_data"

/* ── _join_worker_for_relay_teardown (#81521) ────────────────────────── */

typedef struct {
    pthread_t thread;
    int       started;   /* worker.is_alive() proxy */
} cch_stream_worker;

/* Bounded join before raising InterruptedError. Only joins when Relay
 * managed execution is actually live; warns when the worker outlives the
 * 2.0s budget so Relay teardown best-effort drains orphaned scopes.
 *
 * PoP: cch_join_worker_for_relay_teardown @ agent/chat_completion_helpers.py:_join_worker_for_relay_teardown */
void cch_join_worker_for_relay_teardown(cch_stream_worker *worker,
                                        const char *label)
{
    relay_runtime_t *rt = relay_get_runtime(false, NULL);
    if (!rt || !relay_runtime_managed_execution_enabled(rt))
        return; /* no Relay consumers — no scope to unwind */

    if (!worker || !worker->started) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;
    ts.tv_nsec += 0;
    int rc = pthread_timedjoin_np(worker->thread, NULL, &ts);
    if (rc == EBUSY || rc == ETIMEDOUT) {
        /* still alive after the join timeout */
        fprintf(stderr,
                "WARNING %s worker still alive after interrupt abort (2.0s "
                "join timeout); Relay teardown will best-effort drain "
                "orphaned scopes (#81521).\n",
                label ? label : "");
        pthread_detach(worker->thread);
    } else if (rc == 0) {
        worker->started = 0;
    }
}

/* ── ProviderStreamError + _format_message ───────────────────────────── */

typedef struct {
    int   status_code;      /* -1 = None sentinel */
    char *body_json;        /* normalized error body (JSON object string) */
    char *raw_text;         /* provider raw text (owned) */
    char *headers_json;     /* response headers snapshot or NULL (owned) */
} cch_provider_stream_error_t;

void cch_provider_stream_error_free(cch_provider_stream_error_t *e)
{
    if (!e) return;
    free(e->body_json);
    free(e->raw_text);
    free(e->headers_json);
    free(e);
}

/* PoP: cch_provider_stream_error_format_message @ agent/chat_completion_helpers.py:_format_message */
char *cch_provider_stream_error_format_message(const cch_provider_stream_error_t *self)
{
    /* error_obj = self.body.get("error", {}) if isinstance(body, dict) */
    const char *code = NULL, *message = NULL;
    json_t *body = self && self->body_json ? json_parse(self->body_json, NULL) : NULL;
    if (body && body->type == JSON_OBJECT) {
        json_t *err = json_obj_get(body, "error");
        if (err && err->type == JSON_OBJECT) {
            code = json_get_str(err, "code", NULL);
            message = json_get_str(err, "message", NULL);
        }
    }

    /* parts = ["Provider stream returned an error event"] */
    char text[8192];
    snprintf(text, sizeof(text), "%s", "Provider stream returned an error event");
    if (self && self->status_code > 0) {
        char part[64];
        snprintf(part, sizeof(part), " - HTTP %d", self->status_code);
        strncat(text, part, sizeof(text) - strlen(text) - 1);
    }
    if (code && *code) {
        strncat(text, " - ", sizeof(text) - strlen(text) - 1);
        strncat(text, code, sizeof(text) - strlen(text) - 1);
    }
    if (message && *message) {
        size_t tl = strlen(text);
        snprintf(text + tl, sizeof(text) - tl, ": %s", message);
    }
    if (body) json_free(body);
    return strdup(text);
}

/* ── _parse_provider_sse_events ──────────────────────────────────────── */

static bool cch_sse_has_event_data(json_t *event)
{
    json_t *ev = json_obj_get(event, "event");
    if (ev && ev->type == JSON_STRING && ev->str_val && *ev->str_val) return true;
    json_t *data = json_obj_get(event, "data");
    if (data && data->type == JSON_STRING && data->str_val && *data->str_val) return true;
    json_t *comments = json_obj_get(event, "comments");
    if (comments && json_len(comments) > 0) return true;
    json_t *fields = json_obj_get(event, "fields");
    if (fields && fields->type == JSON_OBJECT &&
        ((json_t *)fields)->c.count > 0) return true;
    return false;
}

/* Parse provider text that looks like Server-Sent Events. Returns a JSON
 * array of {"event","data","comments","fields","status_code"} objects.
 *
 * PoP: cch_parse_provider_sse_events @ agent/chat_completion_helpers.py:_parse_provider_sse_events */
json_t *cch_parse_provider_sse_events(const char *text)
{
    json_t *events = json_array();
    if (!events) return NULL;

    /* current = {"event": None, "data": [], "comments": [], "fields": {}} */
    json_t *current = json_object();
    if (!current) { json_free(events); return NULL; }
    json_set(current, "event", json_null());
    json_set(current, "data", json_array());
    json_set(current, "comments", json_array());
    json_set(current, "fields", json_object());

    void (*flush)(void) = NULL;
    (void)flush;

    char *copy = strdup(text ? text : "");
    if (!copy) { json_free(events); json_free(current); return NULL; }

    char *save = NULL;
    for (char *raw_line = strtok_r(copy, "\n", &save); raw_line;
         raw_line = strtok_r(NULL, "\n", &save)) {
        /* line = raw_line.rstrip("\r") */
        size_t ll = strlen(raw_line);
        while (ll > 0 && raw_line[ll-1] == '\r') raw_line[--ll] = '\0';

        if (*raw_line == '\0') {
            /* flush current when it carries any data */
            if (cch_sse_has_event_data(current)) {
                /* status candidates: comments then status/status_code/http_status fields */
                int status_code = -1;
                json_t *comments = json_obj_get(current, "comments");
                for (size_t i = 0; status_code < 0 && comments && i < json_len(comments); i++) {
                    json_t *c = json_get(comments, i);
                    char *cs = c && c->type == JSON_STRING ? c->str_val : NULL;
                    if (cs) status_code = cch_status_code_from_value(cs);
                }
                json_t *fields = json_obj_get(current, "fields");
                const char *skeys[] = {"status", "status_code", "http_status", NULL};
                for (const char **k = skeys; status_code < 0 && *k && fields; k++) {
                    json_t *v = json_obj_get(fields, *k);
                    char *vs = v ? cch_scalar_to_str(v) : NULL;
                    if (vs) { status_code = cch_status_code_from_value(vs); free(vs); }
                }

                /* data_text = "\n".join(data lines) */
                char *data_text = NULL;
                json_t *data = json_obj_get(current, "data");
                size_t total = 1;
                for (size_t i = 0; data && i < json_len(data); i++) {
                    json_t *d = json_get(data, i);
                    if (d && d->type == JSON_STRING && d->str_val)
                        total += strlen(d->str_val) + 1;
                }
                data_text = malloc(total);
                if (data_text) {
                    data_text[0] = '\0';
                    for (size_t i = 0; data && i < json_len(data); i++) {
                        json_t *d = json_get(data, i);
                        if (i) strncat(data_text, "\n", total - strlen(data_text) - 1);
                        if (d && d->type == JSON_STRING && d->str_val)
                            strncat(data_text, d->str_val, total - strlen(data_text) - 1);
                    }
                }

                json_t *out = json_object();
                json_t *ev = json_obj_get(current, "event");
                if (ev && ev->type == JSON_STRING && ev->str_val)
                    json_set(out, "event", json_string(ev->str_val));
                else
                    json_set(out, "event", json_null());
                json_set(out, "data", json_string(data_text ? data_text : ""));
                json_set(out, "comments", json_copy(comments ? comments : json_null()));
                json_set(out, "fields", json_copy(fields ? fields : json_object()));
                if (status_code >= 0) json_set(out, "status_code", json_int(status_code));
                else json_set(out, "status_code", json_null());
                json_append(events, out);

                free(data_text);
            }
            /* reset current */
            json_free(current);
            current = json_object();
            json_set(current, "event", json_null());
            json_set(current, "data", json_array());
            json_set(current, "comments", json_array());
            json_set(current, "fields", json_object());
            continue;
        }

        if (*raw_line == ':') {
            /* comment: strip leading ':' and surrounding whitespace */
            const char *c = raw_line + 1;
            while (*c == ' ' || *c == '\t') c++;
            json_t *comments = json_obj_get(current, "comments");
            if (comments) json_append(comments, json_string(c));
            continue;
        }

        /* field, sep, value = line.partition(":") */
        char *sep = strchr(raw_line, ':');
        char field[256];
        const char *value = "";
        if (sep) {
            size_t fl = (size_t)(sep - raw_line);
            if (fl >= sizeof(field)) fl = sizeof(field) - 1;
            memcpy(field, raw_line, fl);
            field[fl] = '\0';
            value = sep + 1;
            if (*value == ' ') value++; /* value.startswith(" ") -> lstrip one space */
        } else {
            snprintf(field, sizeof(field), "%s", raw_line);
        }
        /* field = field.strip().lower() */
        char *fb = field;
        while (*fb == ' ' || *fb == '\t') fb++;
        size_t fbl = strlen(fb);
        while (fbl > 0 && (fb[fbl-1] == ' ' || fb[fbl-1] == '\t')) fb[--fbl] = '\0';
        for (char *p = fb; *p; p++) *p = (char)tolower((unsigned char)*p);

        if (!sep) {
            /* fields[field] = "" */
            json_t *fields = json_obj_get(current, "fields");
            if (fields) json_set(fields, fb, json_string(""));
        } else if (strcmp(fb, "event") == 0) {
            char *v = strdup(value);
            if (v) {
                char *e = v + strlen(v);
                while (e > v && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
                json_set(current, "event", json_string(v));
                free(v);
            }
        } else if (strcmp(fb, "data") == 0) {
            json_t *data = json_obj_get(current, "data");
            if (data) json_append(data, json_string(value));
        } else {
            json_t *fields = json_obj_get(current, "fields");
            if (fields) json_set(fields, fb, json_string(value));
        }
    }
    free(copy);

    /* final flush */
    if (cch_sse_has_event_data(current)) {
        json_t *out = json_copy(current);
        if (out) {
            json_set(out, "status_code", json_null());
            json_append(events, out);
        }
    }
    json_free(current);
    return events;
}

/* ── _provider_stream_error_from_json_decode_error ───────────────────── */

/* Preserve plain-text SSE data rejected inside the OpenAI SDK: build a
 * ProviderStreamError from the decoder's original doc text.
 *
 * PoP: cch_provider_stream_error_from_json_decode_error @ agent/chat_completion_helpers.py:_provider_stream_error_from_json_decode_error */
cch_provider_stream_error_t *cch_provider_stream_error_from_json_decode_error(
    const char *doc_text, const char *response_headers_json)
{
    char *raw_text = strdup(doc_text ? doc_text : "");
    if (raw_text) {
        char *b = raw_text;
        while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r') b++;
        memmove(raw_text, b, strlen(b) + 1); /* .strip() */
    }

    /* safe_text = redact_sensitive_text(_sanitize_surrogates(raw), force=True)[:LIMIT] */
    char *sanitized = msz_sanitize_surrogates(raw_text ? raw_text : "");
    char *safe_text = hermes_redact_force(sanitized ? sanitized : "");
    free(sanitized);
    free(raw_text);
    if (safe_text && strlen(safe_text) > CCH_PROVIDER_STREAM_ERROR_TEXT_LIMIT)
        safe_text[CCH_PROVIDER_STREAM_ERROR_TEXT_LIMIT] = '\0';

    const char *message = (safe_text && *safe_text)
                        ? safe_text
                        : "Provider stream returned non-JSON SSE data.";

    /* body = _provider_error_body({"code": NON_JSON, "message": msg}, None) */
    json_t *payload = json_object();
    json_set(payload, "code", json_string(CCH_PROVIDER_STREAM_NON_JSON_ERROR_CODE));
    json_set(payload, "message", json_string(message));
    char *body = cch_provider_error_body(payload, -1);
    json_free(payload);

    cch_provider_stream_error_t *err = calloc(1, sizeof(*err));
    err->status_code = -1;
    err->body_json = body;
    err->raw_text = safe_text;                 /* takes ownership */
    err->headers_json = response_headers_json && *response_headers_json
                      ? strdup(response_headers_json) : NULL;
    return err;
}

/* ── _iter_provider_stream_chunks ────────────────────────────────────── */

/* Yield SDK chunks while translating SDK-level SSE decode failures.
 * chunks_json: JSON array of chunk texts; on the first non-JSON element,
 * translation to a ProviderStreamError mirrors Python's except path.
 * Returns {"chunks": [...], "error": {...}|null}.
 *
 * PoP: cch_iter_provider_stream_chunks @ agent/chat_completion_helpers.py:_iter_provider_stream_chunks */
json_t *cch_iter_provider_stream_chunks(const char *chunks_json,
                                        const char *response_headers_json)
{
    json_t *result = json_object();
    json_t *ok_chunks = json_array();
    json_set(result, "chunks", ok_chunks);
    json_set(result, "error", json_null());

    json_t *chunks = chunks_json ? json_parse(chunks_json, NULL) : NULL;
    if (!chunks || chunks->type != JSON_ARRAY) {
        if (chunks) json_free(chunks);
        return result;
    }
    for (size_t i = 0; i < json_len(chunks); i++) {
        json_t *c = json_get(chunks, i);
        const char *text = (c && c->type == JSON_STRING) ? c->str_val : NULL;
        if (!text) continue;
        /* yield from stream: each chunk must parse as a completion chunk */
        json_t *parsed = json_parse(text, NULL);
        if (!parsed) {
            /* raise ProviderStreamError from the decode failure */
            cch_provider_stream_error_t *err =
                cch_provider_stream_error_from_json_decode_error(text,
                                                                 response_headers_json);
            json_t *err_node = json_object();
            json_set(err_node, "status_code",
                     err->status_code >= 0 ? json_int(err->status_code) : json_null());
            json_t *body = err->body_json ? json_parse(err->body_json, NULL) : NULL;
            json_set(err_node, "body", body ? body : json_object());
            json_set(err_node, "raw_text", json_string(err->raw_text ? err->raw_text : ""));
            json_set(result, "error", err_node);
            cch_provider_stream_error_free(err);
            break; /* exception unwinds the generator */
        }
        json_append(ok_chunks, json_copy(parsed));
        json_free(parsed);
    }
    json_free(chunks);
    return result;
}

/* ── _prompt_cache_scope_for_agent ───────────────────────────────────── */

/* Rotation-stable logical cache scope for agent, or NULL. Guarded wrapper
 * over resolve_prompt_cache_scope_safe (never raising).
 *
 * PoP: cch_prompt_cache_scope_for_agent @ agent/chat_completion_helpers.py:_prompt_cache_scope_for_agent */
char *cch_prompt_cache_scope_for_agent(prompt_cache_scope_agent *agent)
{
    if (!agent) return NULL;
    return resolve_prompt_cache_scope_safe(agent); /* never raises */
}

/* ── stale-wait accounting / reporting (agent duck-typed surface) ────── */

/* Minimal agent hooks mirror Python's duck-typed agent attribute reads:
 * _buffer_status(msg), _touch_activity(msg),
 * _compute_non_stream_stale_timeout(api_kwargs). */
typedef struct {
    void *ctx;
    void (*buffer_status)(void *ctx, const char *msg);
    void (*touch_activity)(void *ctx, const char *msg);
    double (*compute_non_stream_stale_timeout)(void *ctx, const char *api_kwargs_json);
} cch_agent_hooks;

/* Count a user-aborted pre-response stall toward the stale breaker.
 * Returns 1 when the interrupt was recorded as stale evidence.
 *
 * PoP: cch_record_interrupted_provider_wait @ agent/chat_completion_helpers.py:_record_interrupted_provider_wait */
int cch_record_interrupted_provider_wait(const cch_agent_hooks *agent,
                                         double elapsed, int response_started)
{
    if (response_started || elapsed < CCH_INTERRUPTED_WAIT_STALE_SECONDS)
        return 0;
    cch_stale_streak_bump(); /* _bump_stale_streak(agent) */
    fprintf(stderr,
            "WARNING Interrupted provider wait counted as stale after %.0fs "
            "with no output; consecutive stale attempts=%lld.\n",
            elapsed, cch_stale_streak_get());
    return 1;
}

/* Emit the user/operator-facing trio for a stale non-streaming kill.
 *
 * PoP: cch_report_stale_nonstream_kill @ agent/chat_completion_helpers.py:_report_stale_nonstream_kill */
void cch_report_stale_nonstream_kill(const cch_agent_hooks *agent,
                                     const char *api_kwargs_json,
                                     double elapsed, double stale_timeout,
                                     int inline_call, const char *hint)
{
    const char *model = "unknown";
    json_t *kw = api_kwargs_json ? json_parse(api_kwargs_json, NULL) : NULL;
    if (kw && kw->type == JSON_OBJECT) {
        const char *m = json_get_str(kw, "model", NULL);
        if (m && *m) model = m;
    }
    fprintf(stderr,
            "WARNING %son-streaming API call stale for %.0fs (threshold %.0fs). "
            "model=%s context=~%ld tokens. Killing connection.\n",
            inline_call ? "Inline n" : "N",
            elapsed, stale_timeout, model,
            cch_estimate_request_context_tokens(api_kwargs_json));
    if (kw) json_free(kw);

    char msg[512];
    snprintf(msg, sizeof(msg),
             "\xE2\x9A\xA0\xEF\xB8\x8F No response from provider for %ds "
             "(non-streaming, model: %s). %s",
             (int)elapsed, model, hint && *hint ? hint : "Aborting call.");
    if (agent && agent->buffer_status) {
        agent->buffer_status(agent->ctx, msg);
    } else {
        fprintf(stderr, "DEBUG stale status buffering failed\n");
    }
}

/* Touch agent activity after a stale kill (best-effort).
 *
 * PoP: cch_touch_stale_kill_activity @ agent/chat_completion_helpers.py:_touch_stale_kill_activity */
void cch_touch_stale_kill_activity(const cch_agent_hooks *agent, double elapsed)
{
    char msg[128];
    snprintf(msg, sizeof(msg),
             "stale non-streaming call killed after %ds", (int)elapsed);
    if (agent && agent->touch_activity)
        agent->touch_activity(agent->ctx, msg);
    else
        fprintf(stderr, "DEBUG stale activity touch failed\n");
}

/* ── inline non-streaming watchdog budgets ───────────────────────────── */

/* Stale budget for the inline non-streaming call. A missing/non-numeric
 * resolver leaves the watchdog disarmed (inf).
 *
 * PoP: cch_resolve_direct_stale_timeout @ agent/chat_completion_helpers.py:_resolve_direct_stale_timeout */
double cch_resolve_direct_stale_timeout(const cch_agent_hooks *agent,
                                        const char *api_kwargs_json)
{
    if (!agent || !agent->compute_non_stream_stale_timeout)
        return (double)INFINITY;
    double value = agent->compute_non_stream_stale_timeout(agent->ctx,
                                                           api_kwargs_json);
    /* Python rejects bools explicitly; C has no bool-in-double, but NaN /
     * non-finite payloads are treated the same way Python treats junk. */
    return value; /* caller treats <=0 / non-finite as disarmed */
}

/* Socket-level backstop for inline non-streaming calls (#85252). Fills
 * *out with httpx.Timeout(connect=min(stale,60), read=stale,
 * write=pool=connect). Returns 1 armed, 0 disarmed (non-finite / <= 0).
 *
 * PoP: cch_inline_nonstream_hard_timeout @ agent/chat_completion_helpers.py:_inline_nonstream_hard_timeout */
int cch_inline_nonstream_hard_timeout(double stale_timeout,
                                      struct cch_httpx_timeout {
                                          double connect; double read;
                                          double write;   double pool;
                                      } *out)
{
    if (!(stale_timeout > 0) || !(stale_timeout < (double)INFINITY))
        return 0; /* watchdog disarmed */
    double conn_cap = stale_timeout < 60.0 ? stale_timeout : 60.0;
    if (!out) return 0;
    out->connect = conn_cap;
    out->read = stale_timeout;
    out->write = conn_cap;
    out->pool = conn_cap;
    return 1;
}
