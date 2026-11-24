/*
 * ffmpeg_utils.c -- C11 port of plugins/platforms/discord/ffmpeg_utils.py.
 *
 * Everything is real here: ntpath-style `~`/`$VAR`/`%VAR%` expansion and the
 * winget package layout walk. The process environment and the repo-wide shared
 * ffmpeg lookup are injected by the caller, so the decision is testable without
 * mutating the harness environment.
 *
 * PoP: w22_ffmpegutil_86b841_expand_path @ plugins/platforms/discord/ffmpeg_utils.py:resolve_ffmpeg_executable
 * PoP: w22_ffmpegutil_86b841_resolve @ plugins/platforms/discord/ffmpeg_utils.py:resolve_ffmpeg_executable
 */
#include "hermes_json.h"
#include "noc_w22_ffmpegutil_86b841.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FU_MAX_PATH 4096

static char *fu_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *fu_trim(const char *s) {
    const char *end;
    size_t n;
    char *out;
    if (!s) return fu_dup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    end = s + strlen(s);
    while (end > s) {
        char c = end[-1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') break;
        end--;
    }
    n = (size_t)(end - s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int fu_is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* ntpath.expanduser: only a leading `~` is special, and the home comes from
 * USERPROFILE (falling back to HOMEDRIVE + HOMEPATH). */
static char *fu_expanduser(const char *path) {
    const char *home, *tail, *slash;
    char *base, *out;
    size_t n;
    if (!path || path[0] != '~') return fu_dup(path);
    home = getenv("USERPROFILE");
    if (!home || !home[0]) {
        const char *drive = getenv("HOMEDRIVE");
        const char *hpath = getenv("HOMEPATH");
        if (!drive || !hpath) return fu_dup(path);
        n = strlen(drive) + strlen(hpath);
        base = (char *)malloc(n + 1);
        if (!base) return fu_dup(path);
        memcpy(base, drive, strlen(drive));
        memcpy(base + strlen(drive), hpath, strlen(hpath) + 1);
    } else {
        base = fu_dup(home);
        if (!base) return NULL;
    }
    n = strlen(base);
    while (n > 0 && (base[n - 1] == '\\' || base[n - 1] == '/')) base[--n] = '\0';
    if (!n) {
        free(base);
        return fu_dup("\\");
    }
    slash = strchr(path + 1, '\\');
    tail = slash ? slash : path + 1;
    n = strlen(base) + strlen(tail);
    out = (char *)malloc(n + 1);
    if (!out) { free(base); return NULL; }
    memcpy(out, base, strlen(base));
    memcpy(out + strlen(base), tail, strlen(tail) + 1);
    free(base);
    return out;
}

static int fu_grow(char **out, size_t *cap, size_t need) {
    char *grown;
    if (need + 1 <= *cap) return 1;
    grown = (char *)realloc(*out, need + 1);
    if (!grown) return 0;
    *out = grown;
    *cap = need + 1;
    return 1;
}

/* ntpath.expandvars: `$NAME`, `${NAME}` and `%NAME%`; unknown names stay put. */
static char *fu_expandvars(const char *path) {
    size_t cap, len = 0, i = 0;
    char *out;
    if (!path) return fu_dup("");
    cap = strlen(path) + 1;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    while (path[i]) {
        const char *value = NULL;
        size_t name_start, name_end, width, vlen;
        char name[256];
        if (path[i] == '$' && (path[i + 1] == '{' ||
                               fu_is_name_char(path[i + 1]))) {
            name_start = i + 1;
            if (path[name_start] == '{') {
                name_start++;
                name_end = name_start;
                while (path[name_end] && path[name_end] != '}') name_end++;
                if (!path[name_end]) {         /* unterminated: literal '$' */
                    if (!fu_grow(&out, &cap, len + 1)) return NULL;
                    out[len++] = path[i++];
                    out[len] = '\0';
                    continue;
                }
                width = name_end - name_start;
                if (width && width < sizeof(name)) {
                    memcpy(name, path + name_start, width);
                    name[width] = '\0';
                    value = getenv(name);
                }
                if (!value) {                  /* leave "${...}" verbatim */
                    size_t span = name_end - i + 1;
                    if (!fu_grow(&out, &cap, len + span)) return NULL;
                    memcpy(out + len, path + i, span);
                    len += span;
                    out[len] = '\0';
                    i = name_end + 1;
                    continue;
                }
                i = name_end + 1;
            } else {
                name_end = name_start;
                while (path[name_end] && fu_is_name_char(path[name_end])) name_end++;
                width = name_end - name_start;
                if (width < sizeof(name)) {
                    memcpy(name, path + name_start, width);
                    name[width] = '\0';
                    value = getenv(name);
                }
                if (!value) {                  /* leave "$NAME" verbatim */
                    size_t span = name_end - i;
                    if (!fu_grow(&out, &cap, len + span)) return NULL;
                    memcpy(out + len, path + i, span);
                    len += span;
                    out[len] = '\0';
                    i = name_end;
                    continue;
                }
                i = name_end;
            }
            vlen = strlen(value);
            if (!fu_grow(&out, &cap, len + vlen)) return NULL;
            memcpy(out + len, value, vlen);
            len += vlen;
            out[len] = '\0';
            continue;
        }
        if (path[i] == '%') {
            name_start = i + 1;
            name_end = name_start;
            while (path[name_end] && path[name_end] != '%') name_end++;
            if (path[name_end] != '%') {
                if (!fu_grow(&out, &cap, len + 1)) return NULL;
                out[len++] = path[i++];
                out[len] = '\0';
                continue;
            }
            width = name_end - name_start;
            if (width && width < sizeof(name)) {
                memcpy(name, path + name_start, width);
                name[width] = '\0';
                value = getenv(name);
            }
            if (value) {
                vlen = strlen(value);
                if (!fu_grow(&out, &cap, len + vlen)) return NULL;
                memcpy(out + len, value, vlen);
                len += vlen;
                out[len] = '\0';
            } else {
                size_t span = name_end - i + 1;
                if (!fu_grow(&out, &cap, len + span)) return NULL;
                memcpy(out + len, path + i, span);
                len += span;
                out[len] = '\0';
            }
            i = name_end + 1;
            continue;
        }
        if (!fu_grow(&out, &cap, len + 1)) return NULL;
        out[len++] = path[i++];
        out[len] = '\0';
    }
    return out;
}

char *w22_ffmpegutil_86b841_expand_path(const char *value) {
    char *user = fu_expanduser(value);
    char *vars;
    if (!user) return NULL;
    vars = fu_expandvars(user);
    free(user);
    return vars;
}

/* PureWindowsPath.__str__: `/` becomes `\`, duplicate separators collapse, `.`
 * components drop, a trailing separator goes (except at a drive root). */
static char *fu_native_path(const char *path) {
    size_t n, i, len = 0;
    char *out;
    int drive = 0;
    if (!path) return fu_dup("");
    n = strlen(path);
    out = (char *)malloc(n + 2);
    if (!out) return NULL;
    i = 0;
    if (n >= 2 && path[1] == ':') {
        out[len++] = path[0];
        out[len++] = ':';
        i = 2;
        drive = 1;
    }
    while (i < n) {
        char c = path[i];
        if (c == '/' || c == '\\') {
            size_t j = i;
            while (j < n && (path[j] == '/' || path[j] == '\\')) j++;
            i = j;
            if (len == 0 || out[len - 1] == '\\' || i >= n) continue;
            out[len++] = '\\';
            continue;
        }
        if (c == '.' && (i == 0 || path[i - 1] == '/' || path[i - 1] == '\\') &&
            (i + 1 >= n || path[i + 1] == '/' || path[i + 1] == '\\')) {
            i++;
            continue;
        }
        out[len++] = c;
        i++;
    }
    if (drive && len == 2 && n > 2) out[len++] = '\\';
    out[len] = '\0';
    return out;
}

static int fu_icase_starts(const char *name, const char *prefix) {
    size_t i;
    for (i = 0; prefix[i]; i++) {
        char a = name[i], b = prefix[i];
        if (!a) return 0;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int fu_icase_cmp(const char *a, const char *b) {
    for (;;) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return (unsigned char)x < (unsigned char)y ? -1 : 1;
        if (!x) return 0;
        a++;
        b++;
    }
}

static int fu_cmp_paths(const void *a, const void *b) {
    return fu_icase_cmp(*(const char *const *)a, *(const char *const *)b);
}

static int fu_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

static int fu_is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFREG;
}

/* The winget install layout: Packages/Gyan.FFmpeg_* / * / bin/ffmpeg.exe. */
static char *fu_winget_candidate(const char *local_appdata) {
    char *base = fu_native_path(local_appdata);
    char packages[FU_MAX_PATH];
    char *best = NULL;
    char **found = NULL;
    size_t count = 0, cap = 0, k;
    DIR *dp;
    struct dirent *pkg;
    int n;
    if (!base) return NULL;
    n = snprintf(packages, sizeof(packages), "%s\\Microsoft\\WinGet\\Packages", base);
    free(base);
    if (n <= 0 || n >= (int)sizeof(packages)) return NULL;
    dp = opendir(packages);
    if (!dp) return NULL;
    while ((pkg = readdir(dp)) != NULL) {
        char pkgdir[FU_MAX_PATH];
        DIR *vp;
        struct dirent *ver;
        if (!strcmp(pkg->d_name, ".") || !strcmp(pkg->d_name, "..")) continue;
        if (!fu_icase_starts(pkg->d_name, "Gyan.FFmpeg_")) continue;
        n = snprintf(pkgdir, sizeof(pkgdir), "%s\\%s", packages, pkg->d_name);
        if (n <= 0 || n >= (int)sizeof(pkgdir)) continue;
        if (!fu_is_dir(pkgdir)) continue;
        vp = opendir(pkgdir);
        if (!vp) continue;
        while ((ver = readdir(vp)) != NULL) {
            char exe[FU_MAX_PATH];
            char *copy;
            if (!strcmp(ver->d_name, ".") || !strcmp(ver->d_name, "..")) continue;
            n = snprintf(exe, sizeof(exe), "%s\\%s\\bin\\ffmpeg.exe", pkgdir, ver->d_name);
            if (n <= 0 || n >= (int)sizeof(exe)) continue;
            if (!fu_is_file(exe)) continue;
            if (count == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **grown = (char **)realloc(found, ncap * sizeof(char *));
                if (!grown) break;
                found = grown;
                cap = ncap;
            }
            copy = fu_dup(exe);
            if (!copy) break;
            found[count++] = copy;
        }
        closedir(vp);
    }
    closedir(dp);
    if (count) {
        qsort(found, count, sizeof(char *), fu_cmp_paths);
        best = found[count - 1];
        found[count - 1] = NULL;
    }
    for (k = 0; k < count; k++) free(found[k]);
    free(found);
    return best;
}

char *w22_ffmpegutil_86b841_resolve(const char *ffmpeg_path_env,
                                    const char *shared_found,
                                    const char *local_appdata) {
    char *explicit_path = fu_trim(ffmpeg_path_env);
    char *expanded;
    char *candidate;
    if (!explicit_path) return NULL;
    if (explicit_path[0]) {
        expanded = w22_ffmpegutil_86b841_expand_path(explicit_path);
        free(explicit_path);
        return expanded;
    }
    free(explicit_path);
    if (shared_found && shared_found[0]) return fu_dup(shared_found);
    if (local_appdata && local_appdata[0]) {
        candidate = fu_winget_candidate(local_appdata);
        if (candidate) return candidate;
    }
    return fu_dup("ffmpeg");
}
