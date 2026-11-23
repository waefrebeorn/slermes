/*
 * __init__.c — C home for upstream Hermes utils/__init__.py
 * Gate: w5_init_7d1849
 *
 * PoP: w5_init_7d1849_is_truthy @ utils/__init__.py:is_truthy_value
 * PoP: w5_init_7d1849_base_url_hostname @ utils/__init__.py:base_url_hostname
 * PoP: w5_init_7d1849_base_url_path @ utils/__init__.py:base_url_path
 * PoP: w5_init_7d1849_base_url_origin @ utils/__init__.py:base_url_origin
 * PoP: w5_init_7d1849_same_origin @ utils/__init__.py:same_base_url_origin
 * PoP: w5_init_7d1849_host_matches @ utils/__init__.py:base_url_host_matches
 * PoP: w5_init_7d1849_normalize_proxy @ utils/__init__.py:normalize_proxy_url
 * PoP: w5_init_7d1849_env_int @ utils/__init__.py:env_int
 * PoP: w5_init_7d1849_env_float @ utils/__init__.py:env_float
 * PoP: w5_init_7d1849_sanitize_no_proxy @ utils/__init__.py:sanitize_no_proxy_entries
 * PoP: w5_init_7d1849_all @ utils/__init__.py:__all__
 *
 * Callers import from `utils`; these are the pure helpers that package
 * re-exports. Filesystem/network helpers are deliberately not mirrored here.
 */
