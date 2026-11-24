/*
 * desktop_update_verify.c -- real C11 port of
 * hermes_cli/desktop_update_verify.py (gate w21_desktopupd_4b3eb1): read-only
 * verification at the Windows Desktop hand-off receipt boundary.
 *
 * PoP: w21_desktopupd_4b3eb1_root_to_verify @ hermes_cli/desktop_update_verify.py:_root_to_verify
 * PoP: w21_desktopupd_4b3eb1_asar_header_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * PoP: w21_desktopupd_4b3eb1_asar_entry_path_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * PoP: w21_desktopupd_4b3eb1_asar_entry_check @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * PoP: w21_desktopupd_4b3eb1_renderer_has_module_entry @ hermes_cli/desktop_update_verify.py:_verify_packaged_entry
 * PoP: w21_desktopupd_4b3eb1_verify @ hermes_cli/desktop_update_verify.py:verify_windows_desktop_update
 *
 * The verification walks real bytes: the ASAR Pickle header, an entry's path
 * and range, and the renderer's index.html. Each of those readers is ported
 * here as its own verdict, so `verify` takes the answers a read produced
 * (`integrity_error`, `entry_error`) instead of re-reading the archive. The
 * regexes `_HTML_TAG_WITH_URL` and `_MODULE_TAG` come from
 * hermes_cli/main_desktop.py and are reproduced in the scanner below.
 */
#include "hermes_json.h"
#include "noc_w21_desktopupd_4b3eb1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dsk4b1_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *dsk4b1_join(const char *a, const char *b) {
    size_t al = a ? strlen(a) : 0, bl = b ? strlen(b) : 0;
    char *out = (char *)malloc(al + bl + 1);
    if (!out) return NULL;
    if (al) memcpy(out, a, al);
    if (bl) memcpy(out + al, b, bl);
    out[al + bl] = '\0';
    return out;
}

/* Case-insensitive prefix (the regexes below compile with re.IGNORECASE). */
static int dsk4b1_ci_prefix(const char *s, const char *lit) {
    for (; *lit; s++, lit++) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*lit)) return 0;
    }
    return 1;
}

static int dsk4b1_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

char *w21_desktopupd_4b3eb1_root_to_verify(const char *project_root, const char *checkout_root,
                                            int has_packaged) {
    if (!project_root || !project_root[0]) return dsk4b1_dup(checkout_root ? checkout_root : "");
    if (has_packaged) return dsk4b1_dup(project_root);
    return dsk4b1_dup(checkout_root ? checkout_root : "");
}

char *w21_desktopupd_4b3eb1_asar_header_check(const char *size, const char *header_size,
                                               const char *payload_size, const char *json_size,
                                               const char *file_size) {
    long long sz = size ? strtoll(size, NULL, 10) : 0;
    long long hs = header_size ? strtoll(header_size, NULL, 10) : 0;
    long long ps = payload_size ? strtoll(payload_size, NULL, 10) : 0;
    long long js = json_size ? strtoll(json_size, NULL, 10) : 0;
    long long fs = file_size ? strtoll(file_size, NULL, 10) : 0;
    if (sz != 4 || hs != ps + 4 || ps != 4 + ((js + 3) / 4) * 4 ||
        !(js > 0 && js <= 64LL * 1024 * 1024) || 8 + hs > fs) {
        return dsk4b1_dup("invalid ASAR header");
    }
    return dsk4b1_dup("");
}

static void dsk4b1_rm_prefix(char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) memmove(s, s + n, len - n + 1);
    else s[0] = '\0';
}

char *w21_desktopupd_4b3eb1_asar_entry_path_check(const char *name) {
    const char *p;
    int dotdot = 0;
    if (!name || !name[0]) return dsk4b1_dup("invalid ASAR entry path");
    if (name[0] == '/') return dsk4b1_dup("invalid ASAR entry path");
    if (strchr(name, '\\') || strchr(name, ':')) return dsk4b1_dup("invalid ASAR entry path");
    /* ".." in PurePosixPath(name).parts: a part equals ".." exactly. */
    for (p = name; *p;) {
        const char *start = p;
        size_t len;
        while (*p && *p != '/') p++;
        len = (size_t)(p - start);
        if (len == 2 && start[0] == '.' && start[1] == '.') dotdot = 1;
        while (*p == '/') p++;
    }
    if (dotdot) return dsk4b1_dup("invalid ASAR entry path");
    return dsk4b1_dup("");
}

char *w21_desktopupd_4b3eb1_asar_entry_check(const char *name, const char *length,
                                              const char *header_size, const char *offset,
                                              int unpacked, const char *file_size,
                                              const char *read_len, int data_blank) {
    char *path_verdict = w21_desktopupd_4b3eb1_asar_entry_path_check(name);
    long long len = length ? strtoll(length, NULL, 10) : 0;
    long long hs = header_size ? strtoll(header_size, NULL, 10) : 0;
    long long off = offset ? strtoll(offset, NULL, 10) : 0;
    long long fs = file_size ? strtoll(file_size, NULL, 10) : 0;
    long long got = read_len ? strtoll(read_len, NULL, 10) : 0;
    char *out;
    if (path_verdict[0]) return path_verdict;
    free(path_verdict);
    if (!length || len <= 0) {
        out = dsk4b1_join("empty ASAR entry: ", name ? name : "");
        return out;
    }
    if (!unpacked && (off < 0 || 8 + hs + off + len > fs)) {
        return dsk4b1_join("truncated ASAR entry: ", name ? name : "");
    }
    if (got != len || data_blank) {
        return dsk4b1_join("incomplete ASAR entry: ", name ? name : "");
    }
    return dsk4b1_dup("");
}

