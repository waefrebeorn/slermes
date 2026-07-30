/*
 * port_managed_scope_helpers.c
 *
 * Pure, portable helper functions ported from hermes_cli/managed_scope.py.
 * No file I/O, no YAML parse, no deep-merge with lazy config imports, no
 * threading/cache. These take already-parsed config/env JSON as input and
 * perform pure string/set logic. Filesystem/coupled helpers
 * (get_managed_dir, invalidate_managed_cache, _cached_read, load_managed_config,
 * load_managed_env, apply_managed_overlay) stay REAL_GAP.
 *
 * C name <- python name (module prefix 'managed_scope_'):
 *   managed_scope_under_pytest          <- _under_pytest
 *   managed_scope_parse_env             <- _parse_env (from string)
 *   managed_scope_flatten_keys          <- _flatten_keys (from JSON)
 *   managed_scope_is_key_managed        <- is_key_managed (config_json, key)
 *   managed_scope_is_env_managed        <- is_env_managed (env_json, name)
 *   managed_scope_managed_config_keys   <- managed_config_keys (config_json)
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

static char *json_escape_string(const char *s)
{
    if (!s) s = "";
    size_t need = 1;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') need += 2;
        else if (c == '\n') need += 2;
        else if (c == '\r') need += 2;
        else if (c == '\t') need += 2;
        else if (c < 0x20) need += 6;
        else need += 1;
    }
    char *out = malloc(need + 1);
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') { *q++='\\'; *q++='"'; }
        else if (c == '\\') { *q++='\\'; *q++='\\'; }
        else if (c == '\n') { *q++='\\'; *q++='n'; }
        else if (c == '\r') { *q++='\\'; *q++='r'; }
        else if (c == '\t') { *q++='\\'; *q++='t'; }
        else if (c < 0x20) { sprintf(q, "\\u%04x", c); q += 6; }
        else *q++ = (char)c;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

/*
 * PoP: _under_pytest @ hermes_cli/managed_scope.py:_under_pytest */
int managed_scope_under_pytest(void)
{
    return getenv("PYTEST_CURRENT_TEST") != NULL ? 1 : 0;
}

/* ---------------------------------------------------------------------------
 * _parse_env: KEY=VALUE lines -> JSON object of strings.
 * --------------------------------------------------------------------------- */
/*
 * PoP: _parse_env @ hermes_cli/managed_scope.py:_parse_env
 * Parses a .env-style string. Returns malloc'd JSON object. Caller frees. */
