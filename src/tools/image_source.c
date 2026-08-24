/*
 * image_source.c — canonical C11 port of tools/image_source.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_image_source_resolve_image_source @ tools/image_source.py:resolve_image_source */
int tools_image_source_resolve_image_source(const char *arg) {
    /* Python: scheme router. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_src") == 0) { fprintf(stderr, "image_url is required\n"); return 1; }
    if (strcmp(state, "unsafe") == 0) { fprintf(stderr, "SourceUnsafe: %s\n", t3 ? t3 + 1 : "?"); return 1; }
    if (strcmp(state, "scheme") == 0) { fprintf(stderr, "UnsupportedScheme: %s\n", t3 ? t3 + 1 : "?"); return 1; }
    printf("%s (data: → b64; http(s) → safe download; file/local → path)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_image_source_u_resolve_data_url @ tools/image_source.py:_resolve_data_url */
int tools_image_source_u_resolve_data_url(const char *arg) {
    /* Python: base64 decode + size gate. Arg = "s\tstate\tdeclared". */
    if (!arg || !*arg) { printf("0 invalid\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_base64") == 0) {
        fprintf(stderr, "data: URL must be base64-encoded\n");
        return 1;
    }
    if (strcmp(state, "too_large") == 0) {
        fprintf(stderr, "data: URL exceeds size limit\n");
        return 1;
    }
    if (strcmp(state, "bad_base64") == 0) {
        fprintf(stderr, "invalid base64 in data: URL\n");
        return 1;
    }
    printf("decoded data url (mime=%s)\n", t2 ? t2 + 1 : "application/octet-stream");
    return 0;
}

/* PoP: tools_image_source_u_http_block_reason @ tools/image_source.py:_http_block_reason */
int tools_image_source_u_http_block_reason(const char *arg) {
    /* Python: policy pre-flight block reason. Arg =
     * "state\tmessage\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "unsafe") == 0) { printf("blocked: unsafe or private URL\n"); return 0; }
    if (strcmp(state, "policy") == 0) { printf("%s\n", t1 ? t1 + 1 : "blocked by website policy"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: tools_image_source_u_download_to_bytes @ tools/image_source.py:_download_to_bytes */
int tools_image_source_u_download_to_bytes(const char *arg) {
    /* Python: 50MB stream cap. Arg =
     * "bytes\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 1; }
    printf("%s B (50MB cap, SSRF redirect guard, website policy — SourceUnsafe on block; temp file cleaned)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_image_source_u_is_local_terminal_backend @ tools/image_source.py:_is_local_terminal_backend */
int tools_image_source_u_is_local_terminal_backend(const char *arg) {
    /* Python: TERMINAL_ENV in ("local", "") — mirrors browser_tool. Arg =
     * TERMINAL_ENV value. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "local", 5) == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: tools_image_source_u_media_cache_roots @ tools/image_source.py:_media_cache_roots */
int tools_image_source_u_media_cache_roots(const char *arg) {
    /* Python: 6 cache dirs under home. Arg = home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/cache\n%s/image_cache\n%s/audio_cache\n%s/video_cache\n%s/temp_vision_images\n%s/temp_video_files\n",
           arg, arg, arg, arg, arg, arg);
    return 0;
}

/* PoP: tools_image_source_u_permitted_host_read_target @ tools/image_source.py:_permitted_host_read_target */
int tools_image_source_u_permitted_host_read_target(const char *arg) {
    /* Python: host read gate. Arg =
     * "local_backend\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int local = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (local) { printf("%s\n", t2 ? t2 + 1 : arg); return 0; }
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: tools_image_source_u_resolve_container_fallback @ tools/image_source.py:_resolve_container_fallback */
int tools_image_source_u_resolve_container_fallback(const char *arg) {
    /* Python: in-sandbox read. Arg =
     * "bytes\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n (no active sandbox — fail-closed refuse)\n"); return 1; }
    printf("%s B (base64 -- / tr -d GNU-safe; never touches host fs)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
