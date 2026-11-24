/*
 * main_install_repair.c — C home for upstream Hermes hermes_cli/main_install_repair.py.
 *
 * PoP: maininstal_is_windows_npm_path @ hermes_cli/main_install_repair.py:_is_windows_npm_path
 * PoP: maininstal_quarantine_stamp_ms @ hermes_cli/main_install_repair.py:_quarantine_stamp_ms
 * PoP: maininstal_resolve_update_branch @ hermes_cli/main_install_repair.py:_resolve_update_branch
 * PoP: maininstal_filter_pending_shim_renames @ hermes_cli/main_install_repair.py:_filter_pending_shim_renames
 */
#include "noc_w18_maininstal_b04279.h"

#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static char *dupn(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* Python str.strip() over the ASCII whitespace set. Returns a fresh string. */
static char *py_strip(const char *s) {
    static const char *WS = " \t\n\r\v\f";
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && strchr(WS, *a)) a++;
    const char *b = a + strlen(a);
    while (b > a && strchr(WS, b[-1])) b--;
    return dupn(a, (size_t)(b - a));
}

int maininstal_is_windows_npm_path(const char *npm_path) {
    if (!npm_path) npm_path = "";
    size_t n = strlen(npm_path);
    char *low = (char *)malloc(n + 1);
    if (!low) return 0;
    for (size_t i = 0; i < n; ++i) low[i] = (char)tolower((unsigned char)npm_path[i]);
    low[n] = '\0';

    int mount_alpha = 0;
    if (strncmp(low, "/mnt/", 5) == 0) {
        const char *p = low + 5;
        const char *slash = strchr(p, '/');
        size_t ml = slash ? (size_t)(slash - p) : strlen(p);
        if (ml == 1 && p[0] >= 'a' && p[0] <= 'z') mount_alpha = 1;
    }
    int suffix = 0;
    if (n >= 4) {
        const char *tail = low + n - 4;
        suffix = !strcmp(tail, ".exe") || !strcmp(tail, ".cmd") || !strcmp(tail, ".bat");
    }
    int backslash = strchr(npm_path, '\\') != NULL;
    free(low);
    return suffix || mount_alpha || backslash;
}

struct json_t *maininstal_quarantine_stamp_ms(const char *stale_name) {
    const char *s = stale_name ? stale_name : "";
    const char *last = NULL;
    for (const char *p = s;;) {
        const char *q = strstr(p, ".old.");
        if (!q) break;
        last = q;
        p = q + 1;
    }
    if (!last) return json_null();
    const char *num = last + 5;
    while (isspace((unsigned char)*num)) num++;
    size_t len = strlen(num);
    while (len && isspace((unsigned char)num[len - 1])) len--;
    if (!len) return json_null();
    char *tmp = dupn(num, len);
    if (!tmp) return json_null();
    errno = 0;
    char *end = NULL;
    long long v = strtoll(tmp, &end, 10);
    int bad = *end != '\0' || errno == ERANGE || errno == EINVAL;
    free(tmp);
    if (bad) return json_null();
    return json_number((double)v);
}

char *maininstal_resolve_update_branch(const char *branch) {
    if (!branch || !*branch) return dupstr("main");
    char *s = py_strip(branch);
    if (!s) return dupstr("main");
    if (!*s) { free(s); return dupstr("main"); }
    return s;
}

/* ntpath.normcase(ntpath.normpath(value)) subset: pathsep flatten, component
 * collapse, '.', '..', UNC/drive/root prefixes, then lower(). */
