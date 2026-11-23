/*
 * backup_restore.c — C port of upstream Hermes hermes_cli/backup_restore.py.
 *
 * PoP: w12_backuprest_0cd8e1_validate_backup_zip @ hermes_cli/backup_restore.py:_validate_backup_zip
 * PoP: w12_backuprest_0cd8e1_detect_prefix @ hermes_cli/backup_restore.py:_detect_prefix
 * PoP: w12_backuprest_0cd8e1_get_config_path_value @ hermes_cli/backup_restore.py:_get_config_path_value
 * PoP: w12_backuprest_0cd8e1_set_config_path_value @ hermes_cli/backup_restore.py:_set_config_path_value
 * PoP: w12_backuprest_0cd8e1_is_store_file @ hermes_cli/backup_restore.py:_is_store_file
 * PoP: w12_backuprest_0cd8e1_store_file_names @ hermes_cli/backup_restore.py:_STORE_FILE_NAMES
 *
 * Only the pure zip-name / config-path / store-name leaf helpers are ported;
 * the SQLite/restore plumbing is side-effecting and stays in the live module.
 * A `names` JSON array stands in for zipfile.namelist() in the ORACLE.
 */
#include "noc_w12_backuprest_0cd8e1.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* Python Path(n).name: last component after / or \ (trailing separators ignored). */
static void basename_of(const char *n, char *out, size_t cap) {
    out[0] = '\0';
    if (!n) return;
    size_t len = strlen(n);
    while (len > 0 && (n[len - 1] == '/' || n[len - 1] == '\\')) len--;
    size_t start = len;
    while (start > 0 && n[start - 1] != '/' && n[start - 1] != '\\') start--;
    size_t nlen = len - start;
    if (nlen >= cap) nlen = cap - 1;
    memcpy(out, n + start, nlen);
    out[nlen] = '\0';
}

struct json_t *w12_backuprest_0cd8e1_validate_backup_zip(const json_t *names) {
    json_t *o = json_object();
    if (!json_is_array(names) || json_len(names) == 0) {
        json_set(o, "ok", json_bool(false));
        json_set(o, "reason", json_string("zip archive is empty"));
        return o;
    }
    int cfg = 0, env = 0, state = 0;
    for (size_t i = 0; i < json_len(names); i++) {
        json_t *e = json_get(names, i);
        if (!e || e->type != JSON_STRING) continue;
        char base[512];
        basename_of(e->str_val, base, sizeof(base));
        if (!strcmp(base, "config.yaml")) cfg = 1;
        else if (!strcmp(base, ".env")) env = 1;
        else if (!strcmp(base, "state.db")) state = 1;
    }
    if (!cfg && !env && !state) {
        json_set(o, "ok", json_bool(false));
        json_set(o, "reason", json_string(
            "zip does not appear to be a Hermes backup "
            "(no config.yaml, .env, or state databases found)"));
    } else {
        json_set(o, "ok", json_bool(true));
        json_set(o, "reason", json_string(""));
    }
    return o;
}

/* Split Path(n).parts: components separated by / or \, dropping "" and ".". */
static size_t parts_of(const char *n, char parts[24][256]) {
    size_t np = 0;
    if (!n) return 0;
    const char *p = n;
    while (*p) {
        const char *s = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t len = (size_t)(p - s);
        if (len > 0 && !(len == 1 && s[0] == '.') && np < 24) {
            if (len >= 256) len = 255;
            memcpy(parts[np], s, len);
            parts[np][len] = '\0';
            np++;
        }
        if (*p) p++;
    }
    return np;
}

struct json_t *w12_backuprest_0cd8e1_detect_prefix(const json_t *names) {
    char firsts[24][256];
    size_t nf = 0;
    if (json_is_array(names)) {
        for (size_t i = 0; i < json_len(names); i++) {
            json_t *e = json_get(names, i);
            if (!e || e->type != JSON_STRING) continue;
            size_t sl = strlen(e->str_val);
            if (sl && e->str_val[sl - 1] == '/') continue;   /* endswith("/") */
            char parts[24][256];
            size_t np = parts_of(e->str_val, parts);
            if (np <= 1) continue;
            int seen = 0;
            for (size_t k = 0; k < nf; k++)
                if (!strcmp(firsts[k], parts[0])) { seen = 1; break; }
            if (!seen && nf < 24) {
                memcpy(firsts[nf], parts[0], strlen(parts[0]) + 1);
                nf++;
            }
        }
    }
    char res[300];
    if (nf == 1 && (!strcmp(firsts[0], ".hermes") || !strcmp(firsts[0], "hermes")))
        snprintf(res, sizeof(res), "%s/", firsts[0]);
    else
        res[0] = '\0';
    return json_string(res);
}

struct json_t *w12_backuprest_0cd8e1_get_config_path_value(const json_t *data,
                                                          const json_t *dotted) {
    const json_t *node = data;
    if (json_is_array(dotted)) {
        for (size_t i = 0; i < json_len(dotted); i++) {
            json_t *k = json_get(dotted, i);
            if (!json_is_object(node)) return json_null();
            if (!k || k->type != JSON_STRING) return json_null();
            node = json_obj_get(node, k->str_val);
            if (!node) return json_null();
        }
    }
    return json_copy(node);
}

struct json_t *w12_backuprest_0cd8e1_set_config_path_value(const json_t *data,
                                                          const json_t *dotted,
                                                          const json_t *value) {
    json_t *root = json_is_object(data) ? json_copy(data) : json_object();
    if (!json_is_array(dotted) || json_len(dotted) == 0) return root;
    json_t *node = root;
    for (size_t i = 0; i + 1 < json_len(dotted); i++) {
        json_t *k = json_get(dotted, i);
        if (!k || k->type != JSON_STRING) continue;
        json_t *child = json_obj_get(node, k->str_val);
        if (!json_is_object(child)) {
            child = json_object();
            json_set(node, k->str_val, child);
        }
        node = child;
    }
    json_t *last = json_get(dotted, json_len(dotted) - 1);
    if (last && last->type == JSON_STRING)
        json_set(node, last->str_val, json_copy(value));
    return root;
}

static int is_store_name(const char *s) {
    return s && (!strcmp(s, "state.db") || !strcmp(s, "shared-state.db"));
}

struct json_t *w12_backuprest_0cd8e1_is_store_file(const char *name,
                                                   const char *resolved_name) {
    return json_bool((is_store_name(name) || is_store_name(resolved_name)) ? true : false);
}

struct json_t *w12_backuprest_0cd8e1_store_file_names(void) {
    json_t *a = json_array();
    json_append(a, json_string("shared-state.db"));
    json_append(a, json_string("state.db"));
    return a;
}
