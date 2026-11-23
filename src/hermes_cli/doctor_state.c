/*
 * doctor_state.c — faithful C11 port of upstream Hermes hermes_cli/doctor_state.py.
 *
 * PoP: w9_doctorstate_855d6c_format_bytes @ hermes_cli/doctor_state.py:_render_state_db_stats
 * PoP: w9_doctorstate_855d6c_render @ hermes_cli/doctor_state.py:_render_state_db_stats
 * PoP: w9_doctorstate_855d6c_is_malformed @ hermes_cli/doctor_state.py:_classify_unreadable_state_db
 *
 * _render_state_db_stats is the module's pure formatting core (real doctor
 * I/O is injected by the oracle's stubs). format_bytes is the same helper it
 * imports from hermes_cli/sizefmt.py; is_malformed mirrors the marker test in
 * _classify_unreadable_state_db with the module's _MALFORMED_DB_MARKERS.
 */
#define _GNU_SOURCE
#include "noc_w9_doctorstate_855d6c.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_DB_SIZE_WARN_BYTES 1073741824.0

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static json_t *field(const json_t *obj, const char *key) {
    return json_is_object(obj) ? json_obj_get(obj, key) : NULL;
}

static int is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static double num(const json_t *v) { return is_num(v) ? v->num_val : 0.0; }

/* Python str() for the scalar/container values interpolated by the f-strings. */
static void py_str_into(char *buf, size_t cap, const json_t *v) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == trunc(v->num_val)) snprintf(buf, cap, "%lld", (long long)v->num_val);
            else snprintf(buf, cap, "%g", v->num_val);
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            size_t used = 1; buf[0] = '['; buf[1] = '\0';
            for (size_t i = 0; i < json_len(v) && used + 8 < cap; i++) {
                char tmp[256]; py_str_into(tmp, sizeof tmp, json_get(v, i));
                int n = snprintf(buf + used, cap - used, "%s%s", i ? ", " : "", tmp);
                if (n <= 0) break;
                used += (size_t)n;
            }
            snprintf(buf + used, cap - used, "]");
            return;
        }
        default: snprintf(buf, cap, "{}"); return;
    }
}

/* f"{n:,}" for integral JSON numbers. */
static void comma_int(double d, char *buf, size_t cap) {
    long long n = (long long)d;
    unsigned long long u = n < 0 ? (unsigned long long)(-n) : (unsigned long long)n;
    char digits[64];
    snprintf(digits, sizeof digits, "%llu", u);
    size_t len = strlen(digits);
    size_t pos = 0;
    if (n < 0) buf[pos++] = '-';
    for (size_t i = 0; i < len && pos + 2 < cap; i++) {
        if (i && (len - i) % 3 == 0) buf[pos++] = ',';
        buf[pos++] = digits[i];
    }
    buf[pos] = '\0';
}

static char *format_bytes_alloc(const char *input); /* local helper, not exported */

static char *fmt_bytes_value(const json_t *v) {
    if (!v) return strdup("?");
    char buf[128];
    if (v->type == JSON_NUMBER) snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
    else if (v->type == JSON_STRING) snprintf(buf, sizeof buf, "%s", v->str_val ? v->str_val : "");
    else return strdup("?");
    return format_bytes_alloc(buf);
}

static char *format_bytes_alloc(const char *input) {
    if (!input) return strdup("?");
    char *end = NULL;
    double size = strtod(input, &end);
    if (end == input || *end != '\0') return strdup("?");
    if (size < 1024.0) {
        char buf[64];
        snprintf(buf, sizeof buf, "%d B", (int)size);
        return strdup(buf);
    }
    static const char *units[] = {"KB", "MB", "GB"};
    for (int i = 0; i < 3; i++) {
        size /= 1024.0;
        if (size < 1024.0) {
            char buf[64];
            snprintf(buf, sizeof buf, "%.1f %s", size, units[i]);
            return strdup(buf);
        }
    }
    char buf[64];
    snprintf(buf, sizeof buf, "%.1f TB", size / 1024.0);
    return strdup(buf);
}