char *managed_scope_parse_env(const char *text)
{
    char *out = malloc(3);
    out[0] = '{';
    out[1] = '\0';
    if (!text) return out;
    const char *p = text;
    char line[8192];
    int first = 1;
    while (*p) {
        size_t li = 0;
        while (*p && *p != '\n' && li + 1 < sizeof(line)) line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        /* strip */
        char *ls = line;
        while (*ls==' '||*ls=='\t') ls++;
        char *le = ls + strlen(ls);
        while (le > ls && (le[-1]==' '||le[-1]=='\t'||le[-1]=='\r')) le--;
        *le = '\0';
        if (!ls[0] || ls[0]=='#') continue;
        char *eq = strchr(ls, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = ls;
        while (*k==' '||*k=='\t') k++;
        char *ke = k + strlen(k);
        while (ke > k && (ke[-1]==' '||ke[-1]=='\t')) ke--;
        *ke = '\0';
        char *v = eq + 1;
        while (*v==' '||*v=='\t') v++;
        char *ve = v + strlen(v);
        while (ve > v && (ve[-1]==' '||ve[-1]=='\t')) ve--;
        /* strip surrounding quotes */
        if (ve > v && (ve[-1]=='"'||ve[-1]=='\'') && v[0]==ve[-1]) { v++; ve--; *ve='\0'; }
        *ve = '\0';
        size_t need = strlen(out) + strlen(k) + strlen(v) + 8;
        char *n = realloc(out, need + 64);
        if (!n) break;
        out = n;
        char *ek = json_escape_string(k);
        char *ev = json_escape_string(v);
        snprintf(out + strlen(out), need + 64, "%s%s:%s",
                 first ? "" : ",", ek, ev);
        free(ek); free(ev);
        first = 0;
    }
    size_t need = strlen(out) + 2;
    char *n = realloc(out, need);
    if (n) { out = n; strcat(out, "}"); }
    return out;
}

/* ---------------------------------------------------------------------------
 * _flatten_keys: dotted leaf keys of a (JSON) dict.
 * --------------------------------------------------------------------------- */
static void flatten_keys_rec(json_t *node, const char *prefix, char **buf, size_t *cap, size_t *len)
{
    if (!node || node->type != JSON_OBJECT) return;
    for (size_t i = 0; i < json_object_size(node); i++) {
        const char *k = json_object_get_key_at(node, i);
        json_t *v = json_object_get_at(node, i);
        char dotted[2048];
        if (prefix && prefix[0]) snprintf(dotted, sizeof(dotted), "%s.%s", prefix, k);
        else snprintf(dotted, sizeof(dotted), "%s", k);
        if (v && v->type == JSON_OBJECT && json_object_size(v) > 0) {
            flatten_keys_rec(v, dotted, buf, cap, len);
        } else {
            size_t need = strlen(dotted) + 2;
            if (*len + need + 1 > *cap) {
                *cap = (*len + need + 1) * 2 + 64;
                char *nw = realloc(*buf, *cap);
                if (!nw) return;
                *buf = nw;
            }
            strcat(*buf, dotted);
            strcat(*buf, "\n");
            *len += need;
        }
    }
}

/*
 * PoP: _flatten_keys @ hermes_cli/managed_scope.py:_flatten_keys
 * Takes a config JSON string; returns malloc'd newline-separated dotted keys.
 * Caller frees. (C has no set type; newline list is the faithful equivalent.) */
char *managed_scope_flatten_keys(const char *config_json)
{
    char *buf = malloc(1);
    buf[0] = '\0';
    size_t cap = 1, len = 0;
    if (config_json && config_json[0]) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg) { flatten_keys_rec(cfg, "", &buf, &cap, &len); json_free(cfg); }
    }
    return buf;
}

/* ---------------------------------------------------------------------------
 * managed_config_keys / is_key_managed / is_env_managed
 * --------------------------------------------------------------------------- */
/*
 * PoP: managed_config_keys @ hermes_cli/managed_scope.py:managed_config_keys
 * Takes config JSON; returns malloc'd JSON array of dotted leaf keys. */
char *managed_scope_managed_config_keys(const char *config_json)
{
    char *flat = managed_scope_flatten_keys(config_json);
    char *arr = malloc(strlen(flat) + 4);
    strcpy(arr, "[");
    /* split on newline into json array */
    char *p = flat;
    int first = 1;
    char *tok = strtok(p, "\n");
    while (tok) {
        char *e = json_escape_string(tok);
        size_t need = strlen(arr) + strlen(e) + 4;
        char *n = realloc(arr, need);
        if (!n) break;
        arr = n;
        strcat(arr, first ? "" : ",");
        strcat(arr, e);
        free(e);
        first = 0;
        tok = strtok(NULL, "\n");
    }
    strcat(arr, "]");
    free(flat);
    return arr;
}

/*
 * PoP: is_key_managed @ hermes_cli/managed_scope.py:is_key_managed
 * Returns 1 if dotted_key is a leaf key of config_json. */
int managed_scope_is_key_managed(const char *config_json, const char *dotted_key)
{
    char *flat = managed_scope_flatten_keys(config_json);
    int found = 0;
    char *p = flat;
    char *tok = strtok(p, "\n");
    while (tok) {
        if (strcmp(tok, dotted_key) == 0) { found = 1; break; }
        tok = strtok(NULL, "\n");
    }
    free(flat);
    return found;
}

/*
 * PoP: is_env_managed @ hermes_cli/managed_scope.py:is_env_managed
 * Returns 1 if name is a key in env_json (a JSON object of strings). */
int managed_scope_is_env_managed(const char *env_json, const char *name)
{
    if (!env_json || !env_json[0]) return 0;
    json_t *env = json_parse(env_json, NULL);
    if (!env || env->type != JSON_OBJECT) { if (env) json_free(env); return 0; }
    int found = (json_object_get(env, name) != NULL) ? 1 : 0;
    json_free(env);
    return found;
}
