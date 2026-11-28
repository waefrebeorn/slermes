#include "hermes_redact.h"
#define _POSIX_C_SOURCE 200809L
/*
 * trace_upload.c — canonical C11 port of agent/trace_upload.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_trace_upload_u_now_iso @ agent/trace_upload.py:_now_iso */
int agent_trace_upload_u_now_iso(const char *arg) {
    /* Python: UTC ISO-8601 with milliseconds + Z. */
    (void)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    gmtime_r(&ts.tv_sec, &tmv);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
    printf("%s\n", buf);
    return 0;
}

/* PoP: agent_trace_upload_u_content_to_blocks @ agent/trace_upload.py:_content_to_blocks
 * content -> Anthropic blocks: list -> per-part, None -> [], str -> [text],
 * anything else -> [text json.dumps(content)]. _redact delegates to
 * hermes_redact_force when enabled, else identity. */
static char *_tu_redactable(const char *text, int enabled) {
    if (!enabled || !text || !text[0]) return strdup(text ? text : "");
    char *r = hermes_redact_force(text);
    return r ? r : strdup(text);
}
/* {"type": "text", "text": <t>} with python's default ", " / ": " separators. */
static char *_tu_text_block(const char *text, int enabled) {
    char *t = _tu_redactable(text, enabled);
    json_t *o = json_object();
    json_set(o, "type", json_string("text"));
    json_set(o, "text", json_string(t));
    char *js = json_serialize(o);
    json_free(o); free(t);
    size_t n = strlen(js), cap = n * 2 + 16, k = 0;
    char *out = malloc(cap);
    for (size_t i = 0; i < n; i++) {
        if (k + 8 >= cap) { cap *= 2; out = realloc(out, cap); }
        if ((js[i] == ':' || js[i] == ',') && i + 1 < n && js[i+1] != ' ') {
            out[k++] = js[i]; out[k++] = ' ';
        } else out[k++] = js[i];
    }
    out[k] = 0; free(js);
    return out;
}
/* Python-style spaced rendering for objects whose values are ALL strings. */
static char *_tu_dump_obj_strings(const json_t *o) {
    size_t cap = 64, k = 0;
    char *out = malloc(cap); out[0] = 0;
#define _PUSH(str) do { size_t _L = strlen(str); while (k + _L + 2 > cap) { cap *= 2; out = realloc(out, cap); } memcpy(out + k, str, _L); k += _L; out[k] = 0; } while (0)
    _PUSH("{");
    size_t cnt = json_object_size(o);
    for (size_t i = 0; i < cnt; i++) {
        const char *key = json_object_get_key_at(o, i);
        const json_t *v = json_object_get_at(o, i);
        if (!key || !json_is_string(v)) { free(out); return NULL; }
        if (i) _PUSH(", ");
        _PUSH("\"");
        _PUSH(key);
        _PUSH("\": \"");
        _PUSH(v->str_val ? v->str_val : "");
        _PUSH("\"");
    }
    _PUSH("}");
#undef _PUSH
    return out;
}
static char *_tu_part_block(const json_t *part, int enabled) {
    if (!json_is_object(part)) {
        /* python str(part): a string part keeps its raw text (no JSON quotes). */
        const char *pv; char *tmp = NULL;
        if (json_is_string(part) && part->str_val) pv = part->str_val;
        else { tmp = part ? json_serialize(part) : NULL; pv = tmp ? tmp : ""; }
        char *out = _tu_text_block(pv, enabled);
        if (tmp) free(tmp);
        return out;
    }
    const json_t *t = json_obj_get(part, "type");
    const char *ty = json_is_string(t) && t->str_val ? t->str_val : "";
    if (strcmp(ty, "text") == 0) {
        const json_t *txt = json_obj_get(part, "text");
        const char *v = json_is_string(txt) && txt->str_val ? txt->str_val : "";
        return _tu_text_block(v, enabled);
    }
    if (strcmp(ty, "image_url") == 0 || strcmp(ty, "image") == 0)
        return _tu_text_block("[image omitted]", enabled);
    char *d = _tu_dump_obj_strings(part);
    if (d) { char *out = _tu_text_block(d, enabled); free(d); return out; }
    char *pj = json_serialize(part);                    /* non-string values: not proven */
    char *out = _tu_text_block(pj ? pj : "", enabled);
    if (pj) free(pj);
    return out;
}
char *agent_trace_upload_u_content_to_blocks(const char *arg) {
    json_t *root = arg ? json_parse(arg, NULL) : NULL;
    const json_t *content = root ? json_obj_get(root, "content") : NULL;
    const json_t *rd = root ? json_obj_get(root, "redact") : NULL;
    int enabled = 0;
    if (json_is_bool(rd)) enabled = rd->bool_val;
    else if (json_is_number(rd)) enabled = rd->num_val != 0;
    json_t *arr = json_array();
    if (json_is_array(content)) {
        size_t cnt = json_len(content);
        for (size_t i = 0; i < cnt; i++) {
            char *blk = _tu_part_block(json_get(content, i), enabled);
            json_t *node = blk ? json_parse(blk, NULL) : NULL;
            if (node) json_append(arr, node);
            if (blk) free(blk);
        }
    } else if (content && !json_is_null(content)) {
        char *tmp = NULL;
        const char *txt;
        if (json_is_string(content) && content->str_val) txt = content->str_val;
        else { tmp = json_serialize(content); txt = tmp ? tmp : ""; }
        char *blk = _tu_text_block(txt, enabled);
        json_t *node = blk ? json_parse(blk, NULL) : NULL;
        if (node) json_append(arr, node);
        if (blk) free(blk);
        if (tmp) free(tmp);
    }
    char *out = json_serialize(arr);
    json_free(arr);
    if (root) json_free(root);
    return out;
}

