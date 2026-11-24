/*
 * methods_plugin_catalog.c -- real C11 port of the request body of upstream
 * Hermes tui_gateway/methods_plugin_catalog.py (`plugins.catalog.lookup`), with
 * the name rule and kill-list matcher it calls.
 *
 * PoP: w21_methodsplu_29402f_lookup @ tui_gateway/methods_plugin_catalog.py:_
 * PoP: w21_methodsplu_29402f_looks_like_catalog_name @ hermes_cli/plugins_cmd_catalog.py:looks_like_catalog_name
 * PoP: w21_methodsplu_29402f_normalize_repo @ hermes_cli/plugin_catalog.py:_normalize_repo
 * PoP: w21_methodsplu_29402f_match_removed @ hermes_cli/plugin_catalog.py:match_removed
 *
 * The catalog reader and the kill-list resolver (cache file, network fetch,
 * in-tree fallback) are the seams; the registry plumbing (`method`,
 * `_profile_scoped`, `bind_module`, `_ok`) stays upstream.
 */
#include "noc_w21_methodsplu_29402f.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const URL_SCHEMES[] = {
    "https://", "http://", "git@", "ssh://", "file://"
};
static const size_t URL_SCHEME_COUNT = sizeof(URL_SCHEMES) / sizeof(URL_SCHEMES[0]);

static char *pl_dup(const char *s)
{
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int pl_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `str(value or "")`: the falsy forms collapse to the empty string first. */
static char *pl_str_or_empty(const json_t *v)
{
    char buf[64];

    if (!pl_truthy(v)) return pl_dup("");
    switch (v->type) {
    case JSON_STRING: return pl_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return pl_dup("True");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            fabs(v->num_val) < 9007199254740992.0)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return pl_dup(buf);
    case JSON_ARRAY:
    case JSON_OBJECT: {
        char *dump = json_dumps(v, 0);
        char *out = pl_dup(dump ? dump : "");
        free(dump);
        return out;
    }
    default: break;
    }
    return pl_dup("");
}