/* `\b(?:type|rel)=["']module(preload)?["']` inside one tag. */
static int dsk4b1_module_tag(const char *tag, const char *end) {
    const char *r;
    for (r = tag; r < end; r++) {
        const char *v;
        char quote;
        if (r > tag && dsk4b1_word(r[-1])) continue;
        if (dsk4b1_ci_prefix(r, "type=")) v = r + 5;
        else if (dsk4b1_ci_prefix(r, "rel=")) v = r + 4;
        else continue;
        if (v >= end || (*v != '"' && *v != '\'')) continue;
        quote = *v;
        v++;
        if (dsk4b1_ci_prefix(v, "module")) {
            const char *after = v + 6;
            if (after < end && *after == quote) return 1;
            if (dsk4b1_ci_prefix(after, "preload") && after + 7 < end && after[7] == quote) return 1;
        }
    }
    return 0;
}

/* `^[a-z]+:` or `^//` -- a URL a renderer would load from somewhere else. */
static int dsk4b1_url_external(const char *url, size_t len) {
    size_t i = 0;
    if (len >= 2 && url[0] == '/' && url[1] == '/') return 1;
    while (i < len && ((url[i] >= 'a' && url[i] <= 'z') || (url[i] >= 'A' && url[i] <= 'Z'))) i++;
    return i > 0 && i < len && url[i] == ':';
}

int w21_desktopupd_4b3eb1_renderer_has_module_entry(const char *html) {
    const char *p;
    if (!html) return 0;
    for (p = html; *p; p++) {
        const char *tag, *name_end, *gt, *r, *url_start = NULL;
        size_t url_len = 0;
        if (*p != '<') continue;
        tag = p;
        if (dsk4b1_ci_prefix(p, "<script")) name_end = p + 7;
        else if (dsk4b1_ci_prefix(p, "<link")) name_end = p + 5;
        else continue;
        if (dsk4b1_word(*name_end)) continue;      /* the `\b` after the tag name */
        gt = strchr(name_end, '>');
        if (!gt) continue;
        /* `[^>]*\b(?:src|href)=["']([^"']+)["']`: greedy, so the last one wins */
        for (r = name_end; r + 4 <= gt; r++) {
            const char *v, *close;
            char quote;
            if (r > tag && dsk4b1_word(r[-1])) continue;
            if (dsk4b1_ci_prefix(r, "src=")) v = r + 4;
            else if (dsk4b1_ci_prefix(r, "href=")) v = r + 5;
            else continue;
            if (v >= gt || (*v != '"' && *v != '\'')) continue;
            quote = *v;
            v++;
            close = (const char *)memchr(v, quote, (size_t)(gt - v));
            if (!close) continue;
            url_start = v;
            url_len = (size_t)(close - v);
        }
        if (!url_start) continue;
        if (!dsk4b1_ci_prefix(tag, "<script")) continue;   /* tag.lower().startswith("<script") */
        if (!dsk4b1_module_tag(name_end, gt)) continue;
        if (dsk4b1_url_external(url_start, url_len)) continue;
        return 1;
    }
    return 0;
}

static json_t *dsk4b1_ok(void) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    return out;
}

static json_t *dsk4b1_raised(const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("RuntimeError"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* packaged[root] as a string, NULL when absent or null. */
static const char *dsk4b1_packaged(const json_t *map, const char *root) {
    json_t *v;
    if (!map || map->type != JSON_OBJECT || !root) return NULL;
    v = json_obj_get(map, root);
    if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) return NULL;
    return v->str_val;
}

json_t *w21_desktopupd_4b3eb1_verify(const json_t *state) {
    const char *project_root = json_get_str(state, "project_root", NULL);
    const char *checkout_root = json_get_str(state, "checkout_root", "");
    json_t *packaged = json_obj_get(state, "packaged");
    const char *root, *exe, *integrity, *entry;
    char *chosen;
    if (!project_root || !project_root[0]) project_root = NULL;

    chosen = w21_desktopupd_4b3eb1_root_to_verify(project_root, checkout_root,
                                                  dsk4b1_packaged(packaged, project_root) != NULL);
    root = chosen;
    exe = dsk4b1_packaged(packaged, root);
    if (!exe) {
        free(chosen);
        return dsk4b1_raised("The updated Desktop executable is missing");
    }
    integrity = json_get_str(state, "integrity_error", "");
    if (integrity && integrity[0]) {
        json_t *out;
        char *message = dsk4b1_join("The updated Desktop executable is invalid: ", integrity);
        free(chosen);
        out = dsk4b1_raised(message);
        free(message);
        return out;
    }
    entry = json_get_str(state, "entry_error", "");
    if (entry && entry[0]) {
        free(chosen);
        return dsk4b1_raised(entry);
    }
    free(chosen);
    if (json_get_bool(state, "build_needed", 0)) {
        return dsk4b1_raised("The updated Desktop build is stale, unstamped, or incomplete");
    }
    return dsk4b1_ok();
}
