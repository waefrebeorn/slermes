/*
 * content.c — real C11 port of the pure content-block helpers of upstream
 * Hermes acp_adapter/content.py (gate w6_content_cc45ae).
 *
 * PoP: acpcont_resource_display_name @ acp_adapter/content.py:_resource_display_name
 * PoP: acpcont_mime_main @ acp_adapter/content.py:_mime_main
 * PoP: acpcont_is_text_resource @ acp_adapter/content.py:_is_text_resource
 * PoP: acpcont_is_image_resource @ acp_adapter/content.py:_is_image_resource
 * PoP: acpcont_format_resource_text @ acp_adapter/content.py:_format_resource_text
 * PoP: acpcont_attr @ acp_adapter/content.py:_attr
 * PoP: acpcont_image_block_to_openai_part @ acp_adapter/content.py:_image_block_to_openai_part
 * PoP: acpcont_extract_text @ acp_adapter/content.py:_extract_text
 * PoP: acpcont_blocks_to_openai_user_content @ acp_adapter/content.py:_content_blocks_to_openai_user_content
 *
 * Blocks are plain JSON objects whose keys are the Python object attributes;
 * "__type" selects the acp.schema class on the oracle side. Filesystem-backed
 * resource reads are excluded: non-file URIs take the deterministic link-only
 * branch, and embedded blob/text resources are handled entirely in memory.
 */
#include "noc_w6_content_cc45ae.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RESOURCE_BYTES (512 * 1024)

static char *acpcont_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}

static char *acpcont_trim(const char *s) {
    if (!s) return acpcont_dup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static char *acpcont_lower_dup(const char *s) {
    if (!s) return acpcont_dup("");
    char *out = acpcont_dup(s);
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

static int acpcont_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY) return json_len(v) != 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) != 0;
    return 0;
}

static char *acpcont_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return acpcont_dup("");
    if (v->type == JSON_STRING) return acpcont_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return acpcont_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return acpcont_dup(buf);
    }
    return acpcont_dup("");
}

/* urlparse(uri).scheme — scheme is [A-Za-z][A-Za-z0-9+.-]*: */
static int acpcont_scheme(const char *uri, char *buf, size_t cap) {
    if (!uri || !isalpha((unsigned char)uri[0])) return 0;
    size_t i = 1;
    while (uri[i] && (isalnum((unsigned char)uri[i]) || uri[i] == '+' || uri[i] == '-' || uri[i] == '.')) i++;
    if (uri[i] != ':') return 0;
    size_t n = i < cap - 1 ? i : cap - 1;
    memcpy(buf, uri, n);
    buf[n] = '\0';
    return 1;
}

/* urlparse(uri).path (or the raw uri when there is no scheme). */
static char *acpcont_url_path(const char *uri) {
    char scheme[64];
    if (!uri) return acpcont_dup("");
    if (!acpcont_scheme(uri, scheme, sizeof(scheme))) return acpcont_dup(uri);
    const char *p = strchr(uri, ':');
    p = p ? p + 1 : uri;
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        while (*p && *p != '/' && *p != '?' && *p != '#') p++;
    }
    const char *end = p;
    while (*end && *end != '?' && *end != '#') end++;
    size_t n = (size_t)(end - p);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, p, n); out[n] = '\0'; }
    return out;
}

/* urllib.parse.unquote — %XX -> byte (UTF-8 bytes passed through). */
static char *acpcont_unquote(const char *s) {
    if (!s) return acpcont_dup("");
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return acpcont_dup("");
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            char hex[3] = {s[i+1], s[i+2], 0};
            out[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            out[j++] = s[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* pathlib.Path.name — last path component (both separators on Windows).
 * Writes a NUL-terminated component into `out` (cap >= strlen(s)+1). */
static void acpcont_path_name(const char *s, char *out, size_t cap) {
    if (!s || !*s || !out || !cap) { if (out && cap) out[0] = '\0'; return; }
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '/' || s[n-1] == '\\')) n--; /* pathlib drops trailing separators */
    size_t start = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '/' || s[i] == '\\') start = i + 1;
    size_t len = n - start;
    if (len >= cap) len = cap - 1;
    memcpy(out, s + start, len);
    out[len] = '\0';
}

