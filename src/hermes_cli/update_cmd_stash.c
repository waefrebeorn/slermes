/*
 * update_cmd_stash.c — C home for upstream Hermes hermes_cli/update_cmd_stash.py.
 *
 * PoP: w15_updatecmd_05d181_intent_to_add_paths @ hermes_cli/update_cmd_stash.py:_intent_to_add_paths
 * PoP: w15_updatecmd_05d181_porcelain_records @ hermes_cli/update_cmd_stash.py:_porcelain_records
 * PoP: w15_updatecmd_05d181_untracked_collides @ hermes_cli/update_cmd_stash.py:_untracked_collides
 * PoP: w15_updatecmd_05d181_stash_apply_failed_only_on_existing_untracked @ hermes_cli/update_cmd_stash.py:_stash_apply_failed_only_on_existing_untracked
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w15_updatecmd_05d181.h"

#define CAP 8192

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static void ascii_lower_inplace(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Iterate NUL-separated fields in a porcelain -z buffer. */
#define FOR_EACH_Z(z, zlen, field, bufsz, body) \
    do { \
        size_t _p = 0; \
        while (_p <= (zlen)) { \
            size_t _e = _p; \
            while (_e < (zlen) && (z)[_e] != '\0') _e++; \
            size_t _l = (_e - _p); \
            if (_l >= (bufsz)) _l = (bufsz) - 1; \
            memcpy((field), (z) + _p, _l); \
            (field)[_l] = '\0'; \
            body \
            if (_e >= (zlen)) break; \
            _p = _e + 1; \
        } \
    } while (0)

json_t *w15_updatecmd_05d181_intent_to_add_paths(const char *z, size_t zlen) {
    json_t *out = json_array();
    char fld[CAP];
    FOR_EACH_Z(z, zlen, fld, sizeof(fld), {
        size_t n = strlen(fld);
        if (n > 3 && fld[0] == ' ' && fld[1] == 'A') json_append(out, json_string(fld + 3));
    });
    return out;
}

json_t *w15_updatecmd_05d181_porcelain_records(const char *z, size_t zlen) {
    json_t *out = json_array();
    size_t p = 0;
    int skip = 0;
    while (p <= zlen) {
        size_t e = p;
        while (e < zlen && z[e] != '\0') e++;
        char cur[CAP];
        size_t l = e - p;
        if (l >= sizeof(cur)) l = sizeof(cur) - 1;
        memcpy(cur, z + p, l); cur[l] = '\0';
        if (skip) {
            skip = 0;
        } else if (strlen(cur) > 3) {
            json_append(out, json_string(cur));
            if (cur[0] == 'R' || cur[0] == 'C') skip = 1;
        }
        if (e >= zlen) break;
        p = e + 1;
    }
    return out;
}

static int str_in_array(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

json_t *w15_updatecmd_05d181_untracked_collides(const char *entry,
                                                const json_t *files,
                                                const json_t *dirs) {
    char path[CAP];
    const char *e = entry ? entry : "";
    size_t n = strlen(e);
    if (n >= sizeof(path)) n = sizeof(path) - 1;
    memcpy(path, e, n); path[n] = '\0';
    while (n > 0 && path[n - 1] == '/') { n--; path[n] = '\0'; }
    ascii_lower_inplace(path);
    if (str_in_array(files, path) || str_in_array(dirs, path)) return json_bool(true);
    char work[CAP];
    snprintf(work, sizeof(work), "%s", path);
    char *parts[256];
    size_t np = 0;
    char *tok = strtok(work, "/");
    while (tok && np < 256) { parts[np++] = tok; tok = strtok(NULL, "/"); }
    char prefix[CAP];
    for (size_t i = 1; i < np; i++) {
        size_t pos = 0;
        for (size_t j = 0; j < i; j++) {
            size_t tl = strlen(parts[j]);
            if (pos + tl + 2 >= sizeof(prefix)) break;
            if (pos) prefix[pos++] = '/';
            memcpy(prefix + pos, parts[j], tl); pos += tl;
        }
        prefix[pos] = '\0';
        if (str_in_array(files, prefix)) return json_bool(true);
    }
    return json_bool(false);
}

json_t *w15_updatecmd_05d181_stash_apply_failed_only_on_existing_untracked(const char *stderr_text) {
    const char *s = stderr_text ? stderr_text : "";
    int saw = 0;
    const char *p = s;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[CAP];
        {
            size_t n = len;
            if (n >= sizeof(line)) n = sizeof(line) - 1;
            memcpy(line, p, n); line[n] = '\0';
            size_t b = 0, e = n;
            while (b < e && isspace((unsigned char)line[b])) b++;
            while (e > b && isspace((unsigned char)line[e - 1])) e--;
            if (b > 0) memmove(line, line + b, e - b);
            line[e - b] = '\0';
        }
        if (line[0]) {
            if (strstr(line, "already exists, no checkout") ||
                strstr(line, "could not restore untracked files from stash")) {
                saw = 1;
            } else if (strncmp(line, "warning:", 8) != 0 && strncmp(line, "hint:", 5) != 0) {
                return json_bool(false);
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    return json_bool(saw);
}

void hermes_cli_update_cmd_stash_port_pending(void) { (void)0; }
