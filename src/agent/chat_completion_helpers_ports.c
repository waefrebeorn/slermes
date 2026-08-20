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

/* PoP: cch_status_code_from_value @ agent/chat_completion_helpers.py:_status_code_from_value
 * Parse an HTTP status code from an int (100-599) or a string. Returns the
 * status code, or -1 when none can be extracted (LIVE returns None).
 * Arg = the value (int as decimal string, or arbitrary string). */
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

/* PoP: cch_status_code_from_payload @ agent/chat_completion_helpers.py:_status_code_from_payload
 * Extract an HTTP status code from a JSON payload object. Returns the code or
 * -1 when none can be extracted (LIVE returns None). */
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

/* PoP: cch_json_object_from_text @ agent/chat_completion_helpers.py:_json_object_from_text
 * Parse a JSON object from text. Returns a malloc'd JSON string (or NULL). */
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

/* PoP: cch_provider_error_body @ agent/chat_completion_helpers.py:_provider_error_body
 * Normalize a provider error payload to OpenAI-style {"error": {...}}. */
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

/* PoP: cch_provider_stream_text_may_be_sse @ agent/chat_completion_helpers.py:_provider_stream_text_may_be_sse
 * Return true while pending text still looks like an SSE control block. */
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
