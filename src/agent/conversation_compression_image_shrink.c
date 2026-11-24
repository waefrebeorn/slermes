/*
 * conversation_compression_image_shrink.c - C11 port of upstream
 * agent/conversation_compression_image_shrink.py (gate w26_conversati_97cb0b).
 *
 * PoP: w26_conversati_97cb0b_data_url_mime @ agent/conversation_compression_image_shrink.py:_data_url_mime
 * PoP: w26_conversati_97cb0b_source_to_data_url @ agent/conversation_compression_image_shrink.py:_source_to_data_url
 * PoP: w26_conversati_97cb0b_write_source @ agent/conversation_compression_image_shrink.py:_write_data_url_to_source
 * PoP: w26_conversati_97cb0b_decode_pixels @ agent/conversation_compression_image_shrink.py:_decode_pixels
 * PoP: w26_conversati_97cb0b_shrink @ agent/conversation_compression_image_shrink.py:_shrink_data_url
 * PoP: w26_conversati_97cb0b_constants @ agent/conversation_compression_image_shrink.py:<module constants>
 *
 * Pillow and the resizer are seams: the caller hands in the pixel probe and
 * what the resizer returned, and this file owns the decision -- which URL is
 * even a candidate, which axis triggered the shrink, and whether the result
 * may replace the original without re-sending a payload the provider already
 * rejected.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_conversati_97cb0b.h"

#define IS_TARGET_BYTES W26_CONVERSATI_97CB0B_SHRINK_TARGET_BYTES

/* _IMAGE_SUFFIX_BY_MIME, kept in sorted() order (the dict has no order). */
static const char *const IS_MIMES[] = {
    "image/bmp", "image/gif", "image/jpeg", "image/jpg", "image/png", "image/webp",
};
static const char *const IS_SUFFIXES[] = {
    ".bmp", ".gif", ".jpg", ".jpg", ".png", ".webp",
};
#define IS_MIME_COUNT (sizeof(IS_MIMES) / sizeof(IS_MIMES[0]))

static int is_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static void is_trim(const char **s, size_t *n) {
    const char *p = *s;
    size_t len = *n;
    while (len && (p[0] == ' ' || p[0] == '\t' || p[0] == '\n' || p[0] == '\r' ||
                   p[0] == '\f' || p[0] == '\v')) { p++; len--; }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\n' ||
                   p[len - 1] == '\r' || p[len - 1] == '\f' || p[len - 1] == '\v')) len--;
    *s = p;
    *n = len;
}

/* ------------------------------------------------------------- _data_url_mime */

static char *is_mime_of(const char *header, const char *def) {
    if (strncmp(header, "data:", 5) == 0) {
        const char *cand = header + 5;
        const char *semi = strchr(cand, ';');
        size_t len = semi ? (size_t)(semi - cand) : strlen(cand);
        const char *p = cand;
        is_trim(&p, &len);
        if (len > 6 && strncmp(p, "image/", 6) == 0) {
            char *out = (char *)malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, p, len);
            out[len] = '\0';
            return out;
        }
    }
    {
        const char *d = def ? def : "image/jpeg";
        size_t len = strlen(d);
        char *copy = (char *)malloc(len + 1);
        if (!copy) return NULL;
        memcpy(copy, d, len + 1);
        return copy;
    }
}

json_t *w26_conversati_97cb0b_data_url_mime(const json_t *req) {
    const char *header = json_get_str(req, "header", "");
    const json_t *def_node = json_obj_get(req, "default");
    const char *def = (def_node && def_node->type == JSON_STRING && def_node->str_val)
                          ? def_node->str_val : "image/jpeg";
    char *mime = is_mime_of(header, def);
    json_t *out = json_string(mime ? mime : def);
    free(mime);
    return out;
}

/* --------------------------------------------------------- _source_to_data_url */

