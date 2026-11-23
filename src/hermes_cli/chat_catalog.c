/*
 * chat_catalog.c — C port of upstream Hermes hermes_cli/chat_catalog.py.
 *
 * PoP: w9_chatcatalog_a2a9d5_model_id_is_generation @ hermes_cli/chat_catalog.py:model_id_is_generation
 * PoP: w9_chatcatalog_a2a9d5_tag_list_is_generation @ hermes_cli/chat_catalog.py:_tag_list_is_generation
 * PoP: w9_chatcatalog_a2a9d5_output_modalities_are_generation @ hermes_cli/chat_catalog.py:_output_modalities_are_generation
 * PoP: w9_chatcatalog_a2a9d5_catalog_item_is_generation @ hermes_cli/chat_catalog.py:catalog_item_is_generation
 *
 * Only the pure decision surface is ported; process-local _seen_generation_ids
 * (note_generation_model / chat_catalog_ids) is deliberately not modelled.
 */
#include "noc_w9_chatcatalog_a2a9d5.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int is_sep(char c) {
    return c == '/' || c == '_' || c == '.' || c == '-';
}

/* str(tag or "").strip().lower() for the JSON values fixtures can carry. */
static void py_str_lower_strip(const json_t *v, char *out, size_t cap) {
    size_t n = 0;
    const char *p = NULL;
    char numbuf[64];
    if (!v || v->type == JSON_NULL) {
        p = "";
    } else if (v->type == JSON_STRING) {
        p = v->str_val ? v->str_val : "";
    } else if (v->type == JSON_BOOL) {
        p = v->bool_val ? "true" : "false";
    } else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) { p = ""; }
        else {
            if (d == floor(d) && fabs(d) < 1e15) snprintf(numbuf, sizeof(numbuf), "%.0f", d);
            else snprintf(numbuf, sizeof(numbuf), "%g", d);
            p = numbuf;
        }
    } else {
        p = ""; /* [] / {} -> str() is '[]'/'{}', never a generation token */
    }
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    while (p < e && n + 1 < cap) out[n++] = (char)tolower((unsigned char)*p++);
    out[n] = '\0';
}

static int ci_prefix(const char *p, const char *lit, size_t *len) {
    size_t i = 0;
    while (lit[i] && (unsigned char)tolower((unsigned char)p[i]) == (unsigned char)tolower((unsigned char)lit[i])) i++;
    if (!lit[i]) { *len = i; return 1; }
    *len = 0;
    return 0;
}

/* Match one _GENERATION_ID_RE alternative starting at s[i] (i is a boundary). */
static int alt_at(const char *s, size_t i, size_t end, size_t *toklen) {
    const char *p = s + i;
    size_t L, k;
    /* image(?:[/_.-]gen(?:eration)?)?  /  video(?:[/_.-]gen(?:eration)?)? */
    const char *bases[2] = { "image", "video" };
    for (k = 0; k < 2; k++) {
        if (!ci_prefix(p, bases[k], &L)) continue;
        size_t j = i + L;
        if (j < end && is_sep(s[j])) {
            size_t gl;
            if (ci_prefix(s + j + 1, "gen", &gl)) {
                size_t nj = j + 1 + gl;
                size_t el;
                if (ci_prefix(s + nj, "eration", &el)) nj += el;
                j = nj;
            }
        }
        *toklen = j - i;
        return 1;
    }
    /* text[/_.-]to[/_.-](?:image|video) */
    if (ci_prefix(p, "text", &L) && i + L < end && is_sep(s[i + L])) {
        size_t t = i + L + 1;
        size_t tl;
        if (ci_prefix(s + t, "to", &tl)) {
            t += tl;
            if (t < end && is_sep(s[t])) {
                t++;
                size_t al;
                if (ci_prefix(s + t, "image", &al) || ci_prefix(s + t, "video", &al)) {
                    *toklen = (t + al) - i;
                    return 1;
                }
            }
        }
    }
    const char *lits[] = { "stable-diffusion", "sdxl", "flux" };
    for (k = 0; k < 3; k++) {
        if (ci_prefix(p, lits[k], &L)) { *toklen = L; return 1; }
    }
    *toklen = 0;
    return 0;
}

int w9_chatcatalog_a2a9d5_model_id_is_generation(const json_t *model_id) {
    char buf[4096];
    py_str_lower_strip(model_id, buf, sizeof(buf));
    size_t end = strlen(buf);
    for (size_t i = 0; i < end; i++) {
        if (i != 0 && !is_sep(buf[i - 1])) continue;
        size_t toklen;
        if (!alt_at(buf, i, end, &toklen)) continue;
        size_t after = i + toklen;
        if (after == end || is_sep(buf[after])) return 1;
    }
    return 0;
}

static int in_generation_types(const char *s) {
    static const char *const types[] = {
        "image", "image_generation", "image-generation", "image-gen",
        "video", "video_generation", "video-generation", "video-gen", NULL
    };
    for (int i = 0; types[i]; i++) if (!strcmp(s, types[i])) return 1;
    return 0;
}

int w9_chatcatalog_a2a9d5_tag_list_is_generation(const json_t *tags) {
    if (!tags || tags->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < tags->c.count; i++) {
        char s[512];
        py_str_lower_strip(json_get(tags, i), s, sizeof(s));
        if (in_generation_types(s)) return 1;
    }
    return 0;
}

int w9_chatcatalog_a2a9d5_output_modalities_are_generation(const json_t *outputs) {
    if (!outputs || outputs->type != JSON_ARRAY || outputs->c.count == 0) return 0;
    int gen = 0, chat = 0;
    for (size_t i = 0; i < outputs->c.count; i++) {
        char s[512];
        py_str_lower_strip(json_get(outputs, i), s, sizeof(s));
        if (!s[0]) continue;
        if (!strcmp(s, "image") || !strcmp(s, "video")) gen = 1;
        if (!strcmp(s, "text") || !strcmp(s, "chat")) chat = 1;
    }
    return gen && !chat;
}

int w9_chatcatalog_a2a9d5_catalog_item_is_generation(const json_t *item) {
    if (!item || item->type != JSON_OBJECT) return 0;
    if (w9_chatcatalog_a2a9d5_model_id_is_generation(json_obj_get(item, "id"))) return 1;
    json_t *cap = json_obj_get(item, "capabilities");
    if (cap && cap->type == JSON_OBJECT) {
        char mt[512];
        py_str_lower_strip(json_obj_get(cap, "type"), mt, sizeof(mt));
        if (in_generation_types(mt)) return 1;
        if (w9_chatcatalog_a2a9d5_tag_list_is_generation(json_obj_get(cap, "tags"))) return 1;
    }
    json_t *tt = json_obj_get(item, "type");
    if (tt && tt->type == JSON_STRING) {
        char st[512];
        py_str_lower_strip(tt, st, sizeof(st));
        if (in_generation_types(st)) return 1;
    }
    if (w9_chatcatalog_a2a9d5_tag_list_is_generation(json_obj_get(item, "tags"))) return 1;
    json_t *arch = json_obj_get(item, "architecture");
    if (arch && arch->type == JSON_OBJECT &&
        w9_chatcatalog_a2a9d5_output_modalities_are_generation(json_obj_get(arch, "output_modalities"))) return 1;
    json_t *meta = json_obj_get(item, "metadata");
    if (meta && meta->type == JSON_OBJECT &&
        w9_chatcatalog_a2a9d5_tag_list_is_generation(json_obj_get(meta, "tags"))) return 1;
    return 0;
}
