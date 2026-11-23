/*
 * web_upload_limit.c — C port of upstream Hermes hermes_cli/web_upload_limit.py.
 *
 * PoP: w15_webupload_e1e756_check @ hermes_cli/web_upload_limit.py:ChatFileUploadLimitMiddleware.__call__
 *
 * The middleware is ASGI/async and framework-bound; this port extracts its pure
 * admission decision. Route caps and the browser-files policy result are
 * injected so no FastAPI/Starlette runtime is required. The content-length test
 * mirrors `length = header.lstrip('0') or '0'; length.isascii() and
 * length.isdecimal() and (len(length) > len(str(body_max)) or int(length) > body_max)`
 * using digit-length comparison to avoid 64-bit overflow.
 */
#include "noc_w15_webupload_e1e756.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct json_t *with_status(int code) {
    struct json_t *out = json_object();
    json_set(out, "status", json_number(code));
    return out;
}

/* Python: length.isascii() and length.isdecimal() and (len > len_max or int > max) */
static int decimal_exceeds(const char *raw, long long body_max) {
    if (!raw) return 0;
    const char *p = raw;
    while (*p == '0') p++;
    if (!*p) p = "0";                       /* `... or "0"` */
    size_t n = strlen(p);
    for (size_t i = 0; i < n; i++)
        if (p[i] < '0' || p[i] > '9') return 0;   /* isascii + isdecimal */
    char maxbuf[32];
    snprintf(maxbuf, sizeof(maxbuf), "%lld", body_max);
    size_t m = strlen(maxbuf);
    if (n > m) return 1;
    if (n < m) return 0;
    return strcmp(p, maxbuf) > 0;            /* equal length: lexicographic == numeric */
}

struct json_t *w15_webupload_e1e756_check(const char *path, const char *root_path,
                                          const char *scope_type, const char *method,
                                          int browser_files, const char *content_length,
                                          long long chunk_total, long long file_max,
                                          long long image_max) {
    const char *effective = path ? path : "";
    const char *root = root_path ? root_path : "";
    if (*root) {
        size_t rn = strlen(root);
        if (strncmp(effective, root, rn) == 0 && effective[rn] == '/')
            effective += rn;                 /* Python: path = path[len(root):] */
    }

    long long body_max = 0;
    if (strcmp(effective, "/api/chat/file-upload") == 0) body_max = file_max;
    else if (strcmp(effective, "/api/chat/image-upload") == 0) body_max = image_max;

    if (!scope_type || strcmp(scope_type, "http") != 0) return with_status(200);
    if (!method || strcmp(method, "POST") != 0) return with_status(200);
    if (body_max == 0) return with_status(200);
    if (!browser_files) return with_status(200);

    if (decimal_exceeds(content_length, body_max)) return with_status(413);
    if (chunk_total > body_max) return with_status(413);
    return with_status(200);
}
