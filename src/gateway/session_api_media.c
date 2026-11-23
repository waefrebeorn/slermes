/*
 * session_api_media.c — C port of upstream Hermes gateway/session_api_media.py.
 *
 * PoP: w12_sessionapi_cd0976_image_urls @ gateway/session_api_media.py:_image_urls
 * PoP: w12_sessionapi_cd0976_restore_images @ gateway/session_api_media.py:restore_api_images
 *
 * Pure content-shaping seams. Disk staging and native capture stay in Python;
 * media_paths is the restore_native_media seam injected from the fixture.
 */
#include "noc_w12_sessionapi_cd0976.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ci_start(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}
static const char *ci_find(const char *hay, const char *needle) {
    size_t hn = strlen(hay), nn = strlen(needle);
    if (!nn || nn > hn) return NULL;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t j = 0;
        while (j < nn && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nn) return hay + i;
    }
    return NULL;
}
static int valid_mime(const char *m) {
    static const char *const mimes[] = {"image/png", "image/jpeg", "image/gif", "image/webp"};
    for (size_t i = 0; i < 4; i++) if (!strcmp(m, mimes[i])) return 1;
    return 0;
}
/* Faithful enough to session_rules.api_payloads.data_url_bytes: strict alphabet. */
static int valid_b64(const char *s) {
    size_t n = strlen(s);
    if (n % 4) return 0;
    size_t pads = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '=') {
            pads++;
            if (i + 2 < n) return 0;   /* padding only in the final two slots */
        } else {
            if (pads) return 0;
            if (!isalnum(c) && c != '+' && c != '/') return 0;
        }
    }
    return pads <= 2;
}
/* data_url_bytes(url) is not None. */
static int is_committable_data_url(const char *url) {
    if (!url || !ci_start(url, "data:")) return 0;
    const char *comma = strchr(url, ',');
    /* str.partition(',') with no separator yields (url, "", ""). */
    size_t hlen = comma ? (size_t)(comma - url) : strlen(url);
    const char *encoded = comma ? comma + 1 : "";
    char *header = (char *)malloc(hlen + 1);
    if (!header) return 0;
    memcpy(header, url, hlen);
    header[hlen] = '\0';
    const char *semi = strchr(header + 5, ';');
    char mime[128];
    size_t mlen = semi ? (size_t)(semi - (header + 5)) : strlen(header + 5);
    if (mlen >= sizeof(mime)) mlen = sizeof(mime) - 1;
    for (size_t i = 0; i < mlen; i++) mime[i] = (char)tolower((unsigned char)header[5 + i]);
    mime[mlen] = '\0';
    /* strip() around the mime */
    while (mlen && (mime[mlen - 1] == ' ' || mime[mlen - 1] == '\t')) mime[--mlen] = '\0';
    size_t lead = 0;
    while (mime[lead] == ' ' || mime[lead] == '\t') lead++;
    int ok = valid_mime(mime + lead) && ci_find(header, ";base64") != NULL && valid_b64(encoded);
    free(header);
    return ok;
}

static void collect_urls(const json_t *content, json_t *urls) {
    if (!json_is_array(content)) return;
    for (size_t i = 0; i < json_len(content); i++) {
        const json_t *part = json_get(content, i);
        if (!json_is_object(part)) continue;
        const json_t *ty = json_obj_get(part, "type");
        if (!ty || ty->type != JSON_STRING || !ty->str_val || strcmp(ty->str_val, "image_url")) continue;
        const json_t *iu = json_obj_get(part, "image_url");
        const json_t *u = json_is_object(iu) ? json_obj_get(iu, "url") : NULL;
        json_append(urls, json_string(u && u->type == JSON_STRING && u->str_val ? u->str_val : ""));
    }
}

json_t *w12_sessionapi_cd0976_image_urls(const struct json_t *content) {
    json_t *urls = json_array();
    collect_urls(content, urls);
    return urls;
}

json_t *w12_sessionapi_cd0976_restore_images(const struct json_t *content, const struct json_t *media_paths) {
    json_t *urls = json_array();
    collect_urls(content, urls);
    size_t nhints = json_len(urls);
    if (!nhints) { json_free(urls); return json_is_array(content) ? json_copy(content) : json_array(); }

    /* Build the joined hint line, then append it to the first text part. */
    size_t cap = 256 + nhints * 128;
    char *joined = (char *)malloc(cap);
    if (!joined) { json_free(urls); return json_array(); }
    joined[0] = '\0';
    size_t used = 0, path_i = 0;
    for (size_t i = 0; i < nhints; i++) {
        const json_t *uv = json_get(urls, i);
        const char *url = (uv && uv->type == JSON_STRING && uv->str_val) ? uv->str_val : "";
        char hint[512];
        if (is_committable_data_url(url)) {
            const json_t *p = json_is_array(media_paths) ? json_get(media_paths, path_i++) : NULL;
            const char *ps = (p && p->type == JSON_STRING && p->str_val) ? p->str_val : "None";
            snprintf(hint, sizeof(hint), "[Image attached at: %s]", ps);
        } else {
            snprintf(hint, sizeof(hint), "[Image attached: %s]", url);
        }
        size_t hl = strlen(hint);
        if (used + hl + 4 > cap) {
            cap = (used + hl + 4) * 2;
            char *tmp = (char *)realloc(joined, cap);
            if (!tmp) break;
            joined = tmp;
        }
        if (i) joined[used++] = '\n';
        memcpy(joined + used, hint, hl + 1);
        used += hl;
    }
    json_free(urls);

    json_t *parts = json_is_array(content) ? json_copy(content) : json_array();
    json_t *text_part = NULL;
    for (size_t i = 0; i < json_len(parts); i++) {
        json_t *p = json_get(parts, i);
        const json_t *ty = json_obj_get(p, "type");
        if (ty && ty->type == JSON_STRING && ty->str_val && !strcmp(ty->str_val, "text")) { text_part = p; break; }
    }
    if (!text_part) {
        json_t *ordered = json_array();
        json_t *fresh = json_object();
        json_set(fresh, "type", json_string("text"));
        json_set(fresh, "text", json_string("What do you see in this image?"));
        json_append(ordered, fresh);
        for (size_t i = 0; i < json_len(parts); i++) json_append(ordered, json_copy(json_get(parts, i)));
        json_free(parts);
        parts = ordered;
        text_part = json_get(parts, 0);
    }
    const char *old = json_get_str(text_part, "text", "");
    size_t need = strlen(old) + 2 + used + 1;
    char *combined = (char *)malloc(need);
    if (combined) {
        snprintf(combined, need, "%s\n\n%s", old, joined);
        json_set(text_part, "text", json_string(combined));   /* frees the old value */
        free(combined);
    }
    free(joined);
    return parts;
}

