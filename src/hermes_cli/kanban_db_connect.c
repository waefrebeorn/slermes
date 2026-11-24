/*
 * kanban_db_connect.c — C home for upstream Hermes hermes_cli/kanban_db_connect.py.
 *
 * PoP: w16_kanbandbc_db6090_looks_like_tls_record_at @ hermes_cli/kanban_db_connect.py:_looks_like_tls_record_at
 * PoP: w16_kanbandbc_db6090_integrity_messages_ok    @ hermes_cli/kanban_db_connect.py:_integrity_messages_ok
 * PoP: w16_kanbandbc_db6090_repairable_index_names   @ hermes_cli/kanban_db_connect.py:_repairable_index_names
 *
 * Pure byte-probe / integrity-message parsers. No SQLite, filesystem, or locks.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_kanbandbc_db6090.h"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python (raw or "").strip() on ASCII fixtures. Returns owned copy. */
static char *py_strip(const json_t *v) {
    const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

json_t *w16_kanbandbc_db6090_looks_like_tls_record_at(const json_t *data, int offset) {
    int verdict = 0;
    if (data && data->type == JSON_ARRAY && offset >= 0) {
        size_t n = json_len(data);
        if (n >= (size_t)offset + 5) {
            unsigned char ctype = (unsigned char)json_get(data, (size_t)offset)->num_val;
            unsigned char major = (unsigned char)json_get(data, (size_t)offset + 1)->num_val;
            unsigned char minor = (unsigned char)json_get(data, (size_t)offset + 2)->num_val;
            unsigned int length = ((unsigned int)(unsigned char)json_get(data, (size_t)offset + 3)->num_val << 8) |
                                   (unsigned int)(unsigned char)json_get(data, (size_t)offset + 4)->num_val;
            int ok_ct = (ctype == 0x14 || ctype == 0x15 || ctype == 0x16 || ctype == 0x17);
            int ok_mn = (minor == 0x00 || minor == 0x01 || minor == 0x02 ||
                         minor == 0x03 || minor == 0x04);
            verdict = ok_ct && major == 0x03 && ok_mn && length > 0 && length <= 18432;
        }
    }
    return json_bool(verdict);
}

json_t *w16_kanbandbc_db6090_integrity_messages_ok(const json_t *messages) {
    int ok = 0;
    if (messages && messages->type == JSON_ARRAY && json_len(messages) == 1) {
        char *s = py_strip(json_get(messages, 0));
        size_t n = strlen(s);
        for (size_t i = 0; i < n; ++i) s[i] = (char)tolower((unsigned char)s[i]);
        ok = (n == 2 && s[0] == 'o' && s[1] == 'k');
        free(s);
    }
    return json_bool(ok);
}

static char *strip_cstr(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* Match one of the two repairable patterns against an already-stripped message.
 * Returns an owned index-name copy (group("index").strip()) or NULL. */
static char *match_index_name(const char *msg) {
    static const char *p1 = "wrong # of entries in index ";
    static const char *p2 = "row ";
    static const char *p2mid = " missing from index ";
    if (!strncmp(msg, p1, strlen(p1))) {
        const char *rest = msg + strlen(p1);
        if (!*rest) return NULL;
        return strip_cstr(rest);
    }
    if (!strncmp(msg, p2, strlen(p2))) {
        const char *p = msg + strlen(p2);
        size_t digits = 0;
        while (*p >= '0' && *p <= '9') { p++; digits++; }
        if (!digits || strncmp(p, p2mid, strlen(p2mid))) return NULL;
        const char *rest = p + strlen(p2mid);
        if (!*rest) return NULL;
        return strip_cstr(rest);
    }
    return NULL;
}

json_t *w16_kanbandbc_db6090_repairable_index_names(const json_t *messages) {
    json_t *names = json_array();
    if (!messages || messages->type != JSON_ARRAY) return json_null();
    for (size_t i = 0; i < json_len(messages); ++i) {
        char *msg = py_strip(json_get(messages, i));
        if (!msg[0]) { free(msg); continue; }
        char *name = match_index_name(msg);
        free(msg);
        if (!name) { json_free(names); return json_null(); }
        int dup = 0;
        for (size_t j = 0; j < json_len(names); ++j) {
            json_t *have = json_get(names, j);
            if (have && have->type == JSON_STRING && have->str_val &&
                !strcmp(have->str_val, name)) { dup = 1; break; }
        }
        if (!dup) json_append(names, json_string(name));
        free(name);
    }
    if (json_len(names) == 0) { json_free(names); return json_null(); }
    return names;
}
