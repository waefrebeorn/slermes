/*
 * run_turn_progress.c — C port of upstream Hermes gateway/run_turn_progress.py.
 *
 * PoP: w12_runturnpr_ae010a_repeat_count @ gateway/run_turn_progress.py:_append_progress_repeat_count
 * PoP: w12_runturnpr_ae010a_compact @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._TaskCardState._compact
 * PoP: w12_runturnpr_ae010a_progress_text @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._progress_text
 * PoP: w12_runturnpr_ae010a_is_reset_marker @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._is_reset_marker
 * PoP: w12_runturnpr_ae010a_absorb @ gateway/run_turn_progress.py:GatewayTurnProgressMixin._progress_absorb
 *
 * Pure progress-rendering seams: no sockets, no loop, no clock. Every exported
 * symbol is prefixed with the gate name to avoid collisions with other ports.
 */
#include "noc_w12_runturnpr_ae010a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str(v) for the JSON scalar shapes used by these seams. */
static char *py_str(const json_t *v) {
    char buf[128];
    if (!v) return strdup("None");
    switch (v->type) {
        case JSON_NULL:   return strdup("None");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            return strdup(buf);
        default:          return strdup("");
    }
}

/* Python `v or ""` truthiness for the scalar/container shapes in fixtures. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static long long int_arg(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "0", NULL, 10);
    return 0;
}

/* re.search(r"(?:^|\n)[ \t]*```[ \t]*\r?\Z", body) after body = message.rstrip("\r\n"). */
static int ends_in_closing_fence(const char *body) {
    const char *line = body;
    const char *nl = strrchr(body, '\n');
    if (nl) line = nl + 1;
    while (*line == ' ' || *line == '\t') line++;
    if (line[0] != '`' || line[1] != '`' || line[2] != '`') return 0;
    line += 3;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '\0';
}

json_t *w12_runturnpr_ae010a_repeat_count(const struct json_t *message, const struct json_t *occurrences) {
    const char *msg = (message && message->type == JSON_STRING) ? message->str_val : "";
    if (!msg) msg = "";
    char suffix[80];
    snprintf(suffix, sizeof(suffix), "(\xC3\x97%lld)", int_arg(occurrences));

    size_t orig_n = strlen(msg);
    size_t n = orig_n;
    char *body = (char *)malloc(n + 1);
    if (!body) return json_string("");
    memcpy(body, msg, n + 1);
    while (n > 0 && (body[n - 1] == '\r' || body[n - 1] == '\n')) {
        body[n - 1] = '\0';   /* shorten: NUL written at the new length */
        n--;
    }

    /* crlf is decided from the ORIGINAL message, before rstrip shortened it. */
    const char *crlf = (orig_n >= 2 && msg[orig_n - 2] == '\r' && msg[orig_n - 1] == '\n') ? "\r\n" : "\n";
    size_t fenced = strlen(body) + strlen(crlf) + strlen(suffix) + 1;
    size_t plain = strlen(msg) + 1 + strlen(suffix) + 1;
    size_t total = fenced > plain ? fenced : plain;
    char *out = (char *)malloc(total);
    if (!out) { free(body); return json_string(""); }
    if (ends_in_closing_fence(body))
        snprintf(out, total, "%s%s%s", body, crlf, suffix);
    else
        snprintf(out, total, "%s %s", msg, suffix);
    free(body);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* str = re.sub(r"\s+", " ", str(value or "")).strip() then truncate. */
json_t *w12_runturnpr_ae010a_compact(const struct json_t *value, const struct json_t *limit) {
    char *raw = truthy(value) ? py_str(value) : strdup("");
    size_t n = strlen(raw);
    char *collapsed = (char *)malloc(n + 2);
    if (!collapsed) { free(raw); return json_string(""); }
    size_t k = 0;
    int in_space = 0;
    for (size_t i = 0; i < n; i++) {
        if (isspace((unsigned char)raw[i])) {
            if (!in_space && k > 0) collapsed[k++] = ' ';
            in_space = 1;
        } else {
            collapsed[k++] = raw[i];
            in_space = 0;
        }
    }
    while (k > 0 && collapsed[k - 1] == ' ') { collapsed[k - 1] = '\0'; k--; }
    collapsed[k] = '\0';
    free(raw);

    long long lim = limit ? int_arg(limit) : 120;
    if ((long long)k > lim) {
        size_t head = (lim >= 3) ? (size_t)(lim - 3) : 0;
        while (head > 0 && collapsed[head - 1] == ' ') { collapsed[head - 1] = '\0'; head--; }
        collapsed[head] = '\0';   /* NUL at shortened length */
        strcat(collapsed, "...");
    }
    json_t *r = json_string(collapsed);
    free(collapsed);
    return r;
}

json_t *w12_runturnpr_ae010a_progress_text(const struct json_t *lines) {
    json_t *out = json_string("");
    if (!json_is_array(lines)) return out;
    size_t len = strlen(out->str_val);
    char *acc = (char *)malloc(len + 1);
    if (!acc) return out;
    memcpy(acc, out->str_val, len + 1);
    for (size_t i = 0; i < json_len(lines); i++) {
        char *s = py_str(json_get(lines, i));
        size_t ls = strlen(s);
        char *tmp = (char *)realloc(acc, len + ls + 2);
        if (!tmp) { free(s); break; }
        acc = tmp;
        if (i) acc[len++] = '\n';
        memcpy(acc + len, s, ls + 1);
        len += ls;
        free(s);
    }
    json_free(out);
    out = json_string(acc);
    free(acc);
    return out;
}

json_t *w12_runturnpr_ae010a_is_reset_marker(const struct json_t *raw) {
    if (!json_is_array(raw) || json_len(raw) < 1) return json_bool(0);
    const json_t *first = json_get(raw, 0);
    return json_bool(first && first->type == JSON_STRING && first->str_val &&
                     !strcmp(first->str_val, "__reset__"));
}

json_t *w12_runturnpr_ae010a_absorb(const struct json_t *progress_lines, const struct json_t *raw) {
    json_t *buf = json_is_array(progress_lines) ? json_copy(progress_lines) : json_array();
    json_t *rendered = NULL;
    int dedup = json_is_array(raw) && json_len(raw) == 3;
    if (dedup) {
        const json_t *tag = json_get(raw, 0);
        dedup = tag && tag->type == JSON_STRING && tag->str_val && !strcmp(tag->str_val, "__dedup__");
    }
    if (dedup) {
        const json_t *base = json_get(raw, 1);
        long long count = int_arg(json_get(raw, 2));
        if (json_len(buf) == 0) {
            rendered = json_copy(base);
        } else {
            json_t *rc = w12_runturnpr_ae010a_repeat_count(base, json_number((double)(count + 1)));
            json_array_set(buf, json_len(buf) - 1, json_copy(rc));
            rendered = rc;
        }
    } else {
        json_append(buf, json_copy(raw));
        json_t *nil = json_null();
        rendered = json_copy(raw ? raw : nil);
        json_free(nil);
    }
    json_t *out = json_object();
    json_set(out, "buf", buf);
    json_set(out, "rendered", rendered);
    return out;
}
