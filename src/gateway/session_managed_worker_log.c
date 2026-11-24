/*
 * session_managed_worker_log.c - C11 port of upstream gateway/session_managed_worker_log.py
 * (gate w25_sessionman_1c48ef).
 *
 * PoP: w25_sessionman_1c48ef_worker_log_path @ gateway/session_managed_worker_log.py:worker_log_path
 * PoP: w25_sessionman_1c48ef_frame_lines @ gateway/session_managed_worker_log.py:drain_worker_stderr
 * PoP: w25_sessionman_1c48ef_append_plan @ gateway/session_managed_worker_log.py:_append
 * PoP: w25_sessionman_1c48ef_constants @ gateway/session_managed_worker_log.py:<module constants>
 *
 * The worker log is a thread plus a file, and both are seams: the dedicated
 * drain thread, the per-file lock dict, mkdir_under_hermes_home, the 0600
 * os.open/os.write pair, the os.replace rotation and the redactor all stay in
 * the caller.  Ported here is the decision core the thread runs on: the sink
 * path (absolute home or nothing), the framing state machine -- ``skipping``
 * after an over-long line, ``warned`` once on a write failure, the drain
 * continuing either way so a child blocked on a full pipe never wedges -- the
 * KEPT_LINE_CHARS truncation, and the rotate/write sizes.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_sessionman_1c48ef.h"

#define SM_LOG_NAME "managed-worker.log"
#define SM_MAX_LOG_BYTES 1048576
#define SM_MAX_LINE_BYTES 65536
#define SM_KEPT_LINE_CHARS 4096

/* str() over the shapes a profile id takes (None for absent/null). */
static void sm_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%g", v->num_val);
        return;
    case JSON_ARRAY:  snprintf(buf, cap, "[]"); return;
    case JSON_OBJECT: snprintf(buf, cap, "{}"); return;
    case JSON_NULL:   snprintf(buf, cap, "None"); return;
    }
    snprintf(buf, cap, "None");
}

/* WindowsPath.is_absolute() in this Python: drive AND root ("C:\\x"), so
 * "\\foo", "\\\\server\\share", "c:foo" and "/" are all not absolute. */
static int sm_is_absolute(const char *s) {
    return isalpha((unsigned char)s[0]) && s[1] == ':' && (s[2] == '/' || s[2] == '\\');
}

/* str(WindowsPath(...)): backslash separators, empty and "." segments dropped. */
static json_t *sm_join_log_path(const char *s) {
    char *out;
    size_t cap = strlen(s) + strlen(SM_LOG_NAME) + 16;
    size_t len = 0, i;

    out = (char *)malloc(cap);
    if (!out) return json_null();
    out[len++] = (char)toupper((unsigned char)s[0]);
    out[len++] = ':';
    i = 2;
    while (s[i]) {
        size_t start;
        if (s[i] == '/' || s[i] == '\\') { i++; continue; }
        start = i;
        while (s[i] && s[i] != '/' && s[i] != '\\') i++;
        if (i - start == 1 && s[start] == '.') continue;  /* PurePath drops "." */
        out[len++] = '\\';
        memcpy(out + len, s + start, i - start);
        len += i - start;
    }
    memcpy(out + len, "\\logs\\" SM_LOG_NAME, strlen("\\logs\\" SM_LOG_NAME));
    len += strlen("\\logs\\" SM_LOG_NAME);
    out[len] = '\0';
    {
        json_t *res = json_string(out);
        free(out);
        return res;
    }
}

json_t *w25_sessionman_1c48ef_worker_log_path(const json_t *profile_id) {
    char text[4096];
    sm_str(profile_id, text, sizeof text);
    if (!sm_is_absolute(text)) return json_null();
    return sm_join_log_path(text);
}

/* Count of UTF-8 characters in a prefix of at most ``keep`` characters. */
static size_t sm_prefix_bytes(const char *s, size_t keep) {
    size_t i = 0, len = strlen(s), n = 0;
    while (i < len && n < keep) {
        unsigned char c = (unsigned char)s[i];
        size_t step = 1;
        if ((c & 0xE0) == 0xC0) step = 2;
        else if ((c & 0xF0) == 0xE0) step = 3;
        else if ((c & 0xF8) == 0xF0) step = 4;
        if (i + step > len) step = 1;
        i += step;
        n++;
    }
    return i;
}

