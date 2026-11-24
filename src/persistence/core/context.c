/*
 * context.c -- real C11 port of persistence/core/context.py (gate
 * w21_context_1bf504): the profile scope a persistence store handle is bound
 * to, its canonical home key, and the equality that key defines.
 *
 * PoP: w21_context_1bf504_expand_user @ persistence/core/context.py:_canonical_dir
 * PoP: w21_context_1bf504_canonical_dir @ persistence/core/context.py:_canonical_dir
 * PoP: w21_context_1bf504_canonical_home_key @ persistence/core/context.py:canonical_home_key
 * PoP: w21_context_1bf504_scope @ persistence/core/context.py:ProfileScope.__post_init__
 * PoP: w21_context_1bf504_scope_equal @ persistence/core/context.py:ProfileScope.__eq__
 *
 * `_canonical_dir` is `Path(path).expanduser()` then `resolve(strict=True)`
 * with a lenient `resolve(strict=False)` fallback when the folder is not on
 * disk yet. Both steps are ported: expanduser expands a leading `~` from HOME,
 * and resolve asks the filesystem for the on-disk spelling (case and symlink
 * target) of the deepest existing prefix, then appends the rest verbatim --
 * the same two-phase answer CPython's Windows `realpath` gives.
 */
#include "hermes_json.h"
#include "noc_w21_context_1bf504.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CTX1504_PATH_MAX 32768

static char *ctx1504_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ctx1504_is_sep(char c) { return c == '/' || c == '\\'; }

/* pathlib drops a trailing separator but keeps a drive root ("C:\"). */
static void ctx1504_strip_trailing_sep(char *p) {
    size_t n = strlen(p);
    while (n > 1 && ctx1504_is_sep(p[n - 1])) {
        if (n == 3 && p[0] && p[1] == ':' && ctx1504_is_sep(p[2])) break;
        p[--n] = '\0';
    }
}

/* `str(Path(path))`: both separators folded to `\`, empty and `.` parts
 * dropped, `..` parts kept, a trailing separator dropped (`Path("")` is `.`). */
static char *ctx1504_path_norm(const char *path) {
    char *out, *stack[512];
    int depth = 0, rooted = 0, i;
    char drive[3] = {0, 0, 0};
    size_t cap, used = 0;
    const char *p = path ? path : "";
    if (!p[0]) return ctx1504_dup(".");
    cap = strlen(p) + 4;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    if (p[0] && p[1] == ':') { drive[0] = p[0]; drive[1] = ':'; p += 2; }
    if (ctx1504_is_sep(*p)) { rooted = 1; while (ctx1504_is_sep(*p)) p++; }
    for (;;) {
        const char *start = p;
        size_t len;
        while (*p && !ctx1504_is_sep(*p)) p++;
        len = (size_t)(p - start);
        while (ctx1504_is_sep(*p)) p++;
        if (len == 0) {
            if (!*p) break;
            continue;
        }
        if (len == 1 && start[0] == '.') { if (!*p) break; continue; }
        if (depth < (int)(sizeof(stack) / sizeof(stack[0]))) {
            stack[depth] = (char *)malloc(len + 1);
            if (!stack[depth]) { free(out); return NULL; }
            memcpy(stack[depth], start, len);
            stack[depth][len] = '\0';
            depth++;
        }
        if (!*p) break;
    }
    if (drive[0]) {
        out[used++] = drive[0];
        out[used++] = drive[1];
        if (rooted) out[used++] = '\\';
    } else if (rooted) {
        out[used++] = '\\';
    }
    for (i = 0; i < depth; i++) {
        size_t l = strlen(stack[i]);
        if (used && out[used - 1] != '\\') out[used++] = '\\';
        memcpy(out + used, stack[i], l);
        used += l;
        free(stack[i]);
    }
    out[used] = '\0';
    if (!used) { out[used++] = '.'; out[used] = '\0'; }
    return out;
}

/* dirname/basename over a Windows path, as ntpath uses them below. */
static const char *ctx1504_base(const char *s) {
    const char *b = s, *q;
    for (q = s; *q; q++) if (ctx1504_is_sep(*q) && q[1]) b = q + 1;
    return b;
}

