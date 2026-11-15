/* port_auth.c -- pure helpers ported from hermes_cli/auth.py.
 *
 * PoP: st_nonempty_str               @ hermes_cli/auth.py:_nonempty_str
 * PoP: st_is_placeholder_shape       @ hermes_cli/auth.py:_is_placeholder_shape
 * PoP: st_has_usable_secret          @ hermes_cli/auth.py:has_usable_secret
 * PoP: st_matches_key_prefix         @ hermes_cli/auth.py:_matches_key_prefix
 * PoP: st_looks_like_openrouter_key  @ hermes_cli/auth.py:looks_like_openrouter_key
 * PoP: st_usable_declared_secret     @ hermes_cli/auth.py:_usable_declared_secret
 * PoP: st_is_actual_local_base_url   @ hermes_cli/auth.py:is_actual_local_base_url
 * PoP: st_normalize_actual_base_url  @ hermes_cli/auth.py:normalize_actual_base_url
 *
 * Placeholder tables verbatim from hermes_cli/auth.py:278-286; the default base
 * URL is a parameter (locales/en-style seam) rather than a shipped literal.
 * `logger.warning` in _usable_declared_secret is a seam: it sets *warned.
 */
#include "port_auth.h"

#include "libjson/json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void str_lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static void str_rstrip_dot(char *s)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == '.')
        s[--n] = '\0';
}

/* ── _nonempty_str ──────────────────────────────────────────────────────── */
int st_nonempty_str(const char *value)
{
    if (!value)
        return 0;
    for (; *value == ' ' || *value == '\t' || *value == '\n' || *value == '\r'; value++)
        ;
    return *value != '\0';
}

/* ── _is_placeholder_shape ──────────────────────────────────────────────── */
static const char *const PLACEHOLDER_KEY_PREFIXES[] = {"sk-", "ghp_", "hf_"};

int st_is_placeholder_shape(const char *value)
{
    if (!value)
        return 0;
    char lowered[1024];
    snprintf(lowered, sizeof(lowered), "%s", value);
    str_lower(lowered);

    size_t n = strlen(lowered);
    if (strncmp(lowered, "your_", 5) == 0 && n >= 10 &&
        strcmp(lowered + n - 5, "_here") == 0)
        return 1;

    for (size_t k = 0; k < sizeof(PLACEHOLDER_KEY_PREFIXES)
                            / sizeof(PLACEHOLDER_KEY_PREFIXES[0]); k++) {
        const char *prefix = PLACEHOLDER_KEY_PREFIXES[k];
        if (strncmp(lowered, prefix, strlen(prefix)) != 0)
            continue;
        const char *tail = lowered + strlen(prefix);
        if (!*tail)
            continue;
        int all_x = 1;
        for (const char *p = tail; *p; p++) {
            if (*p != 'x') {
                all_x = 0;
                break;
            }
        }
        if (all_x)
            return 1;
    }

    /* stripped of spaces, dashes and underscores; non-empty and all 'x' */
    char stripped[1024];
    size_t w = 0;
    for (const char *p = lowered; *p && w + 1 < sizeof(stripped); p++) {
        if (*p == ' ' || *p == '-' || *p == '_')
            continue;
        stripped[w++] = *p;
    }
    stripped[w] = '\0';
    if (!w)
        return 0;
    for (size_t i = 0; i < w; i++) {
        if (stripped[i] != 'x')
            return 0;
    }
    return 1;
}

/* ── has_usable_secret ──────────────────────────────────────────────────── */
static const char *const PLACEHOLDER_SECRET_VALUES[] = {
    "*", "**", "***", "changeme", "your_api_key", "your_api_key_here",
    "your-api-key", "placeholder", "example", "dummy", "null", "none",
};

int st_has_usable_secret(const char *value, int min_length)
{
    if (!value)
        return 0;
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value);
    /* strip() */
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    /* len() in Python counts characters; secrets here are ASCII. */
    if ((int)strlen(s) < min_length)
        return 0;

    char lowered[2048];
    snprintf(lowered, sizeof(lowered), "%s", s);
    str_lower(lowered);
    for (size_t k = 0; k < sizeof(PLACEHOLDER_SECRET_VALUES)
                            / sizeof(PLACEHOLDER_SECRET_VALUES[0]); k++) {
        if (strcmp(lowered, PLACEHOLDER_SECRET_VALUES[k]) == 0)
            return 0;
    }
    return !st_is_placeholder_shape(s);
}

