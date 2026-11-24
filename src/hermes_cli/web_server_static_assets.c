/*
 * web_server_static_assets.c -- real C11 port of the static-asset path test and
 * the brand-font lookup of upstream Hermes
 *   hermes_cli/web_server_static_assets.py.
 *
 * PoP: w21_webservers_cb0f6d_is_static_asset_path @ hermes_cli/web_server_static_assets.py:is_static_asset_path
 * PoP: w21_webservers_cb0f6d_public_font_file @ hermes_cli/web_server_static_assets.py:public_font_file
 * PoP: w21_webservers_cb0f6d_vocabulary @ hermes_cli/web_server_static_assets.py:STATIC_ASSET_PREFIXES
 *
 * `_WEB_PUBLIC` is the module's one config-seam value (it is
 * `Path(project_root_str())/"web"/"public"` at import time); the port takes the
 * `web/public` root as a parameter instead. Everything else is real filesystem
 * work: a hit must be a regular file whose resolved path stays under the
 * resolved `<root>/<folder>`, exactly like `is_file()` plus `is_relative_to()`.
 */
#include "hermes_json.h"
#include "noc_w21_webservers_cb0f6d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WSA_MAX_PATH 4096

static const char *const WSA_PREFIXES[] = {"assets/", "ds-assets/", "fonts/", "fonts-terminal/", NULL};
static const char *const WSA_FILES[] = {"apple-touch-icon.png", "favicon.ico", NULL};
static const char *const WSA_FONT_DIRS[] = {"fonts", "fonts-terminal", NULL};

int w21_webservers_cb0f6d_is_static_asset_path(const char *full_path) {
    int i;
    size_t n;
    if (!full_path) return 0;
    for (i = 0; WSA_FILES[i]; i++) {
        if (!strcmp(full_path, WSA_FILES[i])) return 1;
    }
    for (i = 0; WSA_PREFIXES[i]; i++) {
        n = strlen(WSA_PREFIXES[i]);
        if (!strncmp(full_path, WSA_PREFIXES[i], n)) return 1;
    }
    return 0;
}

json_t *w21_webservers_cb0f6d_vocabulary(void) {
    /* `frozenset` has no order; the oracle sorts it, so sort here too. */
    static const char *const FILES_SORTED[] = {"apple-touch-icon.png", "favicon.ico", NULL};
    json_t *out = json_object();
    json_t *prefixes = json_array();
    json_t *files = json_array();
    json_t *dirs = json_array();
    int i;
    for (i = 0; WSA_PREFIXES[i]; i++) json_append(prefixes, json_string(WSA_PREFIXES[i]));
    for (i = 0; FILES_SORTED[i]; i++) json_append(files, json_string(FILES_SORTED[i]));
    for (i = 0; WSA_FONT_DIRS[i]; i++) json_append(dirs, json_string(WSA_FONT_DIRS[i]));
    json_set(out, "prefixes", prefixes);
    json_set(out, "files", files);
    json_set(out, "public_font_dirs", dirs);
    return out;
}

/* Forward-slash copy of a filesystem path (both separators accepted). */
static void wsa_slashes(char *s) {
    for (; s && *s; s++) {
        if (*s == '\\') *s = '/';
    }
}

static int wsa_is_regular_file(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFREG ? 1 : 0;
}

/* Resolved absolute form, '/' separated; falls back to the input text. */
static void wsa_resolve(const char *path, char *out, size_t outsz) {
    if (!path) { out[0] = '\0'; return; }
    _fullpath(out, path, outsz);
    wsa_slashes(out);
}

/* `candidate.resolve().is_relative_to(root.resolve())` */
static int wsa_relative_to(const char *candidate, const char *root) {
    char ca[WSA_MAX_PATH], ro[WSA_MAX_PATH];
    size_t n;
    wsa_resolve(candidate, ca, sizeof(ca));
    wsa_resolve(root, ro, sizeof(ro));
    n = strlen(ro);
    if (!n || strncmp(ca, ro, n) != 0) return 0;
    return (ca[n] == '\0' || ca[n] == '/') ? 1 : 0;
}

char *w21_webservers_cb0f6d_public_font_file(const char *dist_root, const char *dashboard_root,
                                            const char *web_public_root, const char *full_path) {
    const char *roots[3];
    const char *slash, *name;
    char folder[256];
    size_t flen;
    int i;

    if (!full_path) return NULL;
    slash = strchr(full_path, '/');
    flen = slash ? (size_t)(slash - full_path) : strlen(full_path);
    name = slash ? slash + 1 : "";
    if (flen >= sizeof(folder)) return NULL;
    memcpy(folder, full_path, flen);
    folder[flen] = '\0';
    {
        int known = 0;
        for (i = 0; WSA_FONT_DIRS[i]; i++) {
            if (!strcmp(folder, WSA_FONT_DIRS[i])) { known = 1; break; }
        }
        if (!known || !name[0]) return NULL;
    }

    roots[0] = dist_root;
    roots[1] = dashboard_root;
    roots[2] = web_public_root;
    for (i = 0; i < 3; i++) {
        char rootdir[WSA_MAX_PATH];
        char candidate[WSA_MAX_PATH];
        char resolved[WSA_MAX_PATH];
        if (!roots[i]) continue;
        snprintf(rootdir, sizeof(rootdir), "%s/%s", roots[i], folder);
        snprintf(candidate, sizeof(candidate), "%s/%s", rootdir, name);
        if (!wsa_is_regular_file(candidate)) continue;
        if (!wsa_relative_to(candidate, rootdir)) continue;
        wsa_resolve(candidate, resolved, sizeof(resolved));
        {
            size_t n = strlen(resolved);
            char *out = (char *)malloc(n + 1);
            if (!out) return NULL;
            memcpy(out, resolved, n + 1);
            return out;
        }
    }
    return NULL;
}