static char *nt_norm_lower(const char *value) {
    static const char PFX[] = { '\\', '?', '?', '\\', 0 };
    const char *s = value ? value : "";
    while (*s == '!') s++;
    if (strncmp(s, PFX, 4) == 0) s += 4;

    size_t n = strlen(s);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return dupstr("");
    for (size_t i = 0; i < n; ++i)
        buf[i] = (s[i] == '/') ? '\\' : (char)tolower((unsigned char)s[i]);
    buf[n] = '\0';

    size_t prefix = 0;
    int rooted = 0, drive_rel = 0, unc = 0;
    if (buf[0] == '\\' && buf[1] == '\\') {
        unc = 1; rooted = 1; prefix = 2;
        while (buf[prefix] == '\\') prefix++;
    } else if (buf[0] == '\\') {
        rooted = 1; prefix = 1;
        while (buf[prefix] == '\\') prefix++;
    } else if (isalpha((unsigned char)buf[0]) && buf[1] == ':') {
        prefix = 2;
        if (buf[2] == '\\') { rooted = 1; prefix = 3; while (buf[prefix] == '\\') prefix++; }
        else drive_rel = 1;
    }

    char **comp = NULL;
    size_t nc = 0, cc = 0;
    size_t i = prefix;
    while (i <= n) {
        size_t j = i;
        while (buf[j] && buf[j] != '\\') j++;
        size_t len = j - i;
        int skip = 0;
        if (len == 0 || (len == 1 && buf[i] == '.')) skip = 1;
        else if (len == 2 && buf[i] == '.' && buf[i + 1] == '.') {
            if (nc) nc--;
            skip = 1;
        }
        if (!skip) {
            if (nc == cc) {
                size_t nc2 = cc ? cc * 2 : 8;
                char **nn = (char **)realloc(comp, nc2 * sizeof(*nn));
                if (nn) { comp = nn; cc = nc2; }
            }
            if (nc < cc) {
                char *c = dupn(buf + i, len);
                comp[nc++] = c;
            }
        }
        if (!buf[j]) break;
        i = j + 1;
    }

    size_t cap = n + 64;
    char *out = (char *)malloc(cap);
    if (!out) { for (size_t k = 0; k < nc; ++k) free(comp[k]); free(comp); free(buf); return dupstr(""); }
    size_t olen = 0;
    if (unc) { out[olen++] = '\\'; out[olen++] = '\\'; }
    else if (rooted && prefix >= 3) { out[olen++] = buf[0]; out[olen++] = buf[1]; out[olen++] = '\\'; }
    else if (rooted) { out[olen++] = '\\'; }
    else if (drive_rel) { out[olen++] = buf[0]; out[olen++] = buf[1]; }
    int need_sep = 0;
    for (size_t k = 0; k < nc; ++k) {
        if (need_sep && olen + 1 < cap) out[olen++] = '\\';
        size_t sl = strlen(comp[k]);
        if (olen + sl + 1 < cap) { memcpy(out + olen, comp[k], sl); olen += sl; }
        need_sep = 1;
    }
    if (nc == 0 && !rooted && !drive_rel && olen + 1 < cap) out[olen++] = '.';
    out[olen] = '\0';

    for (size_t k = 0; k < nc; ++k) free(comp[k]);
    free(comp);
    free(buf);
    return out;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

struct json_t *maininstal_filter_pending_shim_renames(const struct json_t *entries,
                                                      const struct json_t *shims) {
    json_t *out = json_object();
    json_t *kept = json_array();
    size_t nsh = (shims && shims->type == JSON_ARRAY) ? json_len(shims) : 0;
    char **shset = (char **)calloc(nsh ? nsh : 1, sizeof(char *));
    for (size_t i = 0; i < nsh; ++i) shset[i] = nt_norm_lower(jstr(json_get(shims, i)));

    size_t ne = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
    long long removed = 0;
    for (size_t i = 0; i + 1 < ne; i += 2) {
        const char *sa = jstr(json_get(entries, i));
        const char *sb = jstr(json_get(entries, i + 1));
        char *sn = nt_norm_lower(sa);
        int hit = 0;
        for (size_t k = 0; k < nsh; ++k) if (!strcmp(shset[k], sn)) { hit = 1; break; }
        int drop = 0;
        if (hit) {
            char *tn = nt_norm_lower(sb);
            size_t plen = strlen(sn) + 5;
            char *pfx = (char *)malloc(plen + 1);
            if (pfx) {
                strcpy(pfx, sn);
                strcat(pfx, ".old.");
                if (!strncmp(tn, pfx, plen)) drop = 1;
                free(pfx);
            }
            free(tn);
        }
        if (drop) removed++;
        else { json_append(kept, json_string(sa)); json_append(kept, json_string(sb)); }
        free(sn);
    }
    if (ne % 2) json_append(kept, json_string(jstr(json_get(entries, ne - 1))));

    json_set(out, "kept", kept);
    json_set(out, "removed", json_number((double)removed));
    for (size_t k = 0; k < nsh; ++k) free(shset[k]);
    free(shset);
    return out;
}
