/*
 * vision_message_prep.c — faithful C11 port of agent/vision_message_prep.py.
 * Gate w11_visionmess_0cd4b2.
 *
 * PoP: w11_visionmess_0cd4b2_is_image_part @ agent/vision_message_prep.py:_is_image_part
 * PoP: w11_visionmess_0cd4b2_salvage_text_parts @ agent/vision_message_prep.py:_salvage_text_parts
 * PoP: w11_visionmess_0cd4b2_content_has_image_parts @ agent/vision_message_prep.py:VisionMessagePrepMixin._content_has_image_parts
 * PoP: w11_visionmess_0cd4b2_provider_model_key @ agent/vision_message_prep.py:_provider_model_key
 * PoP: w11_visionmess_0cd4b2_anthropic_preserve_dots @ agent/vision_message_prep.py:VisionMessagePrepMixin._anthropic_preserve_dots
 * PoP: w11_visionmess_0cd4b2_is_qwen_portal @ agent/vision_message_prep.py:VisionMessagePrepMixin._is_qwen_portal
 */
#include "hermes_json.h"
#include "noc_w11_visionmess_0cd4b2.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *strip_c(const char *s) {
    size_t a, b;
    char *out;
    if (!s) return dup_cstr("");
    a = 0; b = strlen(s);
    while (a < b && is_ws((unsigned char)s[a])) a++;
    while (b > a && is_ws((unsigned char)s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    if (b > a) memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static char *lower_c(const char *s) {
    size_t i, n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}
/* str(value) for the scalar shapes a text part can carry. */
static char *py_str(const json_t *v) {
    char buf[64];
    const char *s = NULL;
    if (!v) return dup_cstr("");
    switch (v->type) {
        case JSON_NULL:   s = "None"; break;
        case JSON_STRING: s = v->str_val ? v->str_val : ""; break;
        case JSON_BOOL:   s = v->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            s = buf;
            break;
        default: return dup_cstr("");
    }
    return dup_cstr(s);
}

/* ---- _is_image_part / _content_has_image_parts ---- */
int w11_visionmess_0cd4b2_is_image_part(const json_t *part) {
    const char *t;
    if (!part || part->type != JSON_OBJECT) return 0;
    t = json_get_str(part, "type", "");
    return !strcmp(t, "image_url") || !strcmp(t, "input_image");
}
int w11_visionmess_0cd4b2_content_has_image_parts(const json_t *content) {
    size_t i;
    if (!content || content->type != JSON_ARRAY) return 0;
    for (i = 0; i < content->c.count; i++)
        if (w11_visionmess_0cd4b2_is_image_part(content->c.items[i])) return 1;
    return 0;
}

/* ---- _salvage_text_parts ---- */
json_t *w11_visionmess_0cd4b2_salvage_text_parts(const json_t *content, int any_dict_text) {
    json_t *out = json_array();
    size_t i;
    if (!content || content->type != JSON_ARRAY) return out;
    for (i = 0; i < content->c.count; i++) {
        json_t *p = content->c.items[i];
        char *raw = NULL, *text;
        if (p && p->type == JSON_STRING) {
            raw = dup_cstr(p->str_val ? p->str_val : "");
        } else if (p && p->type == JSON_OBJECT) {
            const char *t = json_get_str(p, "type", "");
            int text_type = !strcmp(t, "text") || !strcmp(t, "input_text");
            if (!any_dict_text && !text_type) continue;
            {
                json_t *v = json_obj_get(p, "text");
                if (!v || v->type == JSON_NULL) raw = dup_cstr("");
                else {
                    if (v->type == JSON_NUMBER && v->num_val == 0.0) raw = dup_cstr("");
                    else if (v->type == JSON_BOOL && !v->bool_val) raw = dup_cstr("");
                    else raw = py_str(v);
                }
            }
        } else continue;
        text = strip_c(raw ? raw : "");
        free(raw);
        if (text && *text) json_append(out, json_string(text));
        free(text);
    }
    return out;
}

/* ---- _provider_model_key ---- */
json_t *w11_visionmess_0cd4b2_provider_model_key(const char *provider, const char *model) {
    char *p = strip_c(provider ? provider : "");
    char *pl = lower_c(p ? p : "");
    char *m = strip_c(model ? model : "");
    json_t *out = json_array();
    json_append(out, json_string(pl ? pl : ""));
    json_append(out, json_string(m ? m : ""));
    free(p); free(pl); free(m);
    return out;
}

/* ---- verbatim port of utils/urls.py base_url_hostname / base_url_host_matches ---- */
static char *base_url_hostname(const char *base_url) {
    char *raw = strip_c(base_url ? base_url : "");
    const char *auth;
    size_t j, end, at = (size_t)-1, colon = (size_t)-1;
    char *host, *lower, *ret;
    if (!raw || !*raw) { free(raw); return dup_cstr(""); }
    {
        const char *scheme = strstr(raw, "://");
        auth = scheme ? scheme + 3 : raw;
    }
    end = strlen(auth);
    for (j = 0; j < end; j++) if (auth[j] == '/' || auth[j] == '?' || auth[j] == '#') { end = j; break; }
    for (j = 0; j < end; j++) if (auth[j] == '@') at = j;
    {
        size_t start = (at == (size_t)-1) ? 0 : at + 1;
        size_t hlen;
        if (auth[start] == '[') { /* IPv6 literal: keep through ] */
            size_t k = start;
            while (k < end && auth[k] != ']') k++;
            hlen = (k < end) ? (size_t)(k + 1 - start) : end - start;
        } else {
            for (j = start; j < end; j++) if (auth[j] == ':') { colon = j; break; }
            hlen = (colon == (size_t)-1) ? end - start : colon - start;
        }
        host = (char *)malloc(hlen + 1);
        if (!host) { free(raw); return dup_cstr(""); }
        if (hlen) memcpy(host, auth + start, hlen);
        host[hlen] = '\0';
    }
    lower = lower_c(host);
    free(host); free(raw);
    if (!lower) return dup_cstr("");
    while (strlen(lower) && lower[strlen(lower) - 1] == '.') lower[strlen(lower) - 1] = '\0';
    ret = lower;
    return ret;
}
static int base_url_host_matches(const char *base_url, const char *domain) {
    char *host = base_url_hostname(base_url);
    char *d = strip_c(domain ? domain : "");
    char *dl;
    int ok = 0;
    size_t hl, dl_len;
    if (!d) { free(host); return 0; }
    dl = lower_c(d);
    free(d);
    if (!dl) { free(host); return 0; }
    while (strlen(dl) && dl[strlen(dl) - 1] == '.') dl[strlen(dl) - 1] = '\0';
    hl = strlen(host); dl_len = strlen(dl);
    if (hl && dl_len) {
        if (!strcmp(host, dl)) ok = 1;
        else if (hl > dl_len && host[hl - dl_len - 1] == '.' && !strcmp(host + hl - dl_len, dl)) ok = 1;
    }
    free(host); free(dl);
    return ok;
}

/* ---- _anthropic_preserve_dots / _is_qwen_portal ---- */
int w11_visionmess_0cd4b2_anthropic_preserve_dots(const char *provider, const char *base_url) {
    static const char *const PROVIDERS[] = {
        "alibaba", "minimax", "minimax-cn", "opencode-go", "opencode-zen",
        "zai", "bedrock", "xiaomi", "vertex"
    };
    char *pl, *base, *host;
    int ok = 0;
    size_t i;
    pl = lower_c(provider ? provider : "");
    for (i = 0; i < sizeof(PROVIDERS) / sizeof(PROVIDERS[0]); i++) {
        if (pl && !strcmp(pl, PROVIDERS[i])) { free(pl); return 1; }
    }
    free(pl);
    base = lower_c(base_url ? base_url : "");
    host = base_url_hostname(base ? base : "");
    if (host && strstr(host, "dashscope")) ok = 1;
    else if (base_url_host_matches(base ? base : "", "aliyuncs.com")) ok = 1;
    else if (host && strstr(host, "minimax")) ok = 1;
    else if (base_url_host_matches(base ? base : "", "opencode.ai") && base && strstr(base, "/zen/")) ok = 1;
    else if (base_url_host_matches(base ? base : "", "bigmodel.cn")) ok = 1;
    else if (base_url_host_matches(base ? base : "", "xiaomimimo.com")) ok = 1;
    else if (base_url_host_matches(base ? base : "", "aiplatform.googleapis.com")) ok = 1;
    else if (host && !strncmp(host, "bedrock-runtime.", strlen("bedrock-runtime."))) ok = 1;
    free(host); free(base);
    return ok;
}
int w11_visionmess_0cd4b2_is_qwen_portal(const char *base_url_lower) {
    return base_url_host_matches(base_url_lower ? base_url_lower : "", "portal.qwen.ai");
}

