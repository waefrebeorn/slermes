/*
 * plugins_provenance.c — C11 port of upstream Hermes hermes_cli/plugins_provenance.py.
 *
 * PoP: w17_pluginspro_9aefdd_classify   @ hermes_cli/plugins_provenance.py:plugins_provenance
 * PoP: w17_pluginspro_9aefdd_git_origin @ hermes_cli/plugins_provenance.py:_git_origin_url
 *
 * Two pure functions extracted from the module:
 *  - the 2x2 reconciliation (row+git=git, neither=manual, git-only=self-cloned,
 *    row-only=drift) plus saved_update_url = (row or {}).get("update_url") or None;
 *  - the .git/config fallback parser for the origin URL (the oracle disables the
 *    git executable and writes the same text into a real .git/config).
 * Python truthiness of a row dict = non-empty dict; empty {} is falsy.
 */
#include "noc_w17_pluginspro_9aefdd.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

struct json_t *w17_pluginspro_9aefdd_classify(const struct json_t *rows,
                                              const struct json_t *dirs) {
    json_t *out = json_array();
    if (!dirs || dirs->type != JSON_ARRAY) return out;

    for (size_t i = 0; i < json_len(dirs); i++) {
        const json_t *d = json_get(dirs, i);
        if (!d || d->type != JSON_OBJECT) continue;
        const char *name = json_get_str(d, "name", "");
        int has_git = json_get_bool(d, "has_git", false) ? 1 : 0;

        const json_t *row_obj = rows ? json_obj_get(rows, name) : NULL;
        int has_row = (row_obj && row_obj->type == JSON_OBJECT && json_object_size(row_obj) > 0) ? 1 : 0;

        const char *klass;
        if (has_row && has_git) klass = "git";
        else if (has_row && !has_git) klass = "drift";
        else if (has_git && !has_row) klass = "self-cloned";
        else klass = "manual";

        json_t *item = json_object();
        json_set(item, "name", json_string(name ? name : ""));
        json_set(item, "klass", json_string(klass));
        json_t *saved = json_null();
        if (has_row) {
            const json_t *u = json_obj_get(row_obj, "update_url");
            if (truthy(u)) saved = json_copy(u);
        }
        json_set(item, "saved_update_url", saved);
        json_append(out, item);
    }
    return out;
}

/* In-place ASCII whitespace trim. Writes the terminating NUL at the new length. */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

struct json_t *w17_pluginspro_9aefdd_git_origin(const char *config_text) {
    json_t *result = json_null();
    if (!config_text) return result;
    char *text = dupstr(config_text);   /* copy out before any caller free */
    if (!text) return result;

    char *line = text;
    /* utf-8-sig: strip the UTF-8 BOM at the very start. */
    size_t n = strlen(text);
    if (n >= 3 && (unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        line = text + 3;

    int in_origin = 0;
    while (line && *line) {
        char *p = line;
        while (*p && *p != '\n' && *p != '\r') p++;
        char delim = *p;
        if (delim) *p = '\0';

        char *stripped = trim(line);
        if (!strcmp(stripped, "[remote \"origin\"]")) {
            in_origin = 1;
        } else if (stripped[0] == '[') {
            in_origin = 0;
        } else if (in_origin && !strncmp(stripped, "url", 3)) {
            char *eq = strchr(stripped, '=');
            char *value = eq ? trim(eq + 1) : (char *)"";
            if (*value) result = json_string(value);
            break;   /* Python returns on the first matching url line */
        }

        if (!delim) break;
        p++;
        if (delim == '\r' && *p == '\n') p++;
        line = p;
    }
    free(text);
    return result;
}