json_t *w26_conversati_97cb0b_source_to_data_url(const json_t *req) {
    const json_t *source = json_obj_get(req, "source");
    const json_t *data, *mt;
    const char *media;
    size_t need;
    char *buf;
    json_t *out;

    if (!source || source->type != JSON_OBJECT) return json_null();
    if (strcmp(json_get_str(source, "type", ""), "base64") != 0) return json_null();
    data = json_obj_get(source, "data");
    if (!data || data->type != JSON_STRING || !data->str_val || !data->str_val[0])
        return json_null();
    mt = json_obj_get(source, "media_type");
    media = (mt && mt->type == JSON_STRING && mt->str_val) ? mt->str_val : "image/jpeg";
    if (!media[0]) media = "image/jpeg";
    {
        const char *p = media;
        size_t len = strlen(media);
        is_trim(&p, &len);
        need = strlen("data:;base64,") + len + strlen(data->str_val) + 1;
        buf = (char *)malloc(need);
        if (!buf) return json_null();
        snprintf(buf, need, "data:%s;base64,%s",
                 (len > 6 && strncmp(p, "image/", 6) == 0) ? p : "image/jpeg",
                 data->str_val);
    }
    out = json_string(buf);
    free(buf);
    return out;
}

/* ------------------------------------------------------ _write_data_url_to_source */

json_t *w26_conversati_97cb0b_write_source(const json_t *req) {
    const json_t *source = json_obj_get(req, "source");
    const char *data_url = json_get_str(req, "data_url", "");
    const char *comma = strchr(data_url, ',');
    size_t header_len = comma ? (size_t)(comma - data_url) : strlen(data_url);
    char *header;
    char *mime;
    json_t *out;

    if (!source || source->type != JSON_OBJECT) return json_object();
    header = (char *)malloc(header_len + 1);
    if (!header) return json_object();
    memcpy(header, data_url, header_len);
    header[header_len] = '\0';
    mime = is_mime_of(header, "image/jpeg");
    out = json_copy(source);
    json_set(out, "type", json_string("base64"));
    json_set(out, "media_type", json_string(mime ? mime : "image/jpeg"));
    json_set(out, "data", json_string(comma ? comma + 1 : ""));
    free(header);
    free(mime);
    return out;
}

/* ------------------------------------------------------------- _decode_pixels */

static int is_b64_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/';
}

static int is_b64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* binascii.a2b_base64 acceptance (validate=False): non-alphabet bytes are
 * dropped, then the residue must be a legal length with legal padding. */
static int is_b64_ok(const char *s) {
    size_t datachars = 0, pad = 0, i, n = strlen(s);
    size_t last_data = 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (is_b64_char(c)) { datachars++; last_data = i; }
        else if (c == '=') { pad++; }
    }
    /* padding must be trailing */
    for (i = last_data + 1; i < n; i++) {
        if (is_b64_char((unsigned char)s[i])) return 0;
    }
    if (pad > 2) return 0;
    if (datachars % 4 == 1) return 0;
    if (pad == 0) return datachars % 4 == 0;
    if (pad == 1) return datachars % 4 == 3;
    return datachars % 4 == 2;
}

/* First `want` decoded bytes; returns how many were produced. */
static size_t is_b64_first(const char *s, unsigned char *out, size_t want) {
    size_t produced = 0, have = 0, i;
    int acc = 0;
    for (i = 0; s[i] && produced < want; i++) {
        int v = is_b64_value((unsigned char)s[i]);
        if (v < 0) {
            if (s[i] == '=') break;
            continue;
        }
        acc = (acc << 6) | v;
        have++;
        if (have == 4) {
            out[produced++] = (unsigned char)((acc >> 16) & 0xFF);
            if (produced < want) out[produced++] = (unsigned char)((acc >> 8) & 0xFF);
            if (produced < want) out[produced++] = (unsigned char)(acc & 0xFF);
            have = 0;
            acc = 0;
        }
    }
    if (have == 2 && produced < want) out[produced++] = (unsigned char)((acc >> 4) & 0xFF);
    else if (have == 3) {
        if (produced < want) out[produced++] = (unsigned char)((acc >> 10) & 0xFF);
        if (produced < want) out[produced++] = (unsigned char)((acc >> 2) & 0xFF);
    }
    return produced;
}

