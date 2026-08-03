/*
 * port_image_routing_remaining.c — Port of agent/image_routing.py image
 * input-mode surface. Reference extraction, MIME sniffing from magic
 * bytes, data-url building, native content parts, capability logic.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: extract_image_refs @ agent/image_routing.py:extract_image_refs */
char *imgr_extract_image_refs(const char *text) {
    /* Python: scan for image references (paths/urls). */
    if (!text) return strdup("[]\t");
    size_t cap = strlen(text) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]\t");
    strcpy(out, "[");
    bool first = true;
    const char *p = text;
    while ((p = strstr(p, "MEDIA:")) != NULL || (p = strstr(p, "http")) != NULL) {
        const char *e = p;
        while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != ')' && *e != ',') e++;
        size_t tok = (size_t)(e - p);
        if (tok > 0) {
            size_t need = strlen(out) + tok + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "\"");
            strncat(out, p, tok);
            strcat(out, "\"");
            first = false;
        }
        p = e;
    }
    strcat(out, "]");
    return out;
}

/* PoP: _coerce_capability_bool @ agent/image_routing.py:_coerce_capability_bool */
int imgr_coerce_capability_bool(const char *value) {
    /* Python: True/False for recognised; -1 None. */
    if (!value) return -1;
    char *l = lowerdup(value);
    if (!l) return -1;
    int r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0) r = 1;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0) r = 0;
    else r = -1;
    free(l);
    return r;
}

/* PoP: _supports_vision_override @ agent/image_routing.py:_supports_vision_override */
int imgr_supports_vision_override(const char *config_yaml) {
    /* Python: user-declared vision from config. */
    if (!config_yaml) return -1;
    const char *p = strstr(config_yaml, "supports_vision");
    if (!p) return -1;
    const char *colon = strchr(p, ':');
    if (!colon) return -1;
    return imgr_coerce_capability_bool(colon + 1);
}

/* PoP: _coerce_mode @ agent/image_routing.py:_coerce_mode */
char *imgr_coerce_mode(const char *raw) {
    /* Python: normalize to native/text. */
    if (!raw) return NULL;
    char *l = lowerdup(raw);
    if (!l) return NULL;
    char *r = NULL;
    if (strcmp(l, "native") == 0 || strcmp(l, "auto") == 0) r = strdup("native");
    else if (strcmp(l, "text") == 0) r = strdup("text");
    else r = strdup(l);
    free(l);
    return r;
}

/* PoP: _explicit_aux_vision_override @ agent/image_routing.py:_explicit_aux_vision_override */
bool imgr_explicit_aux_vision_override(const char *config_yaml) {
    /* Python: user configured specific aux vision backend. */
    if (!config_yaml) return false;
    return strstr(config_yaml, "auxiliary_vision") != NULL;
}

/* PoP: _lookup_supports_vision @ agent/image_routing.py:_lookup_supports_vision */
int imgr_lookup_supports_vision(const char *provider, const char *model, const char *config_yaml) {
    /* Python: resolve caps; -1 unknown. */
    int ov = imgr_supports_vision_override(config_yaml);
    if (ov >= 0) return ov;
    if (!provider || !model) return -1;
    char *needle = NULL;
    asprintf(&needle, "%s/%s", provider, model);
    bool hit = needle && (strstr(needle, "gpt-4o") || strstr(needle, "gpt-4.1") ||
                          strstr(needle, "gpt-5") || strstr(needle, "claude") ||
                          strstr(needle, "gemini") || strstr(needle, "vision"));
    free(needle);
    return hit ? 1 : -1;
}

/* PoP: decide_image_input_mode @ agent/image_routing.py:decide_image_input_mode */
char *imgr_decide_image_input_mode(const char *provider, const char *model, const char *config_yaml) {
    /* Python: "native" or "text". */
    int v = imgr_lookup_supports_vision(provider, model, config_yaml);
    if (v == 1) return strdup("native");
    if (v == 0) return strdup("text");
    return strdup("text");
}

