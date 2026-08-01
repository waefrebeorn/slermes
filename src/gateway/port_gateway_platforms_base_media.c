/*
 * port_gateway_platforms_base_media.c — Port of Python gateway/platforms/base.py
 *
 * Real C implementations for module-level gateway platform base helpers:
 * cache-dir resolution, media-delivery path validation primitives,
 * MEDIA: tag normalization, and send-error classification helpers.
 *
 * Faithful to the Python bodies in gateway/platforms/base.py. No stubs.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#include "hermes_file_safety.h"
#include "hermes_util_str.h"

/* Declared in src/gateway/platforms/base.h / defined in base_ext2.c. */
char *validate_media_delivery_path(const char *path);

/* Resolve HERMES_ROOT: $HERMES_ROOT, else same as HERMES_HOME. */
static void hermes_root_dir(char *out, size_t sz)
{
    const char *env = getenv("HERMES_ROOT");
    if (env && *env) {
        snprintf(out, sz, "%s", env);
        return;
    }
    hermes_home_dir(out, sz);
}

/* Expand a leading ~/ into the user's home directory. Returns a malloc'd
 * string (caller frees) or NULL on alloc failure. */
static char *expand_user(const char *path)
{
    if (!path) return NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        size_t need = strlen(home) + strlen(path + 1) + 1;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "%s%s", home, path + 1);
        return out;
    }
    return strdup(path);
}

/* Returns true if a legacy directory path exists AND has content
 * (mirrors _legacy_path_has_content). */
static bool legacy_path_has_content(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return true; /* a file counts as content */
    DIR *d = opendir(path);
    if (!d) return false;
    struct dirent *e;
    bool has_content = false;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        has_content = true;
        break;
    }
    closedir(d);
    return has_content;
}

/* Resolve a Hermes subdirectory with backward compatibility.
 * Faithful to hermes_constants.get_hermes_dir(new_subpath, old_name). */
static void hermes_dir(const char *new_subpath, const char *old_name,
                       char *out, size_t sz)
{
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));

    char old_path[PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s/%s", home, old_name);
    if (legacy_path_has_content(old_path)) {
        snprintf(out, sz, "%s", old_path);
        return;
    }
    snprintf(out, sz, "%s/%s", home, new_subpath);
}