/* PNG IHDR width/height, or 0 when the payload is not a decodable PNG. */
static int is_png_dims(const char *url, long *w, long *h) {
    static const unsigned char SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const char *comma;
    const char *data;
    unsigned char head[24];
    size_t got;

    if (strncmp(url, "data:", 5) != 0) return 0;
    comma = strchr(url, ',');
    if (!comma) return 0;
    data = comma + 1;
    if (!*data) return 0;
    got = is_b64_first(data, head, sizeof(head));
    if (got < sizeof(head)) return 0;
    if (memcmp(head, SIG, 8) != 0) return 0;
    if (memcmp(head + 12, "IHDR", 4) != 0) return 0;
    *w = (long)(((unsigned long)head[16] << 24) | ((unsigned long)head[17] << 16) |
                ((unsigned long)head[18] << 8) | (unsigned long)head[19]);
    *h = (long)(((unsigned long)head[20] << 24) | ((unsigned long)head[21] << 16) |
                ((unsigned long)head[22] << 8) | (unsigned long)head[23]);
    return 1;
}

json_t *w26_conversati_97cb0b_decode_pixels(const json_t *req) {
    const char *url = json_get_str(req, "data_url", "");
    long w, h;
    json_t *out;
    if (!is_png_dims(url, &w, &h)) return json_null();
    out = json_array();
    json_append(out, json_number((double)w));
    json_append(out, json_number((double)h));
    return out;
}

/* ------------------------------------------------------------ _shrink_data_url */

static json_t *is_pair(json_t *first, int unshrinkable) {
    json_t *out = json_array();
    json_append(out, first ? first : json_null());
    json_append(out, json_bool(unshrinkable != 0));
    return out;
}

json_t *w26_conversati_97cb0b_shrink(const json_t *req) {
    const json_t *url_node = json_obj_get(req, "url");
    const json_t *resized_node = json_obj_get(req, "resized");
    long max_dimension = (long)json_get_num(req, "max_dimension", 8000.0);
    const char *url;
    const char *triggered = NULL;
    long w = 0, h = 0;
    long rw = 0, rh = 0;
    int have_dims, have_new_dims;
    size_t ulen, rlen;

    if (!url_node || url_node->type != JSON_STRING || !url_node->str_val)
        return is_pair(NULL, 0);
    url = url_node->str_val;
    ulen = strlen(url);

    /* over byte budget? else check the pixel cap (tiny bytes, huge pixels) */
    if (ulen > (size_t)IS_TARGET_BYTES) {
        triggered = "bytes";
    } else {
        have_dims = is_png_dims(url, &w, &h);
        if (!have_dims || (w > h ? w : h) <= max_dimension) return is_pair(NULL, 0);
        triggered = "dimension";
    }
    {
        const char *comma = strchr(url, ',');
        const char *data = comma ? comma + 1 : "";
        /* the re-encode path base64-decodes the payload; a decode error is the seam's failure */
        if (!is_b64_ok(data)) return is_pair(NULL, 1);
    }
    if (!is_truthy(resized_node) || resized_node->type != JSON_STRING)
        return is_pair(NULL, 1);  /* Pillow couldn't help */
    rlen = strlen(resized_node->str_val);
    have_new_dims = is_png_dims(resized_node->str_val, &rw, &rh);
    if (!strcmp(triggered, "bytes")) {
        /* bytes must shrink; an over-cap blob (long side frozen at the floor) still 400s */
        int ok = rlen < ulen &&
                 (!have_new_dims || (rw > rh ? rw : rh) <= max_dimension);
        return is_pair(ok ? json_string(resized_node->str_val) : NULL, ok ? 0 : 1);
    }
    if (have_new_dims) {
        /* dimension cap is binding: a byte-larger re-encode is fine if now within cap */
        int ok = (rw > rh ? rw : rh) <= max_dimension;
        return is_pair(ok ? json_string(resized_node->str_val) : NULL, ok ? 0 : 1);
    }
    {
        int ok = rlen < ulen;  /* cannot verify dimensions: bytes must shrink */
        return is_pair(ok ? json_string(resized_node->str_val) : NULL, ok ? 0 : 1);
    }
}

/* ---------------------------------------------------------------- constants */

json_t *w26_conversati_97cb0b_constants(void) {
    json_t *o = json_object();
    json_t *suffixes = json_object();
    size_t i;
    for (i = 0; i < IS_MIME_COUNT; i++)
        json_set(suffixes, IS_MIMES[i], json_string(IS_SUFFIXES[i]));
    json_set(o, "shrink_target_bytes", json_number((double)IS_TARGET_BYTES));
    json_set(o, "default_max_dimension",
             json_number((double)W26_CONVERSATI_97CB0B_DEFAULT_MAX_DIMENSION));
    json_set(o, "suffix_by_mime", suffixes);
    return o;
}