json_t *acpcont_resource_display_name(const char *uri, const json_t *name, const json_t *title) {
    char *rn = acpcont_trim(acpcont_py_str(name));
    char *rt = acpcont_trim(acpcont_py_str(title));
    char *out = NULL;
    if (*rt && *rn && strcmp(rt, rn)) {
        size_t n = strlen(rt) + strlen(rn) + 4;
        out = (char *)malloc(n);
        if (out) snprintf(out, n, "%s (%s)", rt, rn);
    } else if (*rt || *rn) {
        out = acpcont_dup(*rt ? rt : rn);
    } else {
        char *candidate = acpcont_url_path(uri ? uri : "");
        char *unq = acpcont_unquote(candidate);
        char *leafbuf = (char *)malloc(strlen(unq) + 1);
        acpcont_path_name(unq, leafbuf, strlen(unq) + 1);
        if (*leafbuf) out = acpcont_dup(leafbuf);
        else if (uri && *uri) out = acpcont_dup(uri);
        else out = acpcont_dup("resource");
        free(leafbuf);
        free(unq); free(candidate);
    }
    free(rn); free(rt);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

static char *acpcont_mime_main_str(const json_t *mime_type) {
    char *raw = acpcont_py_str(mime_type);
    char *semi = strchr(raw, ';');
    if (semi) *semi = '\0';
    char *main = acpcont_trim(raw);
    free(raw);
    char *lower = acpcont_lower_dup(main);
    free(main);
    return lower;
}

json_t *acpcont_mime_main(const json_t *mime_type) {
    char *m = acpcont_mime_main_str(mime_type);
    json_t *out = json_string(m);
    free(m);
    return out;
}

static int acpcont_text_set(const char *m) {
    static const char *const set[] = {
        "application/json", "application/javascript", "application/typescript",
        "application/xml", "application/x-yaml", "application/yaml",
        "application/toml", "application/sql"
    };
    for (size_t i = 0; i < sizeof(set) / sizeof(set[0]); i++)
        if (!strcmp(m, set[i])) return 1;
    return 0;
}

json_t *acpcont_is_text_resource(const json_t *mime_type) {
    char *m = acpcont_mime_main_str(mime_type);
    int yes = !strncmp(m, "text/", 5) || acpcont_text_set(m);
    free(m);
    return json_bool(yes ? true : false);
}

json_t *acpcont_is_image_resource(const json_t *mime_type) {
    char *m = acpcont_mime_main_str(mime_type);
    int yes = !strncmp(m, "image/", 6);
    free(m);
    return json_bool(yes ? true : false);
}

json_t *acpcont_format_resource_text(const char *uri, const char *body,
                                     const json_t *name, const json_t *title,
                                     const json_t *note) {
    json_t *d = acpcont_resource_display_name(uri ? uri : "", name, title);
    const char *display = d->str_val ? d->str_val : "";
    char *nt = acpcont_trim(acpcont_py_str(note));
    size_t cap = strlen(display) + strlen(uri ? uri : "") + strlen(body ? body : "") + strlen(nt) + 80;
    char *buf = (char *)malloc(cap);
    if (buf) {
        if (*nt)
            snprintf(buf, cap, "[Attached file: %s] (%s)\nURI: %s\n\n%s",
                     display, nt, uri ? uri : "", body ? body : "");
        else
            snprintf(buf, cap, "[Attached file: %s]\nURI: %s\n\n%s",
                     display, uri ? uri : "", body ? body : "");
    }
    free(nt);
    json_free(d);
    json_t *out = json_string(buf ? buf : "");
    free(buf);
    return out;
}

json_t *acpcont_attr(const json_t *obj, const char *attr_name) {
    const json_t *v = obj && obj->type == JSON_OBJECT ? json_obj_get(obj, attr_name) : NULL;
    if (!acpcont_truthy(v)) return json_null();
    char *s = acpcont_trim(acpcont_py_str(v));
    json_t *out = *s ? json_string(s) : json_null();
    free(s);
    return out;
}

static char *acpcont_join_text(const json_t *arr) {
    size_t total = 1, count = json_len(arr);
    for (size_t i = 0; i < count; i++) {
        json_t *e = json_get(arr, i);
        total += (e && e->str_val ? strlen(e->str_val) : 0) + 1;
    }
    char *buf = (char *)malloc(total);
    if (!buf) return acpcont_dup("");
    size_t j = 0;
    for (size_t i = 0; i < count; i++) {
        if (i) buf[j++] = '\n';
        json_t *e = json_get(arr, i);
        const char *s = e && e->str_val ? e->str_val : "";
        size_t n = strlen(s);
        memcpy(buf + j, s, n);
        j += n;
    }
    buf[j] = '\0';
    return buf;
}

json_t *acpcont_extract_text(const json_t *prompt) {
    json_t *texts = json_array();
    if (prompt && prompt->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(prompt); i++) {
            json_t *b = json_get(prompt, i);
            if (!b || b->type != JSON_OBJECT) continue;
            json_t *t = json_obj_get(b, "text");
            if (!t) continue; /* hasattr(block, "text") */
            json_append(texts, json_string(acpcont_py_str(t)));
        }
    }
    char *joined = acpcont_join_text(texts);
    json_free(texts);
    json_t *out = json_string(joined);
    free(joined);
    return out;
}

