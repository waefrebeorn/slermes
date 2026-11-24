/*
 * adapter_image_batch.c -- C11 port of plugins/platforms/slack/adapter_image_batch.py.
 *
 * PoP: w20_adapterima_cff594_collect @ plugins/platforms/slack/adapter_image_batch.py:collect_image_uploads
 * PoP: w20_adapterima_cff594_ext_for @ plugins/platforms/slack/adapter_image_batch.py:_IMAGE_CT_EXTS
 * PoP: w20_adapterima_cff594_unquote @ plugins/platforms/slack/adapter_image_batch.py:collect_image_uploads
 * PoP: w20_adapterima_cff594_basename @ plugins/platforms/slack/adapter_image_batch.py:collect_image_uploads
 *
 * The SSRF-guarded HTTP client and the bounded body reader stay upstream; the
 * batch shaping -- which pair becomes an upload, which becomes a refused pair,
 * which alt text survives, and what each upload is named -- is pure and ported.
 */
#include "hermes_json.h"
#include "noc_w20_adapterima_cff594.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ai_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ai_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int ai_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* urllib.parse.unquote: %XX -> byte, invalid escapes kept verbatim, UTF-8 passes
 * through as-is (the oracle compares decoded strings, not code points). */
char *w20_adapterima_cff594_unquote(const char *s) {
    size_t i, o = 0, n;
    char *out;
    if (!s) return ai_dup("");
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n) {
            int hi = ai_hexval(s[i + 1]), lo = ai_hexval(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[o++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

/* ntpath.basename: everything after the last separator, so a trailing separator
 * yields "" (ntpath.split("/tmp/dir/") == ("/tmp/dir", "")). */
char *w20_adapterima_cff594_basename(const char *path) {
    size_t n, i;
    const char *p = path ? path : "";
    n = strlen(p);
    for (i = n; i > 0; i--) {
        if (p[i - 1] == '/' || p[i - 1] == '\\') break;
    }
    {
        char *copy = (char *)malloc(n - i + 1);
        if (!copy) return NULL;
        memcpy(copy, p + i, n - i);
        copy[n - i] = '\0';
        return copy;
    }
}

const char *w20_adapterima_cff594_ext_for(const char *content_type) {
    static const char *const KEYS[] = {"jpeg", "jpg", "gif", "webp"};
    static const char *const EXTS[] = {"jpg", "jpg", "gif", "webp"};
    int i;
    if (!content_type) return "png";
    for (i = 0; i < 4; i++) {
        if (strstr(content_type, KEYS[i])) return EXTS[i];
    }
    return "png";
}

static int ai_list_has_string(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

json_t *w20_adapterima_cff594_collect(const json_t *req) {
    json_t *chunk = json_obj_get(req, "chunk");
    json_t *exists = json_obj_get(req, "exists");
    json_t *unsafe = json_obj_get(req, "unsafe");
    json_t *downloads = json_obj_get(req, "downloads");
    json_t *uploads = json_array();
    json_t *alts = json_array();
    json_t *refused = json_array();
    json_t *value, *out;
    size_t i;

    for (i = 0; chunk && chunk->type == JSON_ARRAY && i < chunk->c.count; i++) {
        json_t *pair = chunk->c.items[i];
        const char *url, *alt;
        if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
        url = (pair->c.items[0] && pair->c.items[0]->type == JSON_STRING)
                  ? pair->c.items[0]->str_val : "";
        alt = (pair->c.items[1] && pair->c.items[1]->type == JSON_STRING)
                  ? pair->c.items[1]->str_val : "";

        if (!strncmp(url, "file://", 7)) {
            char *decoded, *base;
            if (ai_truthy(pair->c.items[1])) json_append(alts, json_string(alt));
            decoded = w20_adapterima_cff594_unquote(url + 7);
            /* os.path.exists via asyncio.to_thread: a missing file is skipped
             * after its alt text was already claimed. */
            if (!decoded || !ai_list_has_string(exists, decoded)) {
                free(decoded);
                continue;
            }
            base = w20_adapterima_cff594_basename(decoded);
            {
                json_t *row = json_object();
                json_set(row, "file", json_string(decoded));
                json_set(row, "filename", json_string(base ? base : ""));
                json_append(uploads, row);
            }
            free(base);
            free(decoded);
            continue;
        }
        if (ai_list_has_string(unsafe, url)) {
            /* is_safe_url_fn(url) false -> refused */
            json_t *row = json_array();
            json_append(row, json_string(url));
            json_append(row, json_string(alt));
            json_append(refused, row);
            continue;
        }
        {
            json_t *dl = (downloads && downloads->type == JSON_OBJECT)
                             ? json_obj_get(downloads, url) : NULL;
            const char *ct;
            json_t *row;
            char name[64];
            if (!dl || !json_get_bool(dl, "ok", 0)) {
                json_t *ref = json_array();
                json_append(ref, json_string(url));
                json_append(ref, json_string(alt));
                json_append(refused, ref);
                continue;
            }
            ct = json_get_str(dl, "content_type", "");
            if (ai_truthy(pair->c.items[1])) json_append(alts, json_string(alt));
            snprintf(name, sizeof(name), "image_%d.%s", (int)json_len(uploads),
                     w20_adapterima_cff594_ext_for(ct));
            row = json_object();
            {
                json_t *body = json_obj_get(dl, "content");
                json_set(row, "content", body ? json_copy(body) : json_string(""));
            }
            json_set(row, "filename", json_string(name));
            json_append(uploads, row);
        }
    }

    value = json_object();
    json_set(value, "file_uploads", uploads);
    json_set(value, "initial_comment_parts", alts);
    json_set(value, "refused", refused);
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    return out;
}