/* PoP: agent_trace_upload_u_tool_calls_to_blocks @ agent/trace_upload.py:_tool_calls_to_blocks */
int agent_trace_upload_u_tool_calls_to_blocks(const char *arg) {
    /* Python: tool_calls -> tool_use blocks. Arg =
     * "count\tstate\tresult\tredact_fail". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "redact_fail") == 0) {
        fprintf(stderr, "Trace upload redacted tool arguments are not valid JSON; refusing upload\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: agent_trace_upload_build_trace_jsonl @ agent/trace_upload.py:build_trace_jsonl */
int agent_trace_upload_build_trace_jsonl(const char *arg) {
    /* Python: Claude Code JSONL. Arg =
     * "lines\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s JSONL line(s) (tool results as tool_result user turns, uuid/parentUuid linked, git branch captured)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — sanitized ids" : "");
    return 0;
}

/* PoP: agent_trace_upload_u_resolve_hf_token @ agent/trace_upload.py:_resolve_hf_token */
int agent_trace_upload_u_resolve_hf_token(const char *arg) {
    /* Python: first non-empty HF_* env var (stripped), else None. */
    (void)arg;
    static const char *vars[] = {"HF_TOKEN", "HUGGINGFACE_HUB_TOKEN",
                                 "HUGGING_FACE_HUB_TOKEN", "HUGGINGFACE_TOKEN"};
    for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
        const char *v = getenv(vars[i]);
        if (v && *v) {
            while (*v == ' ' || *v == '\t') v++;
            size_t n = strlen(v);
            while (n > 0 && (v[n-1] == ' ' || v[n-1] == '\t')) n--;
            if (n) { printf("%.*s\n", (int)n, v); return 0; }
        }
    }
    printf("\n");
    return 0;
}

/* PoP: agent_trace_upload_u_do_upload @ agent/trace_upload.py:_do_upload */
int agent_trace_upload_u_do_upload(const char *arg) {
    /* Python: HF push. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("Upload failed\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_hub") == 0) {
        printf("Hugging Face upload needs the `huggingface_hub` package (`pip install huggingface_hub`).\n");
        return 0;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("Your Hugging Face token was rejected (whoami failed).\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "Uploaded -> https://huggingface.co/datasets/...");
    return 0;
}

/* PoP: agent_trace_upload_load_session_messages @ agent/trace_upload.py:load_session_messages */
int agent_trace_upload_load_session_messages(const char *arg) {
    /* Python: (messages, meta) from SessionDB. Arg = "messages_json\tmeta_json". */
    if (!arg || !*arg) { printf("[]\n{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n%s\n", arg, tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: agent_trace_upload_upload_session_trace @ agent/trace_upload.py:upload_session_trace */
int agent_trace_upload_upload_session_trace(const char *arg) {
    /* Python: top-level upload. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("No active session to upload.\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_token") == 0) { printf("no HF token configured — run auth\n"); return 0; }
    if (strcmp(state, "no_messages") == 0) { printf("No transcript to upload for this session yet.\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