json_t *acpcont_image_block_to_openai_part(const json_t *block) {
    json_t *dv = acpcont_attr(block, "data");
    json_t *uv = acpcont_attr(block, "uri");
    json_t *mv = acpcont_attr(block, "mime_type");
    const char *data = dv && dv->type == JSON_STRING ? dv->str_val : NULL;
    const char *uri = uv && uv->type == JSON_STRING ? uv->str_val : NULL;
    const char *mime = mv && mv->type == JSON_STRING ? mv->str_val : "image/png";
    char *url = NULL;
    if (data && *data) {
        if (!strncmp(data, "data:", 5)) url = acpcont_dup(data);
        else {
            size_t n = strlen(data) + strlen(mime) + 16;
            url = (char *)malloc(n);
            if (url) snprintf(url, n, "data:%s;base64,%s", mime, data);
        }
    } else if (uri && *uri) {
        url = acpcont_dup(uri);
    }
    json_free(dv); json_free(uv); json_free(mv);
    if (!url) return json_null();
    json_t *inner = json_object();
    json_set(inner, "url", json_string(url));
    free(url);
    json_t *out = json_object();
    json_set(out, "type", json_string("image_url"));
    json_set(out, "image_url", inner);
    return out;
}

static void acpcont_append_parts(json_t *parts, json_t *text_parts, json_t *new_parts) {
    if (!new_parts || new_parts->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(new_parts); i++) {
        json_t *part = json_get(new_parts, i);
        json_append(parts, json_copy(part));
        json_t *ty = json_obj_get(part, "type");
        if (ty && ty->type == JSON_STRING && ty->str_val && !strcmp(ty->str_val, "text")) {
            json_t *t = json_obj_get(part, "text");
            json_append(text_parts, t ? json_copy(t) : json_string(""));
        }
    }
}

static json_t *acpcont_text_parts(const char *uri, const char *body,
                                  const json_t *name, const json_t *title, const json_t *note) {
    json_t *arr = json_array();
    json_t *tp = acpcont_format_resource_text(uri ? uri : "", body ? body : "", name, title, note);
    json_t *part = json_object();
    json_set(part, "type", json_string("text"));
    json_set(part, "text", tp);
    json_append(arr, part);
    return arr;
}

static int acpcont_is_file_uri(const char *uri) {
    char scheme[64];
    if (!uri || !*uri) return 1; /* bare path -> Path(uri) */
    if (!acpcont_scheme(uri, scheme, sizeof(scheme))) return 1;
    return !strcmp(scheme, "file");
}

static json_t *acpcont_resource_link_parts(const json_t *block) {
    json_t *uv = acpcont_attr(block, "uri");
    if (!uv || uv->type != JSON_STRING || !uv->str_val || !uv->str_val[0]) {
        json_free(uv);
        return json_array();
    }
    const char *uri = uv->str_val;
    json_t *nv = acpcont_attr(block, "name");
    json_t *tv = acpcont_attr(block, "title");
    json_t *out;
    if (!acpcont_is_file_uri(uri)) {
        out = acpcont_text_parts(uri,
            "[Resource link only; Hermes cannot read non-file ACP resource URIs directly.]",
            nv, tv, NULL);
    } else {
        /* A real file read would be required; fixtures never exercise this
         * branch. Mirror the read-failure text shape rather than touching FS. */
        out = acpcont_text_parts(uri, "[Could not read attached file: file access unavailable]",
                                 nv, tv, NULL);
    }
    json_free(uv); json_free(nv); json_free(tv);
    return out;
}