json_t *w9_doctorstate_855d6c_format_bytes(const char *value) {
    char *s = format_bytes_alloc(value);
    json_t *out = json_string(s);
    free(s);
    return out;
}

static void add_row(json_t *lines, const char *kind, const char *text, const char *detail) {
    json_t *row = json_array();
    json_append(row, json_string(kind));
    json_append(row, json_string(text));
    json_append(row, json_string(detail));
    json_append(lines, row);
}

/* join non-null fragments with ", " */
static void join_bits(char *out, size_t cap, char **bits, size_t n) {
    out[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        if (!bits[i]) continue;
        int w = snprintf(out + used, cap - used, "%s%s", used ? ", " : "", bits[i]);
        if (w > 0) used += (size_t)w;
        free(bits[i]);
    }
}

json_t *w9_doctorstate_855d6c_render(const json_t *stats, const json_t *holders,
                                     const char *host_note) {
    json_t *lines = json_array();
    json_t *logical = field(stats, "logical_size_bytes");
    json_t *wal = field(stats, "wal_size_bytes");
    json_t *freelist = field(stats, "freelist_count");
    json_t *page_count = field(stats, "page_count");
    const char *hn = host_note ? host_note : "";

    char *size_bits[4] = {0};
    size_t nsb = 0;
    if (logical) {
        char *b = fmt_bytes_value(logical);
        char tmp[512]; snprintf(tmp, sizeof tmp, "logical size %s", b); free(b);
        size_bits[nsb++] = strdup(tmp);
    }
    if (page_count) {
        char cm[64]; comma_int(num(page_count), cm, sizeof cm);
        char tmp[512]; snprintf(tmp, sizeof tmp, "%s pages", cm);
        size_bits[nsb++] = strdup(tmp);
    }
    if (freelist) {
        char cm[64]; comma_int(num(freelist), cm, sizeof cm);
        char tmp[512]; snprintf(tmp, sizeof tmp, "%s free", cm);
        size_bits[nsb++] = strdup(tmp);
    }
    if (wal) {
        char *b = fmt_bytes_value(wal);
        char tmp[512]; snprintf(tmp, sizeof tmp, "WAL %s", b); free(b);
        size_bits[nsb++] = strdup(tmp);
    }
    char size_line[2048];
    join_bits(size_line, sizeof size_line, size_bits, nsb);
    if (size_line[0]) {
        char text[2300];
        snprintf(text, sizeof text, "state.db %s", size_line);
        add_row(lines, "info", text, "");
    }

    char *row_bits[4] = {0};
    size_t nrb = 0;
    json_t *messages = field(stats, "messages");
    if (messages) {
        char cm[64]; comma_int(num(messages), cm, sizeof cm);
        char tmp[512]; snprintf(tmp, sizeof tmp, "%s messages", cm);
        row_bits[nrb++] = strdup(tmp);
    }
    json_t *sessions = field(stats, "sessions");
    if (sessions) {
        char cm[64]; comma_int(num(sessions), cm, sizeof cm);
        char tmp[512]; snprintf(tmp, sizeof tmp, "%s sessions", cm);
        row_bits[nrb++] = strdup(tmp);
    }
    json_t *journal = field(stats, "journal_mode");
    if (py_truthy(journal)) {
        char js[512]; py_str_into(js, sizeof js, journal);
        char tmp[600]; snprintf(tmp, sizeof tmp, "journal_mode=%s", js);
        row_bits[nrb++] = strdup(tmp);
    }
    if (holders && holders->type != JSON_NULL) {
        char hs[512]; py_str_into(hs, sizeof hs, holders);
        char tmp[1024]; snprintf(tmp, sizeof tmp, "%s process(es) holding the DB open%s", hs, hn);
        row_bits[nrb++] = strdup(tmp);
    }
    char row_line[2300];
    join_bits(row_line, sizeof row_line, row_bits, nrb);
    if (row_line[0]) add_row(lines, "info", row_line, "");

    json_t *fts = field(stats, "fts_tables");
    if (py_truthy(fts) && fts->type == JSON_OBJECT) {
        char present[2048];
        present[0] = '\0';
        size_t used = 0;
        for (size_t i = 0; i < json_object_size(fts); i++) {
            json_t *val = json_object_get_at(fts, i);
            if (!py_truthy(val)) continue;
            const char *k = json_object_get_key_at(fts, i);
            int n = snprintf(present + used, sizeof present - used, "%s%s", used ? ", " : "", k ? k : "");
            if (n > 0) used += (size_t)n;
        }
        char text[2200];
        snprintf(text, sizeof text, "FTS tables: %s", used ? present : "none");
        add_row(lines, "info", text, "");
    }

    json_t *deferral = field(stats, "fts_rebuild_deferral");
    if (deferral && deferral->type == JSON_OBJECT) {
        json_t *pids = json_obj_get(deferral, "holder_pids");
        char ps[512];
        if (py_truthy(pids)) py_str_into(ps, sizeof ps, pids);
        else snprintf(ps, sizeof ps, "unknown");
        if (py_truthy(json_obj_get(deferral, "futile"))) {
            json_t *att = json_obj_get(deferral, "holders_attempts");
            char as[128];
            if (py_truthy(att)) py_str_into(as, sizeof as, att); else snprintf(as, sizeof as, "?");
            char text[1200], detail[1400];
            snprintf(text, sizeof text,
                     "state.db FTS repair is blocked by the same holder(s) PID(s) %s for %s consecutive deferral(s); waiting is futile",
                     ps, as);
            snprintf(detail, sizeof detail,
                     "(stop ONLY the listed process(es) — the host gateway keeps running and its own retry rebuilds within a minute of the holder leaving)");
            add_row(lines, "warn", text, detail);
        } else {
            json_t *att = json_obj_get(deferral, "attempts");
            char as[128];
            if (py_truthy(att)) py_str_into(as, sizeof as, att); else snprintf(as, sizeof as, "?");
            char text[1200], detail[1600];
            snprintf(text, sizeof text,
                     "state.db FTS repair is blocked after %s deferral(s) by PID(s) %s", as, ps);
            snprintf(detail, sizeof detail,
                     "(stop the listed processes; the host gateway's own retry then rebuilds, or run 'hermes sessions optimize-storage' with every holder stopped)");
            add_row(lines, "warn", text, detail);
        }
    }

    if (is_num(logical) && logical->num_val > STATE_DB_SIZE_WARN_BYTES) {
        char *b = fmt_bytes_value(logical);
        char detail[1400];
        snprintf(detail, sizeof detail,
                 "consider enabling sessions.auto_prune in config.yaml to bound growth");
        if (py_truthy(field(stats, "fts_rebuild_pending")) ||
            py_truthy(field(stats, "fts_teardown_pending")) ||
            py_truthy(field(stats, "fts_storage_upgrade_needed")))
            strcat(detail, "; run 'hermes sessions optimize-storage' offline (with the host gateway stopped) to compact FTS storage");
        char text[512], wrapped[1500];
        snprintf(text, sizeof text, "state.db is large (%s)", b);
        snprintf(wrapped, sizeof wrapped, "(%s)", detail);
        add_row(lines, "warn", text, wrapped);
        free(b);
    }
    return lines;
}

int w9_doctorstate_855d6c_is_malformed(const char *exc) {
    if (!exc) return 0;
    static const char *markers[] = {"malformed database schema", "database disk image is malformed"};
    char low[2048];
    snprintf(low, sizeof low, "%s", exc);
    for (size_t i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    for (int i = 0; i < 2; i++) if (strstr(low, markers[i])) return 1;
    return 0;
}