/* PoP: _sniff_mime_from_bytes @ agent/image_routing.py:_sniff_mime_from_bytes */
char *imgr_sniff_mime_from_bytes(const unsigned char *buf, size_t len) {
    /* Python: magic-byte MIME detect. */
    if (!buf || len < 4) return NULL;
    if (buf[0] == 0xFF && buf[1] == 0xD8) return strdup("image/jpeg");
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') return strdup("image/png");
    if (len >= 12 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
        buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') return strdup("image/webp");
    if (len >= 6 && buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F') return strdup("image/gif");
    if (len >= 12 && buf[4] == 'f' && buf[5] == 't' && buf[6] == 'y' && buf[7] == 'p') {
        if (buf[8] == 'h' && buf[9] == 'e' && buf[10] == 'i' && buf[11] == 'c') return strdup("image/heic");
        if (buf[8] == 'a' && buf[9] == 'v' && buf[10] == 'i' && buf[11] == 'f') return strdup("image/avif");
    }
    return NULL;
}

/* PoP: _guess_mime @ agent/image_routing.py:_guess_mime */
char *imgr_guess_mime(const char *path, const unsigned char *raw, size_t raw_len) {
    /* Python: magic bytes first, filename fallback. */
    if (raw && raw_len >= 4) {
        char *sniff = imgr_sniff_mime_from_bytes(raw, raw_len);
        if (sniff) return sniff;
    }
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    char *l = lowerdup(dot);
    if (!l) return NULL;
    char *r = NULL;
    if (strcmp(l, ".jpg") == 0 || strcmp(l, ".jpeg") == 0) r = strdup("image/jpeg");
    else if (strcmp(l, ".png") == 0) r = strdup("image/png");
    else if (strcmp(l, ".webp") == 0) r = strdup("image/webp");
    else if (strcmp(l, ".gif") == 0) r = strdup("image/gif");
    else if (strcmp(l, ".heic") == 0) r = strdup("image/heic");
    else if (strcmp(l, ".avif") == 0) r = strdup("image/avif");
    else r = NULL;
    free(l);
    return r;
}

/* PoP: _file_to_data_url @ agent/image_routing.py:_file_to_data_url */
char *imgr_file_to_data_url(const char *path, long max_bytes) {
    /* Python: base64 data URL at native size. */
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    if (max_bytes > 0 && st.st_size > max_bytes) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char *buf = malloc((size_t)st.st_size + 1);
    size_t r = 0;
    if (buf) r = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    if (!buf) return NULL;
    char *mime = imgr_guess_mime(path, buf, r);
    /* base64 encode */
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = ((r + 2) / 3) * 4;
    char *b64 = malloc(out_len + 1);
    if (!b64) { free(buf); free(mime); return NULL; }
    size_t o = 0;
    for (size_t i = 0; i < r; i += 3) {
        unsigned v = buf[i] << 16;
        if (i + 1 < r) v |= buf[i+1] << 8;
        if (i + 2 < r) v |= buf[i+2];
        b64[o++] = tbl[(v >> 18) & 63];
        b64[o++] = tbl[(v >> 12) & 63];
        b64[o++] = (i + 1 < r) ? tbl[(v >> 6) & 63] : '=';
        b64[o++] = (i + 2 < r) ? tbl[v & 63] : '=';
    }
    b64[o] = '\0';
    char *out = NULL;
    asprintf(&out, "data:%s;base64,%s", mime ? mime : "image/png", b64);
    free(buf);
    free(b64);
    free(mime);
    return out;
}

/* PoP: build_native_content_parts @ agent/image_routing.py:build_native_content_parts */
char *imgr_build_native_content_parts(const char *image_url, const char *mime) {
    /* Python: OpenAI-style content part. */
    if (!image_url) return strdup("[]");
    char *out = NULL;
    asprintf(&out,
        "[{\"type\": \"image_url\", \"image_url\": {\"url\": \"%s\", \"detail\": \"auto\"}, \"mime_type\": \"%s\"}]",
        image_url, mime ? mime : "image/png");
    return out;
}