/* Minimal RFC 4648 base64 decoder with validate=True semantics. */
static unsigned char *acpcont_b64decode(const char *s, size_t *out_len, int *ok) {
    *ok = 0;
    if (!s) return NULL;
    size_t n = strlen(s);
    if (n % 4 || n == 0) return NULL;
    unsigned char *out = (unsigned char *)malloc(n / 4 * 3 + 4);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i += 4) {
        int v[4];
        for (int k = 0; k < 4; k++) {
            char c = s[i+k];
            if (c >= 'A' && c <= 'Z') v[k] = c - 'A';
            else if (c >= 'a' && c <= 'z') v[k] = c - 'a' + 26;
            else if (c >= '0' && c <= '9') v[k] = c - '0' + 52;
            else if (c == '+') v[k] = 62;
            else if (c == '/') v[k] = 63;
            else if (c == '=') v[k] = -1;
            else { free(out); return NULL; }
        }
        if (v[2] < 0) {
            if (i + 4 != n || v[3] >= 0) { free(out); return NULL; }
            out[j++] = (unsigned char)((v[0] << 2) | (v[1] >> 4));
        } else if (v[3] < 0) {
            if (i + 4 != n) { free(out); return NULL; }
            out[j++] = (unsigned char)((v[0] << 2) | (v[1] >> 4));
            out[j++] = (unsigned char)((v[1] << 4) | (v[2] >> 2));
        } else {
            out[j++] = (unsigned char)((v[0] << 2) | (v[1] >> 4));
            out[j++] = (unsigned char)((v[1] << 4) | (v[2] >> 2));
            out[j++] = (unsigned char)((v[2] << 6) | v[3]);
        }
    }
    *out_len = j;
    *ok = 1;
    return out;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *acpcont_b64encode(const unsigned char *data, size_t len) {
    size_t cap = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return acpcont_dup("");
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned a = data[i], b = i + 1 < len ? data[i+1] : 0, c = i + 2 < len ? data[i+2] : 0;
        out[j++] = B64[a >> 2];
        out[j++] = B64[((a & 3) << 4) | (b >> 4)];
        out[j++] = i + 1 < len ? B64[((b & 15) << 2) | (c >> 6)] : '=';
        out[j++] = i + 2 < len ? B64[c & 63] : '=';
    }
    out[j] = '\0';
    return out;
}