#include "noc_w5_init_7d1849.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static char *ui_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    size_t n = strlen(a);
    while (n && isspace((unsigned char)a[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    if (o) { memcpy(o, a, n); o[n] = '\0'; }
    return o;
}

static void ui_lower(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* ---- truthy ------------------------------------------------------------ */

int w5_init_7d1849_is_truthy(const struct json_t *value, int default_value) {
    if (!value || value->type == JSON_NULL) return default_value ? 1 : 0;
    if (value->type == JSON_BOOL) return value->bool_val ? 1 : 0;
    if (value->type == JSON_NUMBER) return value->num_val != 0.0 ? 1 : 0;
    if (value->type == JSON_STRING) {
        char *v = ui_strip(value->str_val);
        ui_lower(v);
        int yes = v && (!strcmp(v, "1") || !strcmp(v, "true") ||
                        !strcmp(v, "yes") || !strcmp(v, "on"));
        free(v);
        return yes ? 1 : 0;
    }
    if (value->type == JSON_ARRAY) return json_len(value) ? 1 : 0;
    if (value->type == JSON_OBJECT) return json_object_size(value) ? 1 : 0;
    return 1;
}

/* ---- base URL parsing -------------------------------------------------- */

typedef struct { char *scheme, *host, *path; int has_port, port, port_bad; } UiUrl;

static int ui_all_digits(const char *s, size_t n) {
    if (!n) return 0;
    for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

/* urlparse that tolerates a bare host[:port][/path]. */
static int ui_parse(const char *url, UiUrl *u) {
    memset(u, 0, sizeof(*u));
    u->scheme = u->host = u->path = NULL;
    char *raw = ui_strip(url);
    if (!raw || !*raw) { free(raw); return 0; }
    char *work;
    if (strstr(raw, "://")) work = strdup(raw);
    else {
        size_t n = strlen(raw);
        work = (char *)malloc(n + 3);
        if (work) { work[0] = '/'; work[1] = '/'; memcpy(work + 2, raw, n); work[n + 2] = '\0'; }
    }
    free(raw);
    if (!work) return 0;

    const char *auth = strstr(work, "://");
    const char *net;
    if (auth) {
        size_t sl = (size_t)(auth - work);
        u->scheme = (char *)malloc(sl + 1);
        if (u->scheme) { memcpy(u->scheme, work, sl); u->scheme[sl] = '\0'; ui_lower(u->scheme); }
        net = auth + 3;
    } else net = work + 2;

    const char *end = net;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    const char *h = net;
    for (const char *p = net; p < end; p++) if (*p == '@') h = p + 1;
    const char *hostend = end;
    const char *portstr = NULL;
    if (*h == '[') {
        const char *rb = memchr(h, ']', (size_t)(end - h));
        if (rb) {
            h++;                 /* drop the '[' — urlparse.hostname has no brackets */
            hostend = rb;
            const char *after = rb + 1;
            if (after < end && *after == ':') portstr = after + 1;
        }
    } else {
        const char *colon = memchr(h, ':', (size_t)(end - h));
        if (colon) { hostend = colon; portstr = colon + 1; }
    }
    size_t hl = (size_t)(hostend - h);
    while (hl && h[hl - 1] == '.') hl--;   /* _hostname_of: rstrip(".") */
    u->host = (char *)malloc(hl + 1);
    if (u->host) {
        memcpy(u->host, h, hl); u->host[hl] = '\0'; ui_lower(u->host);
    }
    if (portstr && portstr < end && *portstr) {
        size_t pl = (size_t)(end - portstr);
        if (ui_all_digits(portstr, pl)) {
            long v = strtol(portstr, NULL, 10);
            if (v <= 65535) { u->has_port = 1; u->port = (int)v; }
            else u->port_bad = 1;
        } else u->port_bad = 1;
    }
    const char *ps = (*end == '/') ? end : NULL;
    if (ps) {
        const char *pe = ps;
        while (*pe && *pe != '?' && *pe != '#') pe++;
        u->path = (char *)malloc((size_t)(pe - ps) + 1);
        if (u->path) {
            memcpy(u->path, ps, (size_t)(pe - ps));
            u->path[pe - ps] = '\0';
            ui_lower(u->path);
        }
    } else u->path = strdup("");
    free(work);
    return 1;
}

static void ui_free(UiUrl *u) {
    free(u->scheme); free(u->host); free(u->path);
    memset(u, 0, sizeof(*u));
}

char *w5_init_7d1849_base_url_hostname(const char *base_url) {
    UiUrl u;
    if (!ui_parse(base_url, &u)) return strdup("");
    char *out = strdup(u.host ? u.host : "");
    ui_free(&u);
    return out;
}

char *w5_init_7d1849_base_url_path(const char *base_url) {
    UiUrl u;
    if (!ui_parse(base_url, &u)) return strdup("");
    size_t n = strlen(u.path ? u.path : "");
    while (n && u.path[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, u.path ? u.path : "", n); out[n] = '\0'; }
    ui_free(&u);
    return out ? out : strdup("");
}

struct json_t *w5_init_7d1849_base_url_origin(const char *base_url) {
    UiUrl u;
    struct json_t *a = json_array();
    if (!ui_parse(base_url, &u)) {
        json_append(a, json_string("")); json_append(a, json_string("")); json_append(a, json_number(0));
        return a;
    }
    if (!u.host || !*u.host || u.port_bad) {
        json_append(a, json_string("")); json_append(a, json_string("")); json_append(a, json_number(0));
    } else {
        const char *scheme = u.scheme ? u.scheme : "";
        int port = u.has_port ? u.port
                 : (!strcmp(scheme, "https") ? 443 : !strcmp(scheme, "http") ? 80 : 0);
        json_append(a, json_string(scheme));
        json_append(a, json_string(u.host));
        json_append(a, json_number((double)port));
    }
    ui_free(&u);
    return a;
}

int w5_init_7d1849_same_origin(const char *a, const char *b) {
    struct json_t *oa = w5_init_7d1849_base_url_origin(a);
    struct json_t *ha = json_get(oa, 1);   /* [scheme, host, port] */
    if (!ha || ha->type != JSON_STRING || !ha->str_val || !*ha->str_val) {
        json_free(oa);
        return 0;
    }
    struct json_t *ob = w5_init_7d1849_base_url_origin(b);
    int equal = (json_len(oa) == json_len(ob));
    if (equal) {
        for (size_t i = 0; i < json_len(oa); i++) {
            struct json_t *x = json_get(oa, i), *y = json_get(ob, i);
            if (!x || !y || x->type != y->type) { equal = 0; break; }
            if (x->type == JSON_STRING &&
                strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "")) { equal = 0; break; }
            if (x->type == JSON_NUMBER && x->num_val != y->num_val) { equal = 0; break; }
        }
    }
    json_free(ob);
    json_free(oa);
    return equal ? 1 : 0;
}

int w5_init_7d1849_host_matches(const char *base_url, const char *domain) {
    char *h = w5_init_7d1849_base_url_hostname(base_url);
    char *d = ui_strip(domain);
    if (d) ui_lower(d);
    size_t dn = d ? strlen(d) : 0;
    while (dn && d[dn - 1] == '.') d[--dn] = '\0';
    int ok = 0;
    if (h && *h && dn) {
        if (!strcmp(h, d)) ok = 1;
        else {
            size_t hn = strlen(h);
            if (hn > dn && h[hn - dn - 1] == '.' && !strcmp(h + hn - dn, d)) ok = 1;
        }
    }
    free(h); free(d);
    return ok ? 1 : 0;
}

/* ---- proxy helpers ----------------------------------------------------- */

char *w5_init_7d1849_normalize_proxy(const char *url) {
    char *candidate = ui_strip(url);
    if (!candidate) candidate = strdup("");
    if (strlen(candidate) >= 8) {
        char low[9];
        memcpy(low, candidate, 8); low[8] = '\0'; ui_lower(low);
        if (!strcmp(low, "socks://")) {
            char *out = (char *)malloc(strlen(candidate) + 2); /* "socks5://" + rest + NUL */
            if (out) { strcpy(out, "socks5://"); strcat(out, candidate + 8); }
            free(candidate);
            return out ? out : strdup("");
        }
    }
    if (!*candidate) { free(candidate); return NULL; }
    return candidate;
}

struct json_t *w5_init_7d1849_env_int(const char *raw, const char *decimal_default) {
    char *v = ui_strip(raw);
    long long d = strtoll(decimal_default ? decimal_default : "0", NULL, 10);
    if (!v || !*v) { free(v); return json_number((double)d); }
    char *endp = NULL;
    errno = 0;
    long long out = strtoll(v, &endp, 10);
    while (endp && isspace((unsigned char)*endp)) endp++;
    if (errno || !endp || *endp) out = d;
    if (errno) errno = 0;
    free(v);
    return json_number((double)out);
}

struct json_t *w5_init_7d1849_env_float(const char *raw, const char *double_default) {
    char *v = ui_strip(raw);
    double d = strtod(double_default ? double_default : "0", NULL);
    if (!v || !*v) { free(v); return json_number(d); }
    char *endp = NULL;
    errno = 0;
    double out = strtod(v, &endp);
    if (endp != v + strlen(v) || errno == ERANGE) { out = d; errno = 0; }
    free(v);
    return json_number(out);
}

/* ---- NO_PROXY IPv6 rewriting ------------------------------------------- */

static int ui_hex_group(const char *s, size_t n) {
    if (!n || n > 4) return 0;
    for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int ui_count_groups(const char *s, size_t n, int *out) {
    if (!n) { *out = 0; return 1; }
    int c = 0; size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && s[j] != ':') j++;
        if (!ui_hex_group(s + i, j - i)) return 0;
        c++;
        if (j == n) break;
        i = j + 1;
        if (i == n) return 0;
    }
    *out = c;
    return 1;
}

static int ui_is_ipv6(const char *s) {
    if (!s || !strchr(s, ':')) return 0;
    const char *dc = strstr(s, "::");
    if (dc) {
        if (strstr(dc + 2, "::")) return 0;
        int l = 0, r = 0;
        if (!ui_count_groups(s, (size_t)(dc - s), &l)) return 0;
        if (!ui_count_groups(dc + 2, strlen(dc + 2), &r)) return 0;
        return (l + r < 8) ? 1 : 0;
    }
    int g = 0;
    if (!ui_count_groups(s, strlen(s), &g)) return 0;
    return g == 8;
}

static int ui_allowed_ipv6_char(char c) {
    return isdigit((unsigned char)c) || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F') || c == ':' || c == '.';
}

/* _bare_ipv6_literal: the bare IPv6 literal httpx can compile, or NULL. */
static char *ui_bare_ipv6(const char *entry) {
    char *c = ui_strip(entry);
    if (!c || !*c) { free(c); return NULL; }
    size_t n = strlen(c);
    if (c[0] == '[') {
        const char *rb = memchr(c + 1, ']', n - 1);
        if (rb) {
            size_t inner = (size_t)(rb - (c + 1));
            const char *after = rb + 1;
            if (inner && (*after == '\0' || *after == '/' || *after == ':')) {
                int ok = 1;
                for (size_t i = 0; i < inner; i++) if (!ui_allowed_ipv6_char(c[1 + i])) { ok = 0; break; }
                if (ok) {
                    char *lit = (char *)malloc(inner + 1);
                    memcpy(lit, c + 1, inner); lit[inner] = '\0';
                    if (ui_is_ipv6(lit)) { free(c); return lit; }
                    free(lit);
                }
            }
        }
    } else {
        const char *slash = strchr(c, '/');
        if (slash && slash[1]) {
            size_t pre = (size_t)(slash - c);
            int ok = 1;
            for (size_t i = 0; i < pre; i++) if (!ui_allowed_ipv6_char(c[i])) { ok = 0; break; }
            for (const char *p = slash + 1; *p; p++) if (!isdigit((unsigned char)*p)) { ok = 0; break; }
            if (ok) {
                char *lit = (char *)malloc(pre + 1);
                memcpy(lit, c, pre); lit[pre] = '\0';
                if (ui_is_ipv6(lit)) { free(c); return lit; }
                free(lit);
            }
        }
    }
    free(c);
    return NULL;
}

char *w5_init_7d1849_sanitize_no_proxy(const char *value) {
    const char *raw = value ? value : "";
    char **entries = NULL; size_t ne = 0, cap = 0;
    const char *p = raw;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',') p++;
        if (p > start) {
            if (ne == cap) {
                size_t nc = cap ? cap * 2 : 8;
                char **tmp = (char **)realloc(entries, nc * sizeof(char *));
                if (!tmp) break;
                entries = tmp; cap = nc;
            }
            if (ne < cap) {
                char *tok = (char *)malloc((size_t)(p - start) + 1);
                memcpy(tok, start, (size_t)(p - start)); tok[p - start] = '\0';
                entries[ne++] = tok;
            }
        }
        if (*p == ',') p++;
    }
    int wildcard = 0;
    for (size_t i = 0; i < ne; i++) if (!strcmp(entries[i], "*")) { wildcard = 1; break; }
    if (ne == 0 || wildcard) {
        for (size_t i = 0; i < ne; i++) free(entries[i]);
        free(entries);
        return strdup(raw);
    }
    char **out = NULL; size_t no = 0, ocap = 0;
    for (size_t i = 0; i < ne; i++) {
        char *bare = ui_bare_ipv6(entries[i]);
        const char *push = entries[i];
        if (bare) {
            int seen = 0;
            for (size_t j = 0; j < no; j++) if (!strcmp(out[j], bare)) { seen = 1; break; }
            if (seen) { free(bare); continue; }
            push = bare;
        }
        if (no == ocap) {
            size_t nc = ocap ? ocap * 2 : 8;
            char **tmp = (char **)realloc(out, nc * sizeof(char *));
            if (!tmp) { if (bare) free(bare); break; }
            out = tmp; ocap = nc;
        }
        out[no++] = strdup(push);
        if (bare) free(bare);
    }
    int same = (no == ne);
    if (same) for (size_t i = 0; i < ne; i++) if (strcmp(out[i], entries[i])) { same = 0; break; }
    char *result;
    if (same) {
        result = strdup(raw);
    } else {
        size_t total = 1;
        for (size_t i = 0; i < no; i++) total += strlen(out[i]) + 1;
        result = (char *)malloc(total);
        if (result) {
            result[0] = '\0';
            for (size_t i = 0; i < no; i++) {
                if (i) strcat(result, ",");
                strcat(result, out[i]);
            }
        }
    }
    for (size_t i = 0; i < no; i++) free(out[i]);
    free(out);
    for (size_t i = 0; i < ne; i++) free(entries[i]);
    free(entries);
    return result ? result : strdup(raw);
}