/* Python str.strip() over ASCII whitespace, in place-safe form. */
static char *pl_strip(const char *s)
{
    const char *start = s ? s : "";
    const char *end;
    size_t n;
    char *out;

    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v')
        start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v'))
        end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

static char *pl_lower(const char *s)
{
    char *out = pl_dup(s);
    size_t i;
    if (!out) return NULL;
    for (i = 0; out[i]; i++)
        if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
    return out;
}

static int pl_name_ok(const char *text)
{
    size_t i, len;

    if (!text) return 0;
    len = strlen(text);
    /* bool(identifier) and the bare-token rule ^[a-z0-9_-]{1,64}$ */
    if (len < 1 || len > 64) return 0;
    if (strchr(text, '/') || strchr(text, '\\')) return 0;
    for (i = 0; i < URL_SCHEME_COUNT; i++)
        if (!strncmp(text, URL_SCHEMES[i], strlen(URL_SCHEMES[i]))) return 0;
    for (i = 0; i < len; i++) {
        char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

json_t *w21_methodsplu_29402f_looks_like_catalog_name(const json_t *identifier)
{
    if (!identifier || identifier->type != JSON_STRING || !identifier->str_val)
        return json_bool(0);
    return json_bool(pl_name_ok(identifier->str_val));
}

static int pl_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* _SCP_URL_RE: ^(?:[^@/\s]+@)?([^:/\\s]+):(?!//)(.+)$  -> host, path */
static int pl_scp_split(const char *text, char **host, char **path)
{
    const char *colon = strchr(text, ':');
    const char *at;
    const char *p;
    char *raw_host;

    if (!colon || colon == text) return 0;
    for (p = text; p < colon; p++)
        if (pl_is_space(*p) || *p == '/') return 0;
    at = NULL;
    for (p = text; p < colon; p++)
        if (*p == '@') {
            if (at) return 0; /* [^@/\s]+ may take at most one user part */
            at = p;
        }
    if (at) {
        if (at == text) return 0;
        raw_host = (char *)malloc((size_t)(colon - at));
        if (!raw_host) return 0;
        memcpy(raw_host, at + 1, (size_t)(colon - at - 1));
        raw_host[colon - at - 1] = '\0';
    } else {
        raw_host = (char *)malloc((size_t)(colon - text) + 1);
        if (!raw_host) return 0;
        memcpy(raw_host, text, (size_t)(colon - text));
        raw_host[colon - text] = '\0';
    }
    if (!*raw_host) { free(raw_host); return 0; }
    if (colon[1] == '\0' || (colon[1] == '/' && colon[2] == '/')) {
        free(raw_host);
        return 0;
    }
    *host = raw_host;
    *path = pl_dup(colon + 1);
    return *path != NULL;
}

/* urlsplit(text).hostname / .path for the scheme:// branch. */
static void pl_urlsplit(const char *text, char **host, char **path)
{
    const char *rest = strstr(text, "://");
    const char *authority, *slash, *at, *colon;
    char *raw;
    size_t n;

    *host = pl_dup("");
    *path = pl_dup("");
    if (!rest) return;
    rest += 3;
    if (*rest == '/') { *path = pl_dup(rest); return; }
    slash = strchr(rest, '/');
    authority = rest;
    n = slash ? (size_t)(slash - rest) : strlen(rest);
    raw = (char *)malloc(n + 1);
    if (!raw) return;
    memcpy(raw, authority, n);
    raw[n] = '\0';
    if (slash) { free(*path); *path = pl_dup(slash); }

    at = strrchr(raw, '@');
    colon = strrchr(at ? at : raw, ':');
    {
        const char *start = at ? at + 1 : raw;
        size_t len;
        char *hostname;
        if (colon && colon > start) {
            len = (size_t)(colon - start);
        } else {
            len = strlen(start);
        }
        hostname = (char *)malloc(len + 1);
        if (hostname) {
            memcpy(hostname, start, len);
            hostname[len] = '\0';
            free(*host);
            *host = pl_lower(hostname);
            free(hostname);
        }
    }
    free(raw);
}

static char *pl_normalize_path(const char *path)
{
    char *work = pl_dup(path);
    size_t n;
    char *out;

    if (!work) return NULL;
    /* path.strip("/") */
    n = strlen(work);
    {
        size_t start = 0;
        while (start < n && work[start] == '/') start++;
        while (n > start && work[n - 1] == '/') n--;
        memmove(work, work + start, n - start);
        work[n - start] = '\0';
    }
    /* .removesuffix(".git") */
    n = strlen(work);
    if (n >= 4 && !strcmp(work + n - 4, ".git")) work[n - 4] = '\0';
    /* .rstrip("/") */
    for (n = strlen(work); n > 0 && work[n - 1] == '/'; n--) work[n - 1] = '\0';
    out = pl_lower(work);
    free(work);
    return out;
}

char *w21_methodsplu_29402f_normalize_repo(const char *url)
{
    char *text = pl_strip(url);
    char *host = NULL, *path = NULL, *norm_path = NULL, *norm_host = NULL;
    char *out = NULL;
    size_t need;

    if (!text) return NULL;
    if (!pl_scp_split(text, &host, &path)) {
        free(host);
        free(path);
        host = NULL;
        path = NULL;
        if (strstr(text, "://")) {
            pl_urlsplit(text, &host, &path);
        } else {
            host = pl_dup("");
            path = pl_dup(text);
        }
    }
    if (!host || !path) goto done;
    norm_host = pl_lower(host);
    if (!norm_host) goto done;
    if (!strncmp(norm_host, "www.", 4)) {
        char *trimmed = pl_dup(norm_host + 4);
        free(norm_host);
        norm_host = trimmed;
    }
    norm_path = pl_normalize_path(path);
    if (!norm_host || !norm_path) goto done;
    need = strlen(norm_host) + strlen(norm_path) + 2;
    out = (char *)malloc(need);
    if (!out) goto done;
    if (*norm_host)
        snprintf(out, need, "%s/%s", norm_host, norm_path);
    else
        snprintf(out, need, "%s", norm_path);

done:
    free(text);
    free(host);
    free(path);
    free(norm_host);
    free(norm_path);
    return out;
}

json_t *w21_methodsplu_29402f_match_removed(const json_t *candidate, const json_t *removed)
{
    char *text, *text_repo;
    size_t i, n;
    int matched = 0;

    if (!pl_truthy(candidate)) return json_bool(0);
    if (candidate->type != JSON_STRING) return json_bool(0);
    text = pl_strip(candidate->str_val);
    if (!text) return json_bool(0);
    text_repo = w21_methodsplu_29402f_normalize_repo(text);
    n = (removed && removed->type == JSON_ARRAY) ? removed->c.count : 0;
    for (i = 0; i < n && !matched; i++) {
        const json_t *entry = removed->c.items[i];
        const char *name;
        const json_t *repo_node;
        const char *repo;

        if (!entry || entry->type != JSON_OBJECT) continue;
        name = json_get_str(entry, "name", NULL);
        if (name && !strcmp(text, name)) { matched = 1; break; }
        repo_node = json_obj_get(entry, "repo");
        if (!pl_truthy(repo_node) || repo_node->type != JSON_STRING) continue;
        repo = repo_node->str_val;
        if (repo && text_repo) {
            char *norm_repo = w21_methodsplu_29402f_normalize_repo(repo);
            if (norm_repo && !strcmp(text_repo, norm_repo)) matched = 1;
            free(norm_repo);
        }
    }
    free(text);
    free(text_repo);
    return json_bool(matched);
}

json_t *w21_methodsplu_29402f_lookup(const json_t *params, const json_t *entries,
                                     const json_t *removed)
{
    char *name = NULL, *stripped = NULL;
    json_t *out = NULL;
    const json_t *entry = NULL;
    size_t i, n;

    if (!params || params->type != JSON_OBJECT) return NULL;
    name = pl_str_or_empty(json_obj_get(params, "name"));
    if (!name) return NULL;
    stripped = pl_strip(name);
    free(name);
    if (!stripped) return NULL;

    out = json_object();
    if (!out) { free(stripped); return NULL; }

    if (!pl_name_ok(stripped)) {
        json_set(out, "status", json_string("invalid_name"));
        free(stripped);
        return out;
    }
    n = (entries && entries->type == JSON_ARRAY) ? entries->c.count : 0;
    if (n == 0) {
        json_set(out, "status", json_string("unavailable"));
        free(stripped);
        return out;
    }
    for (i = 0; i < n; i++) {
        const json_t *candidate = entries->c.items[i];
        const char *candidate_name;
        if (!candidate || candidate->type != JSON_OBJECT) continue;
        candidate_name = json_get_str(candidate, "name", NULL);
        if (candidate_name && !strcmp(candidate_name, stripped)) { entry = candidate; break; }
    }
    if (entry) {
        json_t *name_node = json_string(stripped);
        json_t *repo_node = json_string(json_get_str(entry, "repo", ""));
        int is_removed =
            (name_node && w21_methodsplu_29402f_match_removed(name_node, removed)->bool_val) ||
            (repo_node && w21_methodsplu_29402f_match_removed(repo_node, removed)->bool_val);
        json_free(name_node);
        json_free(repo_node);
        if (is_removed) {
            json_set(out, "status", json_string("unknown"));
            free(stripped);
            return out;
        }
        {
            json_t *found = json_object();
            json_set(found, "name", json_string(json_get_str(entry, "name", "")));
            json_set(found, "repo", json_string(json_get_str(entry, "repo", "")));
            json_set(found, "sha", json_string(json_get_str(entry, "sha", "")));
            json_set(found, "subdir", json_string(json_get_str(entry, "subdir", "")));
            json_set(found, "tier", json_string(json_get_str(entry, "tier", "")));
            json_set(out, "status", json_string("found"));
            json_set(out, "entry", found);
        }
        free(stripped);
        return out;
    }
    json_set(out, "status", json_string("unknown"));
    free(stripped);
    return out;
}
