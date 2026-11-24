/*
 * update_cmd_commit.c — real C11 port of selected pure helpers in upstream
 * Hermes hermes_cli/update_cmd_commit.py.
 *
 * PoP: w16_updatecmd_383a4c_debt_sha_for_move @ hermes_cli/update_cmd_commit.py:debt_sha_for_move
 * PoP: w16_updatecmd_383a4c_table @ hermes_cli/update_cmd_commit.py:_table
 * PoP: w16_updatecmd_383a4c_startup_files @ hermes_cli/update_cmd_commit.py:startup_files
 */
#include "noc_w16_updatecmd_383a4c.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const W16_MISSING_STARTUP_FILE =
    "the tree lists this startup file in [tool.hermes] update-startup-files but it is missing or could not be read";
static const char *const W16_INVALID_STARTUP_FILES =
    "pyproject.toml must be valid TOML whose [tool.hermes] update-startup-files is a list of file paths";

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ws_c(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Trim in place; returns the (possibly advanced) start. */
static char *trim_ws(char *s) {
    char *e;
    while (*s && ws_c((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && ws_c((unsigned char)e[-1])) *--e = '\0';
    return s;
}

char *w16_updatecmd_383a4c_debt_sha_for_move(const char *pre, const char *target,
                                             const char *run_start_head) {
    const char *p = pre ? pre : "";
    const char *t = target ? target : "";
    if (run_start_head && run_start_head[0] && strcmp(p, run_start_head) != 0)
        return dupstr(p);
    return dupstr(t);
}

json_t *w16_updatecmd_383a4c_table(json_t *tables, json_t *keys) {
    json_t *cur = tables;
    size_t i, n = (keys && keys->type == JSON_ARRAY) ? keys->c.count : 0;
    for (i = 0; i < n; i++) {
        json_t *k = keys->c.items[i];
        if (cur && cur->type == JSON_OBJECT && k && k->type == JSON_STRING && k->str_val)
            cur = json_obj_get(cur, k->str_val);
        else
            cur = NULL;
    }
    return (cur && cur->type == JSON_OBJECT) ? json_copy(cur) : json_object();
}

/* Strip a trailing TOML comment (# outside quotes). */
static void strip_comment(char *s) {
    char quote = 0;
    for (; *s; s++) {
        if (quote) {
            if (*s == quote) quote = 0;
            else if (quote == '"' && *s == '\\' && s[1]) s++;
        } else if (*s == '"' || *s == '\'') {
            quote = *s;
        } else if (*s == '#') {
            *s = '\0';
            return;
        }
    }
}

/* Parse a single-line TOML array of non-empty quoted strings.
 * Returns 1 on success, 0 when the value is not such a list. */
static int parse_string_array(const char *value, json_t **out) {
    char *buf, *p;
    size_t len;
    json_t *arr;
    const char *v = value;
    while (*v && ws_c((unsigned char)*v)) v++;
    len = strlen(v);
    while (len && ws_c((unsigned char)v[len - 1])) len--;
    if (len < 2 || v[0] != '[' || v[len - 1] != ']') return 0;
    buf = (char *)malloc(len - 1);
    if (!buf) return 0;
    memcpy(buf, v + 1, len - 2);
    buf[len - 2] = '\0';

    arr = json_array();
    p = buf;
    for (;;) {
        char quote;
        char *item;
        size_t n = 0;
        while (*p && (ws_c((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        if (*p != '"' && *p != '\'') { free(buf); json_free(arr); return 0; }
        quote = *p++;
        item = (char *)malloc(strlen(p) + 1);
        if (!item) { free(buf); json_free(arr); return 0; }
        while (*p && *p != quote) {
            if (quote == '"' && *p == '\\' && p[1]) {
                p++;
                switch (*p) {
                    case 'n': item[n++] = '\n'; break;
                    case 'r': item[n++] = '\r'; break;
                    case 't': item[n++] = '\t'; break;
                    default:  item[n++] = *p;   break;
                }
                p++;
            } else {
                item[n++] = *p++;
            }
        }
        if (*p != quote || n == 0) { free(item); free(buf); json_free(arr); return 0; }
        p++;
        item[n] = '\0';
        json_append(arr, json_string(item));
        free(item);
    }
    free(buf);
    *out = arr;
    return 1;
}

json_t *w16_updatecmd_383a4c_startup_files(const char *pyproject, json_t *undeclared) {
    char *buf, *line;
    int in_target = 0, found = 0, invalid = 0;
    json_t *listed = NULL, *files, *out;

    if (!pyproject || !pyproject[0]) {
        out = json_object();
        json_set(out, "files", undeclared ? json_copy(undeclared) : json_array());
        json_set(out, "required", json_bool(0));
        return out;
    }
    buf = dupstr(pyproject);
    if (!buf) return json_null();

    line = buf;
    while (line) {
        char *next = strchr(line, '\n');
        char *p;
        if (next) { *next = '\0'; next++; }
        p = trim_ws(line);
        if (*p && *p != '#') {
            if (*p == '[') {
                char *close = strchr(p, ']');
                char *name;
                if (!close) { invalid = 1; break; }
                *close = '\0';
                name = p + 1;
                if (*name == '[') name++;   /* [[array of tables]] */
                name = trim_ws(name);
                in_target = (strcmp(name, "tool.hermes") == 0);
            } else {
                char *eq = strchr(p, '=');
                if (!eq) { invalid = 1; break; }
                *eq = '\0';
                {
                    char *key = trim_ws(p);
                    char *val = trim_ws(eq + 1);
                    strip_comment(val);
                    val = trim_ws(val);
                    if (in_target && strcmp(key, "update-startup-files") == 0) {
                        found = 1;
                        if (!parse_string_array(val, &listed)) invalid = 1;
                    }
                }
            }
        }
        line = next;
    }
    free(buf);
    if (invalid) { if (listed) json_free(listed); return json_null(); }

    files = found ? listed : (undeclared ? json_copy(undeclared) : json_array());
    out = json_object();
    json_set(out, "files", files);
    json_set(out, "required", json_bool(found ? 1 : 0));
    return out;
}