/* ── _matches_key_prefix / looks_like_openrouter_key ────────────────────── */
static const char *const KNOWN_PREFIX_OPENROUTER[] = {"sk-or-"};

int st_matches_key_prefix(const char *provider_id, const char *val)
{
    if (!provider_id || !val)
        return 0;
    if (strcmp(provider_id, "openrouter") != 0)
        return 0; /* the only provider with a declared prefix; rest are fail-open */
    for (size_t k = 0; k < sizeof(KNOWN_PREFIX_OPENROUTER)
                            / sizeof(KNOWN_PREFIX_OPENROUTER[0]); k++) {
        if (strncmp(val, KNOWN_PREFIX_OPENROUTER[k],
                    strlen(KNOWN_PREFIX_OPENROUTER[k])) == 0)
            return 1;
    }
    return 0;
}

int st_looks_like_openrouter_key(const char *value)
{
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value ? value : "");
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';
    return st_matches_key_prefix("openrouter", s);
}

/* ── _usable_declared_secret ────────────────────────────────────────────── */
int st_usable_declared_secret(const char *provider_id, const char *value,
                              const char *source, char *out, size_t cap,
                              int *warned)
{
    (void)source; /* only used by the warning seam */
    if (out && cap)
        out[0] = '\0'; /* defined on every path */
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value ? value : "");
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    if (!st_has_usable_secret(s, 4))
        return 0;
    if (strcmp(provider_id, "openrouter") == 0 &&
        !st_matches_key_prefix("openrouter", s)) {
        if (warned)
            *warned = 1;
        return 0;
    }
    if (out && cap)
        snprintf(out, cap, "%s", s);
    return 1;
}

/* ── urlparse hostname ──────────────────────────────────────────────────── */
/* Minimal netloc host extractor matching urllib.parse.urlparse().hostname:
 * lowercase, brackets stripped for IPv6, userinfo and port removed. Returns an
 * empty string when the URL has no netloc (urlparse().hostname is None). */
static void url_hostname(const char *url, char *out, size_t cap)
{
    if (!cap) {
        return;
    }
    out[0] = '\0';
    const char *p = url ? url : "";
    const char *authority = NULL;

    const char *scheme_end = strstr(p, "://");
    if (scheme_end) {
        authority = scheme_end + 3;
    } else if (p[0] == '/' && p[1] == '/') {
        authority = p + 2;
    } else {
        return; /* no netloc -> hostname is None */
    }

    size_t n = 0;
    for (const char *q = authority; *q && *q != '/' && *q != '?' && *q != '#'; q++)
        n++;

    /* strip userinfo: everything up to the last '@' in the authority */
    const char *host = authority;
    for (size_t i = 0; i < n; i++) {
        if (authority[i] == '@')
            host = authority + i + 1;
    }
    n = (size_t)(authority + n - host);

    if (host[0] == '[') {
        const char *close = memchr(host, ']', n);
        if (!close)
            return;
        size_t hn = (size_t)(close - host - 1);
        if (hn >= cap)
            hn = cap - 1;
        memcpy(out, host + 1, hn);
        out[hn] = '\0';
    } else {
        size_t hn = 0;
        while (hn < n && host[hn] != ':')
            hn++;
        if (hn >= cap)
            hn = cap - 1;
        memcpy(out, host, hn);
        out[hn] = '\0';
    }
    str_lower(out);
    str_rstrip_dot(out);
}

/* urlparse().path, or "" when there is no netloc and no path separator. */
static void url_path(const char *url, char *out, size_t cap)
{
    if (!cap)
        return;
    out[0] = '\0';
    const char *p = url ? url : "";
    const char *scheme_end = strstr(p, "://");
    const char *start;
    if (scheme_end) {
        start = scheme_end + 3;
        while (*start && *start != '/' && *start != '?' && *start != '#')
            start++;
    } else if (p[0] == '/' && p[1] == '/') {
        start = p + 2;
        while (*start && *start != '/' && *start != '?' && *start != '#')
            start++;
    } else {
        start = p;
    }
    const char *q = start;
    while (*q && *q != '?' && *q != '#')
        q++;
    size_t n = (size_t)(q - start);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, start, n);
    out[n] = '\0';
}

/* ── is_actual_local_base_url ───────────────────────────────────────────── */
int st_is_actual_local_base_url(const char *base_url)
{
    char host[512];
    url_hostname(base_url, host, sizeof(host));
    return strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0
        || strcmp(host, "::1") == 0 || strcmp(host, "0.0.0.0") == 0;
}

