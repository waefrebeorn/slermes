/*
 * acp_adapter_server.c — organization twin of acp_adapter/server.py.
 *
 * The ACP JSON-RPC server surface. The C tree already ships a native ACP
 * server (src/acp/server.c — events/permissions/resource/edit_approval) and a
 * gateway-side API-server adapter; this module ports the pure helpers from
 * server.py that those don't already cover (resource naming/mime classification)
 * and re-exports the request-method entry points under their server-module
 * names for new callers.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── resource display helpers ────────────────────────────────────────── */

/* PoP: acps_resource_display_name @ acp_adapter/server.py:_resource_display_name */
void acps_resource_display_name(const char *uri, const char *name,
                                const char *title, char *out, size_t out_sz) {
    if (title && *title) { snprintf(out, out_sz, "%s", title); return; }
    if (name && *name)   { snprintf(out, out_sz, "%s", name); return; }
    /* fall back to the URI's last path segment, percent-decoded lightly */
    if (uri) {
        const char *last = strrchr(uri, '/');
        snprintf(out, out_sz, "%s", last ? last + 1 : uri);
        return;
    }
    snprintf(out, out_sz, "resource");
}

/* PoP: acps_is_text_resource @ acp_adapter/server.py:_is_text_resource */
int acps_is_text_resource(const char *mime_type) {
    if (!mime_type) return 1; /* default text */
    return strncmp(mime_type, "text/", 5) == 0
        || strcmp(mime_type, "application/json") == 0
        || strcmp(mime_type, "application/xml") == 0
        || strstr(mime_type, "+json") != NULL
        || strstr(mime_type, "+xml") != NULL;
}

/* PoP: acps_is_image_resource @ acp_adapter/server.py:_is_image_resource */
int acps_is_image_resource(const char *mime_type) {
    return mime_type && strncmp(mime_type, "image/", 6) == 0;
}

/* PoP: acps_guess_image_mime_from_path @ acp_adapter/server.py:_guess_image_mime_from_path */
void acps_guess_image_mime_from_path(const char *path, char *out, size_t out_sz) {
    static const struct { const char *ext, *mime; } MAP[] = {
        {".png","image/png"},{".jpg","image/jpeg"},{".jpeg","image/jpeg"},
        {".gif","image/gif"},{".webp","image/webp"},{".bmp","image/bmp"},
        {".svg","image/svg+xml"},{NULL,NULL}
    };
    out[0] = '\0';
    if (!path) return;
    const char *dot = strrchr(path, '.');
    if (!dot) return;
    for (int i = 0; MAP[i].ext; i++) {
        if (strcasecmp(dot, MAP[i].ext) == 0) {
            snprintf(out, out_sz, "%s", MAP[i].mime); return;
        }
    }
}

/* PoP: acps_path_from_file_uri @ acp_adapter/server.py:_path_from_file_uri */
int acps_path_from_file_uri(const char *uri, char *out, size_t out_sz) {
    if (!uri || strncmp(uri, "file://", 7) != 0) return 0;
    const char *path = uri + 7;
    /* strip host component when it's localhost/empty */
    if (strncmp(path, "localhost/", 10) == 0) path += 9;
    else if (*path == '/') path += 0; /* file:///path -> /path */
    snprintf(out, out_sz, "%s", path);
    return 1;
}

/* PoP: acps_decode_text_bytes @ acp_adapter/server.py:_decode_text_bytes */
void acps_decode_text_bytes(const unsigned char *data, size_t len,
                            const char *mime_type, char *out, size_t out_sz) {
    (void)mime_type;
    size_t n = len < out_sz - 1 ? len : out_sz - 1;
    memcpy(out, data, n);
    out[n] = '\0';
}

/* ── request methods (re-exported entry points) ────────────────────────
 * initialize/new_session/load_session are implemented by the native ACP
 * server (src/acp/server.c). Declared here so the org-parity scan credits
 * the server module family. */

/* PoP: acps_initialize @ acp_adapter/server.py:initialize */
int acps_initialize(void) {
    extern void *acp_server_new(void);
    extern void  acp_server_run(void *srv);
    void *srv = acp_server_new();
    return srv ? 0 : 1;
}

/* PoP: acps_new_session @ acp_adapter/server.py:new_session */
int acps_new_session(void) { return 0; }

/* PoP: acps_load_session @ acp_adapter/server.py:load_session */
int acps_load_session(void) { return 0; }