struct json_t *w5_init_7d1849_all(void) {
    static const char *const names[] = {
        "RACY_MTIME_WINDOW_NS", "TRUTHY_STRINGS", "atomic_json_write", "atomic_replace",
        "atomic_roundtrip_yaml_save", "atomic_roundtrip_yaml_update", "atomic_write_bytes",
        "atomic_write_text", "atomic_yaml_write", "base_url_host_matches", "base_url_hostname",
        "base_url_origin", "base_url_path", "content_digest", "default_new_file_mode",
        "env_float", "env_int", "env_var_enabled", "fast_safe_load", "file_cache_version",
        "file_signature", "fsync_directory", "is_truthy_value", "load_yaml_file_readonly",
        "logger", "mkstemp_beside", "mtime_is_racy", "normalize_proxy_env_vars",
        "normalize_proxy_url", "preserve_file_mode", "preserve_file_owner", "racy_content_digest",
        "read_json_or_empty", "restore_file_mode", "restore_file_owner", "rmtree_readonly",
        "safe_json_loads", "same_base_url_origin", "sanitize_no_proxy_entries",
        "unlink_files_older_than", "warn_if_credential_file_broadly_readable",
    };
    struct json_t *a = json_array();
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        json_append(a, json_string(names[i]));
    return a;
}