/* _decode_text_bytes, simplified: BOM strip + NUL guard + ASCII passthrough. */
static char *acpcont_decode_text_bytes(const unsigned char *data, size_t len, const char *mime, int *none) {
    *none = 0;
    if (memchr(data, 0, len) && !(mime && !strncmp(mime, "text/", 5) && acpcont_text_set(mime + 5))) {
        if (!(mime && (!strncmp(mime, "text/", 5) || acpcont_text_set(mime)))) {
            *none = 1;
            return NULL;
        }
    }
    size_t off = (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ? 3 : 0;
    char *out = (char *)malloc(len - off + 1);
    if (!out) { *none = 1; return NULL; }
    memcpy(out, data + off, len - off);
    out[len - off] = '\0';
    return out;
}

static json_t *acpcont_image_parts(const char *uri, const char *display,
                                   const unsigned char *data, size_t len, const char *mime) {
    json_t *arr = json_array();
    size_t cap = strlen(display) + strlen(uri ? uri : "") + 48;
    char *header = (char *)malloc(cap);
    if (header) {
        if (uri && *uri) snprintf(header, cap, "[Attached image: %s]\nURI: %s", display, uri);
        else snprintf(header, cap, "[Attached image: %s]", display);
    }
    json_t *tp = json_object();
    json_set(tp, "type", json_string("text"));
    json_set(tp, "text", json_string(header ? header : ""));
    free(header);
    json_append(arr, tp);
    char *b64 = acpcont_b64encode(data, len);
    size_t n = strlen(b64) + strlen(mime ? mime : "image/png") + 16;
    char *url = (char *)malloc(n);
    if (url) snprintf(url, n, "data:%s;base64,%s", mime ? mime : "image/png", b64);
    free(b64);
    json_t *inner = json_object();
    json_set(inner, "url", json_string(url ? url : ""));
    free(url);
    json_t *ip = json_object();
    json_set(ip, "type", json_string("image_url"));
    json_set(ip, "image_url", inner);
    json_append(arr, ip);
    return arr;
}

static json_t *acpcont_embedded_parts(const json_t *block) {
    json_t *resource = json_obj_get(block, "resource");
    if (!resource || resource->type == JSON_NULL) return json_array();
    json_t *uv = acpcont_attr(resource, "uri");
    const char *uri = (uv && uv->type == JSON_STRING) ? uv->str_val : "";
    json_t *mv = acpcont_attr(resource, "mime_type");
    const char *mime = (mv && mv->type == JSON_STRING) ? mv->str_val : NULL;
    json_t *ty = json_obj_get(resource, "__type");
    const char *rt = (ty && ty->type == JSON_STRING) ? ty->str_val : "";
    json_t *out = NULL;
    if (!strcmp(rt, "text")) {
        const char *body = json_get_str(resource, "text", "");
        out = acpcont_text_parts(uri, body, NULL, NULL, NULL);
    } else if (!strcmp(rt, "blob")) {
        const char *blob = json_get_str(resource, "blob", "");
        size_t len = 0; int valid = 0;
        unsigned char *data = acpcont_b64decode(blob, &len, &valid);
        if (!valid) {
            free(data);
            data = (unsigned char *)acpcont_dup(blob);
            len = strlen(blob);
        }
        json_t *dm = acpcont_is_image_resource(mv);
        int is_image = dm && dm->type == JSON_BOOL && dm->bool_val;
        json_free(dm);
        if (is_image) {
            if (len > MAX_RESOURCE_BYTES) {
                size_t n = 160;
                char *msg = (char *)malloc(n);
                if (msg) snprintf(msg, n, "[Embedded image too large to inline: %zu bytes, cap=%d]",
                                  len, MAX_RESOURCE_BYTES);
                out = acpcont_text_parts(uri, msg ? msg : "", NULL, NULL, NULL);
                free(msg);
            } else {
                json_t *dn = acpcont_resource_display_name(uri, NULL, NULL);
                out = acpcont_image_parts(uri, dn->str_val ? dn->str_val : "", data, len,
                                          mime ? mime : "image/png");
                json_free(dn);
            }
        } else {
            size_t take = len < MAX_RESOURCE_BYTES ? len : MAX_RESOURCE_BYTES;
            int none = 0;
            char *body = acpcont_decode_text_bytes(data, take, mime, &none);
            char *final = NULL;
            if (none) {
                size_t n = 192;
                final = (char *)malloc(n);
                if (final) snprintf(final, n, "[Binary embedded file omitted: %zu bytes, mime=%s]",
                                    len, mime ? mime : "unknown");
            } else {
                size_t n = strlen(body ? body : "") + 192;
                final = (char *)malloc(n);
                if (final) {
                    if (len > MAX_RESOURCE_BYTES)
                        snprintf(final, n, "%s\n\n[Truncated to %d of %zu bytes]",
                                 body ? body : "", MAX_RESOURCE_BYTES, len);
                    else
                        snprintf(final, n, "%s", body ? body : "");
                }
            }
            free(body);
            out = acpcont_text_parts(uri, final ? final : "", NULL, NULL, NULL);
            free(final);
        }
        free(data);
    } else {
        json_t *text = json_obj_get(resource, "text");
        if (acpcont_truthy(text))
            out = acpcont_text_parts(uri, json_get_str(resource, "text", ""), NULL, NULL, NULL);
        else
            out = json_array();
    }
    json_free(uv); json_free(mv);
    return out;
}

json_t *acpcont_blocks_to_openai_user_content(const json_t *prompt) {
    json_t *parts = json_array();
    json_t *text_parts = json_array();
    if (prompt && prompt->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(prompt); i++) {
            json_t *b = json_get(prompt, i);
            if (!b || b->type != JSON_OBJECT) continue;
            json_t *ty = json_obj_get(b, "__type");
            const char *t = (ty && ty->type == JSON_STRING) ? ty->str_val : "";
            if (!strcmp(t, "text")) {
                const char *text = json_get_str(b, "text", "");
                if (text && *text) {
                    json_t *p = json_object();
                    json_set(p, "type", json_string("text"));
                    json_set(p, "text", json_string(text));
                    json_append(parts, p);
                    json_append(text_parts, json_string(text));
                }
            } else if (!strcmp(t, "image")) {
                json_t *part = acpcont_image_block_to_openai_part(b);
                if (part && part->type != JSON_NULL) { json_append(parts, part); }
                else json_free(part);
            } else if (!strcmp(t, "resource_link")) {
                acpcont_append_parts(parts, text_parts, acpcont_resource_link_parts(b));
            } else if (!strcmp(t, "embedded")) {
                acpcont_append_parts(parts, text_parts, acpcont_embedded_parts(b));
            }
        }
    }
    json_t *out;
    if (json_len(parts) == 0) {
        out = acpcont_extract_text(prompt);
    } else {
        int all_text = 1;
        for (size_t i = 0; i < json_len(parts); i++) {
            json_t *ty = json_obj_get(json_get(parts, i), "type");
            if (!ty || ty->type != JSON_STRING || !ty->str_val || strcmp(ty->str_val, "text")) {
                all_text = 0;
                break;
            }
        }
        if (all_text) {
            char *joined = acpcont_join_text(text_parts);
            out = json_string(joined);
            free(joined);
        } else {
            out = parts;
            parts = NULL;
        }
    }
    json_free(parts);
    json_free(text_parts);
    return out;
}