/* Ensure a directory exists (mkdir -p equivalent, recursive). */
static bool ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    /* Create parent first, then the directory itself. */
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '/') buf[len-1] = '\0';
    for (size_t i = 1; i < strlen(buf); i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (stat(buf, &st) != 0)
                mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
    return mkdir(buf, 0755) == 0 || (stat(buf, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ================================================================
 *  _platform_name
 *  Faithful to: def _platform_name(platform) -> str
 * ================================================================ */
/* PoP: base_platform_name @ gateway/platforms/base.py:_platform_name */
char *base_platform_name(const char *platform)
{
    /* Python: value = getattr(platform, "value", platform);
     *         return str(value or "").lower() */
    const char *value = platform ? platform : "";
    char *out = strdup(value);
    if (!out) return strdup("");
    for (char *p = out; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return out;
}

/* ================================================================
 *  _log_safe_path
 *  Faithful to: def _log_safe_path(path) -> str
 *  Replaces control chars + line separators with '?', truncates at 200.
 * ================================================================ */
/* PoP: base_platform_log_safe_path @ gateway/platforms/base.py:_log_safe_path */
char *base_platform_log_safe_path(const char *path)
{
    if (!path) path = "";
    size_t len = strlen(path);
    size_t cap = len + 1;
    if (cap > 201) cap = 201; /* 200 chars + NUL */
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len && j < 200; i++) {
        unsigned char c = (unsigned char)path[i];
        bool unsafe = (c <= 0x1f) || (c == 0x7f) || (c == 0x85) ||
                      (c == 0xe2 && (unsigned char)path[i+1] == 0x80 &&
                       ((unsigned char)path[i+2] == 0xa8 || (unsigned char)path[i+2] == 0xa9));
        if (unsafe) {
            out[j++] = '?';
            if (c == 0xe2) i += 2; /* skip 3-byte line separator */
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  _resolve_cache_dir
 *  Faithful to: def _resolve_cache_dir(constant_name, new_subpath, old_name)
 *  Note: the import-time monkeypatch comparison is a test seam that has no
 *  C equivalent; we resolve fresh via hermes_dir(), honoring the legacy
 *  cache layout exactly as the non-monkeypatched path does.
 * ================================================================ */
/* PoP: base_platform_resolve_cache_dir @ gateway/platforms/base.py:_resolve_cache_dir */
void base_platform_resolve_cache_dir(const char *new_subpath, const char *old_name,
                                     char *out, size_t sz)
{
    hermes_dir(new_subpath, old_name, out, sz);
}

/* ================================================================
 *  get_image_cache_dir / get_audio_cache_dir / get_video_cache_dir /
 *  get_document_cache_dir
 *  Faithful to: each returns the (created) cache dir as a malloc'd string.
 * ================================================================ */
/* PoP: base_platform_get_image_cache_dir @ gateway/platforms/base.py:get_image_cache_dir */
char *base_platform_get_image_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/images", "image_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_audio_cache_dir @ gateway/platforms/base.py:get_audio_cache_dir */
char *base_platform_get_audio_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/audio", "audio_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_video_cache_dir @ gateway/platforms/base.py:get_video_cache_dir */
char *base_platform_get_video_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/videos", "video_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* PoP: base_platform_get_document_cache_dir @ gateway/platforms/base.py:get_document_cache_dir */
char *base_platform_get_document_cache_dir(void)
{
    char d[PATH_MAX];
    base_platform_resolve_cache_dir("cache/documents", "document_cache", d, sizeof(d));
    ensure_dir(d);
    return strdup(d);
}

/* ================================================================
 *  _media_delivery_denied_paths
 *  Faithful to: returns the absolute denylist (system prefixes,
 *  $HOME credential subpaths, and per-file/dir Hermes credential stores).
 *  Caller frees the returned array and each element.
 * ================================================================ */

/* Number of denied paths returned by base_platform_media_delivery_denied_paths. */
static const char * const DENIED_SYSTEM_PREFIXES[] = {
    "/etc", "/proc", "/sys", "/dev", "/root", "/boot",
    "/var/log", "/var/lib", "/var/run", NULL
};
static const char * const DENIED_HOME_SUBPATHS[] = {
    ".ssh", ".aws", ".gnupg", ".kube", ".docker", ".config",
    ".azure", ".gcloud", "Library/Keychains", NULL
};
static const char * const ROOT_CREDENTIAL_FILES[] = {
    ".env", "auth.json", "auth.lock", "credentials", "config.yaml",
    ".anthropic_oauth.json", "google_token.json", "google_oauth_pending.json",
    "auth/google_oauth.json", "webhook_subscriptions.json",
    "cache/bws_cache.json", NULL
};
static const char * const ROOT_CREDENTIAL_DIRS[] = {
    "pairing", "mcp-tokens", NULL
};

/* PoP: base_platform_media_delivery_denied_paths @ gateway/platforms/base.py:_media_delivery_denied_paths */
char **base_platform_media_delivery_denied_paths(int *count_out)
{
    char home[PATH_MAX], hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    char *h = expand_user("~");
    snprintf(home, sizeof(home), "%s", h ? h : "/root");
    free(h);
    hermes_home_dir(hermes_home, sizeof(hermes_home));
    hermes_root_dir(hermes_root, sizeof(hermes_root));

    /* Count */
    int n = 0;
    for (int i = 0; DENIED_SYSTEM_PREFIXES[i]; i++) n++;
    for (int i = 0; DENIED_HOME_SUBPATHS[i]; i++) n++;
    for (int r = 0; r < 2; r++) {
        for (int i = 0; ROOT_CREDENTIAL_FILES[i]; i++) n++;
        for (int i = 0; ROOT_CREDENTIAL_DIRS[i]; i++) n++;
    }

    char **arr = calloc((size_t)n + 1, sizeof(char *));
    if (!arr) { *count_out = 0; return NULL; }
    int k = 0;
    for (int i = 0; DENIED_SYSTEM_PREFIXES[i]; i++)
        arr[k++] = strdup(DENIED_SYSTEM_PREFIXES[i]);
    for (int i = 0; DENIED_HOME_SUBPATHS[i]; i++) {
        size_t need = strlen(home) + 1 + strlen(DENIED_HOME_SUBPATHS[i]) + 1;
        char *p = malloc(need);
        snprintf(p, need, "%s/%s", home, DENIED_HOME_SUBPATHS[i]);
        arr[k++] = p;
    }
    for (int r = 0; r < 2; r++) {
        const char *root = (r == 0) ? hermes_home : hermes_root;
        for (int i = 0; ROOT_CREDENTIAL_FILES[i]; i++) {
            size_t need = strlen(root) + 1 + strlen(ROOT_CREDENTIAL_FILES[i]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/%s", root, ROOT_CREDENTIAL_FILES[i]);
            arr[k++] = p;
        }
        for (int i = 0; ROOT_CREDENTIAL_DIRS[i]; i++) {
            size_t need = strlen(root) + 1 + strlen(ROOT_CREDENTIAL_DIRS[i]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/%s", root, ROOT_CREDENTIAL_DIRS[i]);
            arr[k++] = p;
        }
    }
    arr[k] = NULL;
    *count_out = k;
    return arr;
}

void base_platform_media_delivery_denied_paths_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _path_is_within
 *  Faithful to: Path.relative_to(root) succeeds -> True.
 *  Returns true if `path` is the same as or nested under `root`.
 * ================================================================ */
/* PoP: base_platform_path_is_within @ gateway/platforms/base.py:_path_is_within */
bool base_platform_path_is_within(const char *path, const char *root)
{
    if (!path || !root) return false;
    size_t plen = strlen(path), rlen = strlen(root);
    if (plen < rlen) return false;
    if (strncmp(path, root, rlen) != 0) return false;
    /* Exact match, or path continues with a separator. */
    if (plen == rlen) return true;
    return path[rlen] == '/';
}

/* ================================================================
 *  _profile_cache_roots
 *  Faithful to: enumerate <HERMES_ROOT>/profiles/<name>/cache/{subdirs}.
 *  Returns malloc'd NULL-terminated array; caller frees via
 *  base_platform_profile_cache_roots_free.
 * ================================================================ */
static const char * const CACHE_SUBDIRS[] = {
    "images", "audio", "videos", "documents", "screenshots", NULL
};

/* PoP: base_platform_profile_cache_roots @ gateway/platforms/base.py:_profile_cache_roots */
char **base_platform_profile_cache_roots(int *count_out)
{
    char root[PATH_MAX];
    hermes_root_dir(root, sizeof(root));
    char profiles_dir[PATH_MAX];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/profiles", root);

    /* Collect profile directory names */
    char **profiles = NULL;
    int np = 0;
    DIR *d = opendir(profiles_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", profiles_dir, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            char **tmp = realloc(profiles, (size_t)(np + 2) * sizeof(char *));
            if (!tmp) break;
            profiles = tmp;
            profiles[np] = strdup(full);
            if (profiles[np]) np++;
        }
        closedir(d);
    }

    int n = np * 5;
    char **roots = calloc((size_t)n + 1, sizeof(char *));
    if (!roots) {
        for (int i = 0; i < np; i++) free(profiles[i]);
        free(profiles);
        *count_out = 0;
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < np; i++) {
        for (int s = 0; CACHE_SUBDIRS[s]; s++) {
            size_t need = strlen(profiles[i]) + 1 + 7 + 1 + strlen(CACHE_SUBDIRS[s]) + 1;
            char *p = malloc(need);
            snprintf(p, need, "%s/cache/%s", profiles[i], CACHE_SUBDIRS[s]);
            roots[k++] = p;
        }
        free(profiles[i]);
    }
    free(profiles);
    roots[k] = NULL;
    *count_out = k;
    return roots;
}

void base_platform_profile_cache_roots_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _media_delivery_allowed_roots
 *  Faithful to: MEDIA_DELIVERY_SAFE_ROOTS + profile roots + env allow dirs.
 *  Caller frees via base_platform_media_delivery_allowed_roots_free.
 * ================================================================ */

/* PoP: base_platform_media_delivery_allowed_roots @ gateway/platforms/base.py:_media_delivery_allowed_roots */
char **base_platform_media_delivery_allowed_roots(int *count_out)
{
    char hermes_home[PATH_MAX], hermes_root[PATH_MAX];
    hermes_home_dir(hermes_home, sizeof(hermes_home));
    hermes_root_dir(hermes_root, sizeof(hermes_root));

    /* Static safe roots (mirror MEDIA_DELIVERY_SAFE_ROOTS). */
    char *static_roots[20];
    int ns = 0;
    char img[PATH_MAX], aud[PATH_MAX], vid[PATH_MAX], doc[PATH_MAX], shot[PATH_MAX];
    base_platform_resolve_cache_dir("cache/images", "image_cache", img, sizeof(img));
    base_platform_resolve_cache_dir("cache/audio", "audio_cache", aud, sizeof(aud));
    base_platform_resolve_cache_dir("cache/videos", "video_cache", vid, sizeof(vid));
    base_platform_resolve_cache_dir("cache/documents", "document_cache", doc, sizeof(doc));
    base_platform_resolve_cache_dir("cache/screenshots", "browser_screenshots", shot, sizeof(shot));
    static_roots[ns++] = strdup(img);
    static_roots[ns++] = strdup(aud);
    static_roots[ns++] = strdup(vid);
    static_roots[ns++] = strdup(doc);
    static_roots[ns++] = strdup(shot);
    const char *legacy[10] = {"image_cache","audio_cache","video_cache",
        "document_cache","browser_screenshots",NULL};
    for (int i = 0; legacy[i]; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", hermes_home, legacy[i]);
        static_roots[ns++] = strdup(p);
    }
    const char *canon[10] = {"images","audio","videos","documents","screenshots",NULL};
    for (int i = 0; canon[i]; i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/cache/%s", hermes_home, canon[i]);
        static_roots[ns++] = strdup(p);
    }

    int prof_count = 0;
    char **prof = base_platform_profile_cache_roots(&prof_count);

    /* Count env-allow entries. */
    int env_count = 0;
    const char *allow = getenv("HERMES_MEDIA_ALLOW_DIRS");
    if (allow && *allow) {
        /* split on ':' then ',' */
        char *buf = strdup(allow);
        char *save1 = NULL;
        for (char *tok = strtok_r(buf, ":", &save1); tok;
             tok = strtok_r(NULL, ":", &save1)) {
            char *save2 = NULL;
            for (char *t2 = strtok_r(tok, ",", &save2); t2;
                 t2 = strtok_r(NULL, ",", &save2)) {
                char *trim = t2;
                while (*trim == ' ' || *trim == '\t') trim++;
                if (*trim) env_count++;
            }
        }
        free(buf);
    }

    int total = ns + prof_count + env_count;
    char **roots = calloc((size_t)total + 1, sizeof(char *));
    if (!roots) {
        for (int i = 0; i < ns; i++) free(static_roots[i]);
        base_platform_profile_cache_roots_free(prof);
        *count_out = 0;
        return NULL;
    }
    int k = 0;
    for (int i = 0; i < ns; i++) roots[k++] = static_roots[i];
    for (int i = 0; i < prof_count; i++) roots[k++] = prof[i];
    base_platform_profile_cache_roots_free(prof); prof = NULL;
    if (allow && *allow) {
        char *buf = strdup(allow);
        char *save1 = NULL;
        for (char *tok = strtok_r(buf, ":", &save1); tok;
             tok = strtok_r(NULL, ":", &save1)) {
            char *save2 = NULL;
            for (char *t2 = strtok_r(tok, ",", &save2); t2;
                 t2 = strtok_r(NULL, ",", &save2)) {
                char *trim = t2;
                while (*trim == ' ' || *trim == '\t') trim++;
                if (!*trim) continue;
                char *exp = expand_user(trim);
                if (!exp) continue;
                /* only absolute allowed roots */
                if (exp[0] == '/') roots[k++] = exp;
                else free(exp);
            }
        }
        free(buf);
    }
    roots[k] = NULL;
    *count_out = k;
    return roots;
}

void base_platform_media_delivery_allowed_roots_free(char **arr)
{
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ================================================================
 *  _path_under_denied_prefix
 *  Faithful to: any denied path that contains `resolved` blocks it,
 *  except the running user's own $HOME tree (its credential subdirs
 *  are blocked by their own more-specific entries).
 * ================================================================ */
/* PoP: base_platform_path_under_denied_prefix @ gateway/platforms/base.py:_path_under_denied_prefix */
bool base_platform_path_under_denied_prefix(const char *resolved)
{
    if (!resolved || !*resolved) return false;
    char *r = realpath(resolved, NULL);
    const char *rpath = r ? r : resolved;

    char *h = expand_user("~");
    char *resolved_home = h ? realpath(h, NULL) : NULL;
    free(h);

    int n = 0;
    char **denied = base_platform_media_delivery_denied_paths(&n);
    bool result = false;
    for (int i = 0; i < n; i++) {
        char *dr = realpath(denied[i], NULL);
        const char *dp = dr ? dr : denied[i];
        if (base_platform_path_is_within(rpath, dp) || strcmp(rpath, dp) == 0) {
            /* Allow the running user's own home tree. */
            if (resolved_home && strcmp(dp, resolved_home) == 0) {
                free(dr);
                continue;
            }
            result = true;
            free(dr);
            break;
        }
        free(dr);
    }
    base_platform_media_delivery_denied_paths_free(denied);
    free(resolved_home);
    free(r);
    return result;
}

/* ================================================================
 *  _file_is_recently_produced
 *  Faithful to: (time.time() - mtime) <= window_seconds; window<=0 -> False.
 * ================================================================ */
/* PoP: base_platform_file_is_recently_produced @ gateway/platforms/base.py:_file_is_recently_produced */
bool base_platform_file_is_recently_produced(const char *resolved, double window_seconds)
{
    if (!resolved || window_seconds <= 0.0) return false;
    struct stat st;
    if (stat(resolved, &st) != 0) return false;
    double age = difftime(time(NULL), st.st_mtime);
    return age <= window_seconds;
}

/* ================================================================
 *  _normalize_media_tag_path
 *  Faithful to: strip matching surrounding quotes/backticks, then
 *  lstrip backticks/quotes, rstrip backticks/quotes/,.	  :)}]
 * ================================================================ */
/* PoP: base_platform_normalize_media_tag_path @ gateway/platforms/base.py:_normalize_media_tag_path */
char *base_platform_normalize_media_tag_path(const char *raw)
{
    if (!raw) return strdup("");
    const char *quotes = "`\"'";
    char *s = strdup(raw);
    if (!s) return strdup("");
    /* strip surrounding whitespace */
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' ||
                       p[len-1] == '\n' || p[len-1] == '\r'))
        p[--len] = '\0';

    size_t plen = strlen(p);
    if (plen >= 2 && p[0] == p[plen-1] && strchr(quotes, p[0])) {
        memmove(p, p+1, plen-1);
        p[plen-1] = '\0';
    }
    /* lstrip quotes */
    while (*p && strchr(quotes, *p)) p++;
    /* rstrip trailing chars */
    len = strlen(p);
    while (len > 0 && strchr("`\"',.;:)}]", p[len-1])) p[--len] = '\0';

    char *out = strdup(p);
    free(s);
    return out ? out : strdup("");
}

/* ================================================================
 *  _path_lacks_deliverable_extension
 *  Faithful to: not Path(path).suffix  OR  suffix not in MEDIA_DELIVERY_EXTS.
 *  Python returns True when the basename has no extension (e.g. Caddyfile,
 *  Makefile) OR its extension is not in the deliverable-extensions set
 *  (.png/.mp4/.pdf/.zip/... — see gateway/platforms/base.py MEDIA_DELIVERY_EXTS).
 *  A path like "user@example.com" has suffix ".com" which is NOT a deliverable
 *  extension, so it lacks a deliverable extension -> True.
 * ================================================================ */
/* PoP: base_platform_path_lacks_deliverable_extension @ gateway/platforms/base.py:_path_lacks_deliverable_extension */
bool base_platform_path_lacks_deliverable_extension(const char *path)
{
    static const char *const DELIV_EXTS[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".tiff", ".svg",
        ".mp4", ".mov", ".avi", ".mkv", ".webm",
        ".mp3", ".wav", ".ogg", ".opus", ".m4a", ".flac",
        ".pdf", ".docx", ".doc", ".odt", ".rtf", ".txt", ".md", ".epub",
        ".xlsx", ".xls", ".ods", ".csv", ".tsv", ".json", ".xml", ".yaml", ".yml",
        ".pptx", ".ppt", ".odp", ".key",
        ".zip", ".tar", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar", ".apk", ".ipa",
        ".html", ".htm",
        NULL
    };
    if (!path || !*path) return true;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot || !dot[1]) return true;            /* no extension at all */
    /* case-insensitive suffix match against deliverable set */
    size_t slen = strlen(dot);
    for (int i = 0; DELIV_EXTS[i]; i++) {
        size_t elen = strlen(DELIV_EXTS[i]);
        if (elen != slen) continue;
        if (strncasecmp(dot, DELIV_EXTS[i], elen) == 0) return false; /* has deliverable ext */
    }
    return true;                                 /* extension not in deliverable set */
}

/* ================================================================
 *  _strip_media_tag_directives
 *  Faithful to: remove [[audio_as_voice]] / [[as_document]] markers,
 *  remove MEDIA: tags with extensions (via regex-equivalent scan),
 *  and for extension-less path tags, drop them only when the path
 *  validates as a deliverable media file.
 * ================================================================ */

/* Count occurrences of needle in haystack. */
static int count_sub(const char *hay, const char *needle)
{
    int c = 0;
    size_t nl = strlen(needle);
    if (nl == 0) return 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += nl) c++;
    return c;
}

/* PoP: base_platform_strip_media_tag_directives @ gateway/platforms/base.py:_strip_media_tag_directives */
char *base_platform_strip_media_tag_directives(const char *text)
{
    if (!text) return strdup("");
    /* Fast path: nothing to strip. */
    if (!strstr(text, "MEDIA:") &&
        !strstr(text, "[[audio_as_voice]]") &&
        !strstr(text, "[[as_document]]")) {
        return strdup(text);
    }

    /* Remove the [[...]] markers. */
    int extra = count_sub(text, "[[audio_as_voice]]") * strlen("[[audio_as_voice]]")
              + count_sub(text, "[[as_document]]") * strlen("[[as_document]]");
    size_t out_sz = strlen(text) + 1;
    char *cleaned = malloc(out_sz > (size_t)extra + 1 ? out_sz : (size_t)extra + 1);
    if (!cleaned) return strdup(text);
    /* Build cleaned by removing markers (simple linear copy). */
    {
        const char *p = text;
        char *o = cleaned;
        while (*p) {
            if (strncmp(p, "[[audio_as_voice]]", 17) == 0) { p += 17; continue; }
            if (strncmp(p, "[[as_document]]", 14) == 0) { p += 14; continue; }
            *o++ = *p++;
        }
        *o = '\0';
    }

    /* Strip MEDIA: tags with a recognized extension.
     * Python MEDIA_TAG_CLEANUP_RE matches: optional quote, "MEDIA:", the path
     * (backtick/double/single-quoted or bare absolute/~/drive path) ending in
     * a known media extension, followed by a boundary char. We replicate by
     * scanning for "MEDIA:" then the path up to a boundary, taking the inner
     * path (strip surrounding quotes), and dropping the whole tag if it ends
     * in one of the known media extensions. */
    static const char *MEDIA_EXTS[] = {
        ".png",".jpg",".jpeg",".gif",".webp",".bmp",".tiff",".svg",
        ".mp3",".ogg",".opus",".wav",".m4a",".flac",".aac",".mp4",
        ".mov",".webm",".mkv",".avi",".pdf",".md",".txt",".csv",".log",
        ".json",".xml",".yaml",".yml",".toml",".ini",".cfg",".zip",
        ".doc",".docx",".xls",".xlsx",".ppt",".pptx",".ts",".py",".sh",
        NULL
    };
    char *result = strdup(cleaned);
    free(cleaned);
    if (!result) return strdup(text);

    char *out = malloc(strlen(result) + 1);
    if (!out) { free(result); return strdup(text); }
    out[0] = '\0';
    char *w = out;

    const char *s = result;
    const char *media_kw;
    while ((media_kw = strstr(s, "MEDIA:")) != NULL) {
        /* copy everything before this tag */
        size_t pre = (size_t)(media_kw - s);
        memcpy(w, s, pre);
        w += pre;
        /* skip "MEDIA:" */
        const char *p = media_kw + strlen("MEDIA:");
        while (*p == ' ' || *p == '\t') p++;
        /* optional surrounding quote */
        char q = 0;
        if (*p == '`' || *p == '"' || *p == '\'') { q = *p; p++; }
        /* path runs until boundary: whitespace, quote, , ; : ) ] } or end */
        const char *path_start = p;
        while (*p && !strchr(" \t\n\r`\"',;:)}]", *p)) p++;
        size_t plen = (size_t)(p - path_start);
        /* consume optional closing quote */
        if (q && *p == q) p++;
        /* extract inner path */
        char *pathbuf = malloc(plen + 1);
        if (!pathbuf) { /* give up, keep rest */
            strcpy(w, media_kw);
            w += strlen(w);
            break;
        }
        memcpy(pathbuf, path_start, plen);
        pathbuf[plen] = '\0';
        /* strip surrounding quotes from inner */
        size_t iplen = strlen(pathbuf);
        if (iplen >= 2 && pathbuf[0] == pathbuf[iplen-1] &&
            strchr("`\"'", pathbuf[0])) {
            memmove(pathbuf, pathbuf+1, iplen-1);
            pathbuf[iplen-1] = '\0';
        }
        bool drop = false;
        size_t l = strlen(pathbuf);
        for (int e = 0; MEDIA_EXTS[e]; e++) {
            size_t el = strlen(MEDIA_EXTS[e]);
            if (l >= el && strcasecmp(pathbuf + l - el, MEDIA_EXTS[e]) == 0) {
                drop = true;
                break;
            }
        }
        free(pathbuf);
        if (!drop) {
            /* not a recognized-extension tag; keep the original text
             * (handled by extension-less pass / left visible). */
            strcpy(w, media_kw);
            w += strlen(w);
            s = p;
            continue;
        }
        /* dropped: advance past it */
        s = p;
    }
    strcpy(w, s);

    /* Extension-less path tags: drop only when path validates. */
    char *final = strdup(out);
    free(out);
    free(result);
    if (!final) return strdup(text);

    /* Scan extension-less MEDIA: tags and validate. */
    char *fout = malloc(strlen(final) + 1);
    if (!fout) { free(final); return strdup(text); }
    fout[0] = '\0';
    char *fw = fout;
    const char *fs = final;
    const char *mk;
    while ((mk = strstr(fs, "MEDIA:")) != NULL) {
        size_t pre = (size_t)(mk - fs);
        memcpy(fw, fs, pre);
        fw += pre;
        const char *p = mk + strlen("MEDIA:");
        while (*p == ' ' || *p == '\t') p++;
        char q = 0;
        if (*p == '`' || *p == '"' || *p == '\'') { q = *p; p++; }
        const char *path_start = p;
        while (*p && !strchr(" \t\n\r`\"',;:)}]", *p)) p++;
        size_t plen = (size_t)(p - path_start);
        if (q && *p == q) p++;
        char *pathbuf = malloc(plen + 1);
        if (!pathbuf) { strcpy(fw, mk); fw += strlen(fw); break; }
        memcpy(pathbuf, path_start, plen);
        pathbuf[plen] = '\0';
        size_t iplen = strlen(pathbuf);
        if (iplen >= 2 && pathbuf[0] == pathbuf[iplen-1] &&
            strchr("`\"'", pathbuf[0])) {
            memmove(pathbuf, pathbuf+1, iplen-1);
            pathbuf[iplen-1] = '\0';
        }
        bool has_ext = (strrchr(pathbuf, '.') != NULL);
        bool drop = false;
        if (!has_ext) {
            /* only drop if it validates as a deliverable path */
            char *norm = base_platform_normalize_media_tag_path(pathbuf);
            if (norm && *norm) {
                char *valid = validate_media_delivery_path(norm);
                if (valid) { drop = true; free(valid); }
            }
            free(norm);
        }
        free(pathbuf);
        if (!drop) {
            strcpy(fw, mk);
            fw += strlen(fw);
            fs = p;
            continue;
        }
        fs = p;
    }
    strcpy(fw, fs);
    free(final);

    char *ret = strdup(fout);
    free(fout);
    return ret ? ret : strdup(text);
}

/* ================================================================
 *  _error_blob
 *  Faithful to: build lowercased text blob for send-error classifiers.
 * ================================================================ */
/* PoP: base_platform_error_blob @ gateway/platforms/base.py:_error_blob */
char *base_platform_error_blob(const char *exc_str, const char *exc_class,
                               const char *error_text)
{
    /* parts: error_text, exc_str, exc_class — joined by space, lowercased. */
    size_t cap = 1;
    if (error_text) cap += strlen(error_text) + 1;
    if (exc_str)    cap += strlen(exc_str) + 1;
    if (exc_class)  cap += strlen(exc_class) + 1;
    char *blob = malloc(cap);
    if (!blob) return strdup("");
    blob[0] = '\0';
    bool first = true;
    if (error_text && *error_text) {
        strcat(blob, error_text);
        first = false;
    }
    if (exc_str && *exc_str) {
        if (!first) strcat(blob, " ");
        strcat(blob, exc_str);
        first = false;
    }
    if (exc_class && *exc_class) {
        if (!first) strcat(blob, " ");
        strcat(blob, exc_class);
    }
    for (char *p = blob; *p; p++) *p = (char)tolower((unsigned char)*p);
    return blob;
}

/* ================================================================
 *  is_chat_level_not_found
 *  Faithful to: returns True only for chat-level not_found, never
 *  when a sub-chat marker is present.
 * ================================================================ */
/* PoP: base_platform_is_chat_level_not_found @ gateway/platforms/base.py:is_chat_level_not_found */
bool base_platform_is_chat_level_not_found(const char *exc_str,
                                           const char *exc_class,
                                           const char *error_text)
{
    static const char *subchat[] = {
        "message to edit not found", "message to reply not found",
        "thread not found", "topic_deleted", "message_id_invalid", NULL
    };
    static const char *chat_level[] = { "chat not found", NULL };

    char *blob = base_platform_error_blob(exc_str, exc_class, error_text);
    if (!blob) return false;
    bool result = false;
    for (int i = 0; subchat[i]; i++) {
        if (strstr(blob, subchat[i])) { result = false; free(blob); return result; }
    }
    for (int i = 0; chat_level[i]; i++) {
        if (strstr(blob, chat_level[i])) { result = true; break; }
    }
    free(blob);
    return result;
}