/* ── normalize_actual_base_url ──────────────────────────────────────────── */
void st_normalize_actual_base_url(const char *base_url, const char *default_url,
                                  char *out, size_t cap)
{
    char url[2048];
    snprintf(url, sizeof(url), "%s", base_url ? base_url : "");
    char *s = url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && s[n - 1] == '/')
        s[--n] = '\0';
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    if (!*s) {
        if (cap)
            snprintf(out, cap, "%s", default_url ? default_url : "");
        return;
    }

    char host[512], path[1024];
    url_hostname(s, host, sizeof(host));
    url_path(s, path, sizeof(path));
    size_t pn = strlen(path);
    while (pn && path[pn - 1] == '/')
        path[--pn] = '\0';

    int bare = (pn == 0);
    if (bare && (strcmp(host, "api.actual.inc") == 0
                 || st_is_actual_local_base_url(s))) {
        if (cap)
            snprintf(out, cap, "%s/v1", s);
        return;
    }
    if (cap)
        snprintf(out, cap, "%s", s);
}

/* ── credential-pool row helpers ──────────────────────────────────────────
 * PoP: st_credential_token_pair    @ hermes_cli/auth.py:_credential_token_pair
 * PoP: st_entry_ids                @ hermes_cli/auth.py:_entry_ids
 * PoP: st_token_pairs_by_id        @ hermes_cli/auth.py:_token_pairs_by_id
 * PoP: st_suppressed_source_list   @ hermes_cli/auth.py:_suppressed_source_list
 * PoP: st_merge_pool_row_generation @ hermes_cli/auth.py:_merge_pool_row_generation
 */

static void jstr_or_empty(json_t *obj, const char *key, char *out, size_t cap)
{
    if (out && cap)
        out[0] = '\0';
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    if (v && v->type == JSON_STRING && out && cap)
        snprintf(out, cap, "%s", v->str_val);
}

/* _credential_token_pair: (access_token, refresh_token) for a row; both empty
 * when the row is not an object. Returns 1 when *row* is an object. */
int st_credential_token_pair(json_t *row, char *access, size_t acap,
                             char *refresh, size_t rcap)
{
    jstr_or_empty(row, "access_token", access, acap);
    jstr_or_empty(row, "refresh_token", refresh, rcap);
    return row && row->type == JSON_OBJECT;
}

/* _entry_ids: id -> row, for dict rows with a truthy "id". *rows* holds the
 * borrowed element pointers; *ids* holds malloc'd copies. Caller frees ids. */
int st_entry_ids(json_t *entries, json_t **rows, char **ids, int maxn)
{
    int n = 0;
    if (!entries || entries->type != JSON_ARRAY)
        return 0;
    for (size_t i = 0; i < entries->c.count && n < maxn; i++) {
        json_t *row = entries->c.items[i];
        if (!row || row->type != JSON_OBJECT)
            continue;
        json_t *id = json_obj_get(row, "id");
        if (!id || id->type != JSON_STRING || !id->str_val || !*id->str_val)
            continue;
        rows[n] = row;
        ids[n] = strdup(id->str_val);
        if (!ids[n])
            break;
        n++;
    }
    return n;
}

/* _token_pairs_by_id: one (access, refresh) pair per row id, INCLUDING blank
 * pairs -- a blank base is a known generation, not an unknown one. */
int st_token_pairs_by_id(json_t *entries, char **ids, char **access,
                         char **refresh, int maxn)
{
    json_t *rows[256];
    int n = st_entry_ids(entries, rows, ids, maxn > 256 ? 256 : maxn);
    for (int i = 0; i < n; i++) {
        access[i] = (char *)malloc(512);
        refresh[i] = (char *)malloc(512);
        if (!access[i] || !refresh[i])
            break;
        st_credential_token_pair(rows[i], access[i], 512, refresh[i], 512);
    }
    return n;
}

/* _suppressed_source_list: canonical list-form sources for *provider_id*. A
 * legacy mapping (keys = source names) is MIGRATED to the list form in place.
 * Returns the count, or 0 when the provider has no entry. */
