/*
 * web_asset_mime_types.c -- C11 port of the pin table and suffix decisions of
 * upstream Hermes hermes_cli/web_asset_mime_types.py (issue #28987 shape: a
 * Windows registry map can serve .js as text/plain and blank the SPA).
 *
 * PoP: w22_webassetmi_0a1f41_pairs @ hermes_cli/web_asset_mime_types.py:WEB_ASSET_MIME_TYPES
 * PoP: w22_webassetmi_0a1f41_mime_for @ hermes_cli/web_asset_mime_types.py:WEB_ASSET_MIME_TYPES
 * PoP: w22_webassetmi_0a1f41_pinned_map @ hermes_cli/web_asset_mime_types.py:normalize_web_asset_mime_types
 * PoP: w22_webassetmi_0a1f41_guess @ hermes_cli/web_asset_mime_types.py:normalize_web_asset_mime_types
 *
 * `mimetypes.init()` / `mimetypes.add_type(...)` mutate the process-global
 * Python map; that side effect stays upstream. Pure C11, ASCII, no VLA.
 */
#include "hermes_json.h"
#include "noc_w22_webassetmi_0a1f41.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *ext;
    const char *mime;
} wam0a1f41_row;

/* Insertion order of the Python dict literal, verbatim. */
static const wam0a1f41_row WAM0A1F41_TABLE[] = {
    {".js",          "text/javascript"},
    {".mjs",         "text/javascript"},
    {".cjs",         "text/javascript"},
    {".css",         "text/css"},
    {".json",        "application/json"},
    {".map",         "application/json"},
    {".svg",         "image/svg+xml"},
    {".wasm",        "application/wasm"},
    {".webp",        "image/webp"},
    {".webmanifest", "application/manifest+json"},
    {".woff",        "font/woff"},
    {".woff2",       "font/woff2"},
    {".ico",         "image/x-icon"},
};

#define WAM0A1F41_ROWS (sizeof(WAM0A1F41_TABLE) / sizeof(WAM0A1F41_TABLE[0]))

json_t *w22_webassetmi_0a1f41_pairs(void) {
    json_t *out = json_array();
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < WAM0A1F41_ROWS; i++) {
        json_t *pair = json_array();
        if (!pair) continue;
        json_append(pair, json_string(WAM0A1F41_TABLE[i].ext));
        json_append(pair, json_string(WAM0A1F41_TABLE[i].mime));
        json_append(out, pair);
    }
    return out;
}

const char *w22_webassetmi_0a1f41_mime_for(const char *ext) {
    size_t i;
    if (!ext) return NULL;
    for (i = 0; i < WAM0A1F41_ROWS; i++) {
        if (strcmp(WAM0A1F41_TABLE[i].ext, ext) == 0) return WAM0A1F41_TABLE[i].mime;
    }
    return NULL;
}

json_t *w22_webassetmi_0a1f41_pinned_map(void) {
    json_t *out = json_object();
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < WAM0A1F41_ROWS; i++) {
        json_set(out, WAM0A1F41_TABLE[i].ext, json_string(WAM0A1F41_TABLE[i].mime));
    }
    return out;
}

/* The last path segment of `url` (mimetypes looks at the basename only). */
static const char *wam0a1f41_basename(const char *url) {
    const char *base = url;
    const char *p;
    for (p = url; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

const char *w22_webassetmi_0a1f41_guess(const char *url) {
    const char *base, *dot;
    char ext[32];
    size_t i, n;
    if (!url) return NULL;
    /* no query/fragment splitting upstream either: os.path.splitext sees them */
    base = wam0a1f41_basename(url);
    dot = strrchr(base, '.');
    if (!dot || dot == base) return NULL;   /* no suffix, or a dotfile */
    n = strlen(dot);
    if (n >= sizeof(ext)) return NULL;
    for (i = 0; i < n; i++) {
        char c = dot[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        ext[i] = c;
    }
    ext[n] = '\0';
    return w22_webassetmi_0a1f41_mime_for(ext);
}