static char *ctx1504_dir(const char *s) {
    const char *q, *last = NULL;
    char *out;
    size_t n;
    for (q = s; *q; q++) if (ctx1504_is_sep(*q) && q[1]) last = q;
    if (!last) return ctx1504_dup("");
    n = (size_t)(last - s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ntpath.expanduser: `~` -> USERPROFILE, `~user` -> the profile directory
 * beside the current one, and unchanged when that shape is not a profile
 * directory (`target_user != USERNAME and USERNAME != basename(userhome)`). */
static char *ctx1504_expanduser_text(const char *path) {
    char *norm = ctx1504_path_norm(path);
    const char *userprofile = getenv("USERPROFILE");
    const char *username = getenv("USERNAME");
    const char *homepath = getenv("HOMEPATH");
    const char *rest;
    size_t i, n;
    char *userhome = NULL, *out;
    if (!norm) return NULL;
    if (norm[0] != '~') return norm;
    if (userprofile && userprofile[0]) {
        userhome = ctx1504_dup(userprofile);
    } else if (!homepath) {
        return norm;
    } else {
        const char *drive = getenv("HOMEDRIVE");
        size_t dl = drive ? strlen(drive) : 0;
        size_t hl = strlen(homepath);
        userhome = (char *)malloc(dl + hl + 1);
        if (!userhome) { free(norm); return NULL; }
        if (dl) memcpy(userhome, drive, dl);
        memcpy(userhome + dl, homepath, hl + 1);
    }
    if (!userhome) return norm;
    n = strlen(norm);
    i = 1;
    while (i < n && !ctx1504_is_sep(norm[i])) i++;
    if (i != 1) { /* ~user */
        char *target = (char *)malloc(i);
        if (!target) { free(norm); free(userhome); return NULL; }
        memcpy(target, norm + 1, i - 1);
        target[i - 1] = '\0';
        if (!username || strcmp(target, username)) {
            if (!username || strcmp(username, ctx1504_base(userhome))) {
                free(target);
                free(userhome);
                return norm;
            }
            {
                char *parent = ctx1504_dir(userhome);
                size_t pl, tl;
                if (!parent) { free(target); free(userhome); free(norm); return NULL; }
                pl = strlen(parent);
                tl = strlen(target);
                free(userhome);
                userhome = (char *)malloc(pl + tl + 2);
                if (!userhome) { free(parent); free(target); free(norm); return NULL; }
                memcpy(userhome, parent, pl);
                userhome[pl] = '\\';
                memcpy(userhome + pl + 1, target, tl + 1);
                free(parent);
            }
        }
        free(target);
    }
    rest = norm + i;
    {
        size_t hl = strlen(userhome), rl = strlen(rest);
        out = (char *)malloc(hl + rl + 1);
        if (!out) { free(norm); free(userhome); return NULL; }
        memcpy(out, userhome, hl);
        memcpy(out + hl, rest, rl + 1);
    }
    free(norm);
    free(userhome);
    return out;
}

/* `Path(path).expanduser()`: pathlib re-segments the expanded text, so the
 * flavour's separators and `.` parts are folded a second time. */
char *w21_context_1bf504_expand_user(const char *path) {
    char *expanded = ctx1504_expanduser_text(path);
    char *out;
    if (!expanded) return NULL;
    out = ctx1504_path_norm(expanded);
    free(expanded);
    return out;
}

/* One filesystem round-trip: the on-disk spelling of an existing path, or NULL. */
static char *ctx1504_final_path(const char *path) {
#ifdef _WIN32
    HANDLE h;
    char *buf;
    DWORD n, cap = CTX1504_PATH_MAX;
    buf = (char *)malloc(cap);
    if (!buf) return NULL;
    h = CreateFileA(path, FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        free(buf);
        return NULL;
    }
    n = GetFinalPathNameByHandleA(h, buf, cap, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (n == 0 || n >= cap) {
        free(buf);
        return NULL;
    }
    if (!strncmp(buf, "\\\\?\\UNC\\", 8)) {
        /* \\?\UNC\server\share -> \\server\share */
        char *out = (char *)malloc(strlen(buf + 6) + 1);
        if (!out) { free(buf); return NULL; }
        memcpy(out, "\\\\", 2);
        memcpy(out + 2, buf + 8, strlen(buf + 8) + 1);
        free(buf);
        return out;
    }
    if (!strncmp(buf, "\\\\?\\", 4)) {
        char *out = ctx1504_dup(buf + 4);
        free(buf);
        return out;
    }
    return buf;
#else
    return realpath(path, NULL);
#endif
}

char *w21_context_1bf504_canonical_dir(const char *path) {
    char *expanded, *full, *resolved;
    size_t cut, len;
    if (!path) path = "";
    expanded = w21_context_1bf504_expand_user(path);
    if (!expanded) return NULL;
    /* Path("") is Path("."): it resolves to the working directory. */
    if (!expanded[0]) {
        free(expanded);
        expanded = ctx1504_dup(".");
        if (!expanded) return NULL;
    }
    full = realpath(expanded, NULL); /* absolute, `.`/`..` folded */
    free(expanded);
    if (!full) return NULL;

    resolved = ctx1504_final_path(full);
    if (resolved) {
        free(full);
        ctx1504_strip_trailing_sep(resolved);
        return resolved;
    }
    /* A folder not on disk yet: resolve the deepest existing prefix and keep
     * the rest verbatim, exactly like resolve(strict=False). */
    len = strlen(full);
    cut = len;
    while (cut > 0) {
        char saved;
        char *prefix;
        size_t pl, tail;
        char *joined;
        while (cut > 0 && !ctx1504_is_sep(full[cut])) cut--;
        if (cut == 0) break;
        if (cut == 2 && full[1] == ':') cut = 3; /* never hand "C:" alone to the API */
        saved = full[cut];
        full[cut] = '\0';
        prefix = ctx1504_final_path(full);
        full[cut] = saved;
        if (prefix) {
            pl = strlen(prefix);
            tail = strlen(full + cut);
            joined = (char *)malloc(pl + tail + 1);
            if (!joined) { free(prefix); free(full); return NULL; }
            memcpy(joined, prefix, pl);
            memcpy(joined + pl, full + cut, tail + 1);
            free(prefix);
            free(full);
            ctx1504_strip_trailing_sep(joined);
            return joined;
        }
        if (cut == 0) break;
        cut--; /* step above the separator just tried */
    }
    ctx1504_strip_trailing_sep(full);
    return full;
}

char *w21_context_1bf504_canonical_home_key(const char *path) {
    char *resolved;
    size_t i;
    resolved = w21_context_1bf504_canonical_dir(path);
    if (!resolved) return NULL;
    for (i = 0; resolved[i]; i++) {
        unsigned char c = (unsigned char)resolved[i];
        if (c == '/') resolved[i] = '\\';
        else if (c < 0x80) resolved[i] = (char)tolower(c);
    }
    return resolved;
}

static json_t *ctx1504_error(const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("ValueError"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* `str(value)` for a field a fixture may carry as text or number. */
static char *ctx1504_field_text(const json_t *v) {
    char buf[40];
    if (!v) return ctx1504_dup("");
    switch (v->type) {
    case JSON_STRING:
        return ctx1504_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ctx1504_dup(buf);
    case JSON_BOOL:
        return ctx1504_dup(v->bool_val ? "True" : "False");
    case JSON_NULL:
        return ctx1504_dup("");
    default:
        return ctx1504_dup("");
    }
}

static int ctx1504_blank(const char *s) { return !s || !s[0]; }

json_t *w21_context_1bf504_scope(const char *profile_dir, const char *name) {
    char *resolved, *key;
    json_t *out;
    if (ctx1504_blank(profile_dir)) return ctx1504_error("ProfileScope.profile_dir must be non-empty");
    if (ctx1504_blank(name)) return ctx1504_error("ProfileScope.name must be non-empty");
    resolved = w21_context_1bf504_canonical_dir(profile_dir);
    if (!resolved) return ctx1504_error("ProfileScope.profile_dir must be non-empty");
    key = w21_context_1bf504_canonical_home_key(profile_dir);
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "profile_dir", json_string(resolved));
    json_set(out, "name", json_string(name));
    json_set(out, "hermes_home_key", json_string(key ? key : ""));
    free(resolved);
    free(key);
    return out;
}

json_t *w21_context_1bf504_scope_equal(const char *dir_a, const char *name_a,
                                        const char *dir_b, const char *name_b) {
    char *key_a, *key_b;
    json_t *out;
    if (ctx1504_blank(dir_a) || ctx1504_blank(name_a)) return ctx1504_error("ProfileScope.profile_dir must be non-empty");
    if (ctx1504_blank(dir_b) || ctx1504_blank(name_b)) return ctx1504_error("ProfileScope.profile_dir must be non-empty");
    key_a = w21_context_1bf504_canonical_home_key(dir_a);
    key_b = w21_context_1bf504_canonical_home_key(dir_b);
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "equal", json_bool(key_a && key_b && !strcmp(key_a, key_b)));
    free(key_a);
    free(key_b);
    return out;
}