json_t *w25_sessionman_1c48ef_frame_lines(const json_t *plan) {
    const json_t *chunks;
    const char *pid = "0";
    char pidbuf[64];
    int skipping = 0, warned = 0, fail_from = -1, calls = 0;
    json_t *out, *appended;
    size_t i, n;

    if (!plan || plan->type != JSON_OBJECT) return json_null();
    chunks = json_obj_get(plan, "chunks");
    if (chunks && chunks->type != JSON_NULL && chunks->type != JSON_ARRAY) return json_null();
    if (json_obj_get(plan, "pid")) {
        sm_str(json_obj_get(plan, "pid"), pidbuf, sizeof pidbuf);
        pid = pidbuf;
    }
    if (json_obj_get(plan, "fail_from")) fail_from = (int)json_get_num(plan, "fail_from", -1.0);

    out = json_object();
    appended = json_array();
    n = chunks ? json_len(chunks) : 0;

    for (i = 0; i < n; i++) {
        const json_t *cv = json_get(chunks, i);
        const char *chunk;
        int whole;
        char *text;
        size_t need;

        /* readline() returned b"": the stream ended, the loop returns. */
        if (!cv || cv->type != JSON_STRING || !cv->str_val || cv->str_val[0] == '\0') break;
        chunk = cv->str_val;
        whole = chunk[strlen(chunk) - 1] == '\n';
        if (skipping) {                 /* tail of an over-long line: discarded */
            skipping = !whole;
            continue;
        }
        skipping = !whole;

        if (whole) {
            need = strlen(chunk) + strlen(pid) + 16;
            text = (char *)malloc(need);
            if (!text) break;
            snprintf(text, need, "[pid %s] %s", pid, chunk);
        } else {
            char *cut = (char *)malloc(strlen(chunk) + 1);
            size_t used;
            if (!cut) break;
            used = sm_prefix_bytes(chunk, SM_KEPT_LINE_CHARS);
            memcpy(cut, chunk, used);
            cut[used] = '\0';
            need = strlen(cut) + strlen(pid) + 32;
            text = (char *)malloc(need);
            if (!text) { free(cut); break; }
            snprintf(text, need, "[pid %s] %s [truncated]\n", pid, cut);
            free(cut);
        }

        /* The sink is handed every line; a failed write warns once and the
         * drain keeps going (a child blocked on a full pipe would wedge). */
        if (fail_from >= 0 && calls >= fail_from) warned = 1;
        calls++;
        json_append(appended, json_string(text));
        free(text);
    }

    json_set(out, "appended", appended);
    json_set(out, "warned", json_bool(warned));
    return out;
}

json_t *w25_sessionman_1c48ef_append_plan(const json_t *plan) {
    double size, nbytes;
    int rotate;
    json_t *out;

    if (!plan || plan->type != JSON_OBJECT) return json_null();
    size = json_get_num(plan, "size", 0.0);
    nbytes = json_get_num(plan, "nbytes", 0.0);
    rotate = size != 0.0 && size + nbytes > (double)SM_MAX_LOG_BYTES;
    out = json_object();
    json_set(out, "rotate", json_bool(rotate));
    json_set(out, "write", json_number(nbytes > (double)SM_MAX_LOG_BYTES ? (double)SM_MAX_LOG_BYTES : nbytes));
    return out;
}

json_t *w25_sessionman_1c48ef_constants(void) {
    json_t *o = json_object();
    json_set(o, "log_name", json_string(SM_LOG_NAME));
    json_set(o, "max_log_bytes", json_number((double)SM_MAX_LOG_BYTES));
    json_set(o, "max_line_bytes", json_number((double)SM_MAX_LINE_BYTES));
    json_set(o, "kept_line_chars", json_number((double)SM_KEPT_LINE_CHARS));
    json_set(o, "rotate_suffix", json_string(".1"));
    json_set(o, "file_mode", json_number(384.0));   /* 0o600 */
    return o;
}