int st_suppressed_source_list(json_t *suppressed, const char *provider_id,
                              char **out, int maxn)
{
    if (!suppressed || suppressed->type != JSON_OBJECT)
        return 0;
    json_t *raw = json_obj_get(suppressed, provider_id);
    if (!raw)
        return 0;

    json_t *list = NULL;
    if (raw->type == JSON_ARRAY) {
        list = raw;
    } else if (raw->type == JSON_OBJECT) {
        list = json_array();
        if (!list)
            return 0;
        for (size_t i = 0; i < raw->c.count; i++)
            json_append(list, json_string(raw->c.keys[i]));
        json_set(suppressed, provider_id, list);
    } else {
        return 0;
    }

    int n = 0;
    for (size_t i = 0; i < list->c.count && n < maxn; i++) {
        json_t *item = list->c.items[i];
        if (!item || item->type != JSON_STRING || !item->str_val)
            continue;
        out[n++] = strdup(item->str_val);
        if (!out[n - 1])
            break;
    }
    return n;
}

static const char *const POOL_STATUS_FIELDS[] = {
    "last_status", "last_status_at", "last_error_code", "last_error_reason",
    "last_error_message", "last_error_reset_at", "status_cleared_at",
};
static const char *const POOL_TOKEN_GENERATION_FIELDS[] = {
    "access_token", "refresh_token", "expires_at", "expires_at_ms",
    "expires_in", "obtained_at", "last_refresh", "agent_key",
    "agent_key_expires_at", "agent_key_expires_in", "agent_key_id",
    "agent_key_obtained_at", "agent_key_reused", "scope", "inference_base_url",
};

/* _merge_pool_row_generation: keep a newer on-disk token generation
 * authoritative during stale writes. *cooldown_cb* stands in for
 * _merge_disk_cooldown_state (a seam; that merge needs wall-clock and the
 * PooledCredential model). A NULL base access or refresh means "no base".
 * Serialises the merged entry into *out*. */
char *st_merge_pool_row_generation(json_t *entry, json_t *disk,
                                   const char *provider_id,
                                   const char *base_access,
                                   const char *base_refresh,
                                   int status_cleared,
                                   json_t *(*cooldown_cb)(json_t *, json_t *,
                                                         const char *),
                                   char *out, size_t cap)
{
    json_t *merge_disk = status_cleared ? NULL : disk;

    char da[512] = "", dr[512] = "";
    st_credential_token_pair(disk, da, sizeof(da), dr, sizeof(dr));
    int disk_has_pair = (*da != '\0') || (*dr != '\0');
    int no_base = (base_access == NULL) && (base_refresh == NULL);
    int same_pair = !no_base && strcmp(da, base_access ? base_access : "") == 0
                    && strcmp(dr, base_refresh ? base_refresh : "") == 0;

    json_t *target = entry;
    json_t *owned = NULL;

    if (!no_base && disk_has_pair && !same_pair) {
        owned = json_copy(entry);
        if (!owned) {
            if (out && cap)
                out[0] = '\0';
            return NULL;
        }
        target = owned;

        for (size_t k = 0; k < sizeof(POOL_TOKEN_GENERATION_FIELDS)
                                / sizeof(POOL_TOKEN_GENERATION_FIELDS[0]); k++) {
            const char *f = POOL_TOKEN_GENERATION_FIELDS[k];
            json_t *v = (disk && disk->type == JSON_OBJECT) ? json_obj_get(disk, f)
                                                            : NULL;
            if (v)
                json_set(target, f, json_copy(v));
            else
                json_obj_del(target, f);
        }

        json_t *ls = (entry && entry->type == JSON_OBJECT)
                         ? json_obj_get(entry, "last_status") : NULL;
        int is_dead = ls && ls->type == JSON_STRING && strcmp(ls->str_val, "dead") == 0;
        if (!status_cleared && is_dead) {
            for (size_t k = 0; k < sizeof(POOL_STATUS_FIELDS)
                                    / sizeof(POOL_STATUS_FIELDS[0]); k++) {
                const char *f = POOL_STATUS_FIELDS[k];
                json_t *v = (disk && disk->type == JSON_OBJECT)
                                ? json_obj_get(disk, f) : NULL;
                if (v)
                    json_set(target, f, json_copy(v));
                else
                    json_obj_del(target, f);
            }
            const char *f = "failure_reason";
            json_t *v = (disk && disk->type == JSON_OBJECT) ? json_obj_get(disk, f)
                                                            : NULL;
            if (v)
                json_set(target, f, json_copy(v));
            else
                json_obj_del(target, f);
        }
    }

    json_t *result = cooldown_cb ? cooldown_cb(target, merge_disk, provider_id)
                                 : target;
    if (out && cap) {
        if (result) {
            char *s = json_serialize(result);
            snprintf(out, cap, "%s", s ? s : "");
            free(s);
        } else {
            snprintf(out, cap, "%s", "");
        }
    }
    if (owned)
        json_free(owned);
    return out;
}
