/*
 * gateway_rollback.c — C port of upstream Hermes hermes_cli/gateway_rollback.py.
 * Gate: w15_gatewayrol_519c95
 *
 * PoP: w15_gatewayrol_519c95_checkpoint_rows @ hermes_cli/gateway_rollback.py:_checkpoint_rows
 * PoP: w15_gatewayrol_519c95_diff @ hermes_cli/gateway_rollback.py:_diff
 *
 * t() is stubbed by the oracle as `key[|k=v,...]`; the C mirror below emits the
 * same strings. _diff's printed lines are captured as an array, one per print.
 */
#include "noc_w15_gatewayrol_519c95.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

/* Python str(value) for f-string interpolation. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val) snprintf(buf, cap, "%lld", i);
        else snprintf(buf, cap, "%g", v->num_val);
        return;
    }
    char *s = json_dumps(v, 0);
    snprintf(buf, cap, "%s", s ? s : "");
    free(s);
}

json_t *w15_gatewayrol_519c95_checkpoint_rows(const struct json_t *result) {
    json_t *out = json_array();
    json_t *cps = json_obj_get(result, "checkpoints");
    if (!json_truthy(cps) || !json_is_array(cps)) return out;
    for (size_t i = 0; i < json_len(cps); i++) {
        json_t *c = json_get(cps, i);
        if (!json_is_object(c)) continue;
        json_t *row = json_object();
        const json_t *h = json_obj_get(c, "hash");
        json_set(row, "hash", h ? json_copy(h) : json_string(""));
        char short_hash[8];
        short_hash[0] = '\0';
        if (json_truthy(h) && h->type == JSON_STRING) {
            size_t n = strlen(h->str_val ? h->str_val : "");
            if (n > 7) n = 7;
            if (n) memcpy(short_hash, h->str_val, n);
            short_hash[n] = '\0';
        }
        json_set(row, "short_hash", json_string(short_hash));
        const json_t *ts = json_obj_get(c, "timestamp");
        json_set(row, "timestamp", ts ? json_copy(ts) : json_string(""));
        const json_t *msg = json_obj_get(c, "message");
        json_set(row, "reason", msg ? json_copy(msg) : json_string(""));
        json_append(out, row);
    }
    return out;
}

/* Split on \n / \r (\r\n is one break); drop the trailing empty segment. */
static size_t split_lines(const char *text, char ***lines_out) {
    char **lines = NULL;
    size_t n = 0, cap = 0;
    const char *p = text;
    const char *start = p;
    while (*p) {
        if (*p == '\n' || *p == '\r') {
            size_t len = (size_t)(p - start);
            if (n == cap) { cap = cap ? cap * 2 : 8; char **t = realloc(lines, cap * sizeof(char *)); if (t) lines = t; }
            char *line = (char *)malloc(len + 1);
            if (line) { if (len) memcpy(line, start, len); line[len] = '\0'; }
            if (n < cap) lines[n++] = line;
            if (*p == '\r' && p[1] == '\n') p++;
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (p > start) {
        size_t len = (size_t)(p - start);
        if (n == cap) { cap = cap ? cap * 2 : 8; char **t = realloc(lines, cap * sizeof(char *)); if (t) lines = t; }
        char *line = (char *)malloc(len + 1);
        if (line) { if (len) memcpy(line, start, len); line[len] = '\0'; }
        if (n < cap) lines[n++] = line;
    }
    *lines_out = lines;
    return n;
}

static void free_lines(char **lines, size_t n) {
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

json_t *w15_gatewayrol_519c95_diff(const struct json_t *result) {
    json_t *out = json_array();
    const json_t *st = json_obj_get(result, "stat");
    const json_t *df = json_obj_get(result, "diff");
    int have_stat = json_truthy(st), have_diff = json_truthy(df);
    if (!have_stat && !have_diff) {
        json_append(out, json_string("  cli.commands.rollback.no_changes_since"));
        return out;
    }
    if (have_stat) {
        char s[4096], line[4200];
        py_str(st, s, sizeof(s));
        snprintf(line, sizeof(line), "\n%s", s);
        json_append(out, json_string(line));
    }
    if (have_diff) {
        char d[65536];
        py_str(df, d, sizeof(d));
        char **lines = NULL;
        size_t n = split_lines(d, &lines);
        if (n > 80) {
            size_t cap = 1;
            for (size_t i = 0; i < 80; i++) cap += strlen(lines[i]) + 1;
            char *joined = (char *)malloc(cap);
            if (joined) {
                joined[0] = '\0';
                for (size_t i = 0; i < 80; i++) {
                    if (i) strcat(joined, "\n");
                    strcat(joined, lines[i]);
                }
                json_append(out, json_string(joined));
                free(joined);
            }
            char more[256];
            snprintf(more, sizeof(more), "\n  cli.commands.rollback.more_lines|count=%zu", n - 80);
            json_append(out, json_string(more));
        } else {
            char line[66000];
            snprintf(line, sizeof(line), "\n%s", d);
            json_append(out, json_string(line));
        }
        free_lines(lines, n);
    }
    return out;
}

