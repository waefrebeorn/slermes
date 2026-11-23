/*
 * session_history.c - C home for upstream Hermes tui_gateway/session_history.py.
 * PoP: w15_sessionhis_503dfd_history_part_image_url @ tui_gateway/session_history.py:_history_part_image_url
 * PoP: w15_sessionhis_503dfd_history_dict_text @ tui_gateway/session_history.py:_history_dict_text
 * PoP: w15_sessionhis_503dfd_content_display_text @ tui_gateway/session_history.py:_content_display_text
 * PoP: w15_sessionhis_503dfd_coerce_message_text @ tui_gateway/session_history.py:_coerce_message_text
 * PoP: w15_sessionhis_503dfd_is_display_hidden_marker @ tui_gateway/session_history.py:_is_display_hidden_marker
 * PoP: w15_sessionhis_503dfd_strip_trailing_screenshot_placeholders @ tui_gateway/session_history.py:_strip_trailing_screenshot_placeholders
 * PoP: w15_sessionhis_503dfd_coerce_seed_history @ tui_gateway/session_history.py:_coerce_seed_history
 * PoP: w15_sessionhis_503dfd_inflight_text @ tui_gateway/session_history.py:_inflight_text
 *
 * Faithful ports of the pure message/content projection helpers used by the
 * history→wire conversion. Agent/DB/file state stays with the live Python owner.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionhis_503dfd.h"

#define SCREENSHOT_PLACEHOLDER "[screenshot]"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

typedef struct { char *p; size_t n, cap; } sbuf;
static void sb_add(sbuf *b, const char *s) {
    if (!s) return;
    size_t k = strlen(s);
    if (!k) return;
    if (b->n + k + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 32;
        while (nc < b->n + k + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = '\0';
}
static char *sb_take(sbuf *b) {
    char *r = b->p ? b->p : dupstr("");
    b->p = NULL; b->n = b->cap = 0;
    return r;
}

static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}

/* Python str() for JSON scalars (fixtures keep to scalars for these shapes). */
static char *str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}
static char *str_or_empty(const json_t *v) { return falsy(v) ? dupstr("") : str_of(v); }

/* In-place ASCII strip; returns the ORIGINAL allocation start (safe to free). */
static char *strip(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}

static int in_kinds(const char *k, const char *const *kinds, size_t n) {
    if (!k || !*k) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(k, kinds[i])) return 1;
    return 0;
}
static const char *const TEXT_KINDS[] = {"text", "input_text", "output_text"};
static const char *const IMAGE_KINDS[] = {"image_url", "input_image", "image"};
static const char *const AUDIO_KINDS[] = {"input_audio", "audio"};

static char *image_url_of(const json_t *part) {
    json_t *iu = json_obj_get(part, "image_url");
    if (iu && iu->type == JSON_OBJECT) iu = json_obj_get(iu, "url");
    return dupstr(iu && iu->type == JSON_STRING && iu->str_val ? iu->str_val : "");
}

static char *hist_dict_text(const json_t *c, int image_urls) {
    const char *kind = json_get_str(c, "type", "");
    if (in_kinds(kind, TEXT_KINDS, 3)) {
        json_t *a = json_obj_get(c, "text"), *b = json_obj_get(c, "content");
        if (!falsy(a)) return str_or_empty(a);
        if (!falsy(b)) return str_or_empty(b);
        return dupstr("");
    }
    if (in_kinds(kind, IMAGE_KINDS, 3)) {
        if (image_urls) {
            char *u = image_url_of(c);
            if (*u) return u;
            free(u);
        }
        return dupstr("[image]");
    }
    if (in_kinds(kind, AUDIO_KINDS, 2)) return dupstr("[audio]");
    if (kind && *kind) {
        sbuf b = {0}; sb_add(&b, "["); sb_add(&b, kind); sb_add(&b, "]");
        return sb_take(&b);
    }
    if (json_has(c, "text")) return str_or_empty(json_obj_get(c, "text"));
    return dupstr("[structured content]");
}

static char *content_display_text(const json_t *c) {
    if (c && c->type == JSON_ARRAY) {
        sbuf b = {0};
        for (size_t i = 0; i < json_len(c); i++) {
            char *t = strip(content_display_text(json_get(c, i)));
            if (*t) {
                if (b.n) sb_add(&b, "\n");
                sb_add(&b, t);
            }
            free(t);
        }
        return sb_take(&b);
    }
    if (c && c->type == JSON_OBJECT) return hist_dict_text(c, 0);
    if (!c || c->type == JSON_NULL) return dupstr("");
    return str_of(c);
}

static char *coerce_message_text(const json_t *c, int image_urls) {
    if (c && c->type == JSON_ARRAY) {
        sbuf b = {0};
        for (size_t i = 0; i < json_len(c); i++) {
            json_t *part = json_get(c, i);
            if (part && part->type == JSON_STRING) {
                sb_add(&b, part->str_val ? part->str_val : "");
            } else if (part && part->type == JSON_OBJECT) {
                json_t *txt = json_obj_get(part, "text");
                if (txt && txt->type == JSON_STRING) {
                    sb_add(&b, txt->str_val ? txt->str_val : "");
                } else {
                    const char *k = json_get_str(part, "type", "");
                    if (k && *k) {
                        char *rendered = hist_dict_text(part, image_urls);
                        if (!in_kinds(k, TEXT_KINDS, 3)) sb_add(&b, "\n");
                        sb_add(&b, rendered);
                        free(rendered);
                    }
                }
            }
        }
        return sb_take(&b);
    }
    if (c && c->type == JSON_OBJECT) return hist_dict_text(c, image_urls);
    if (!c || c->type == JSON_NULL) return dupstr("");
    return str_of(c);
}

struct json_t *w15_sessionhis_503dfd_history_part_image_url(const struct json_t *part) {
    char *u = image_url_of(part);
    json_t *out = json_string(u);
    free(u);
    return out;
}

struct json_t *w15_sessionhis_503dfd_history_dict_text(const struct json_t *content, int image_urls) {
    char *s = hist_dict_text(content, image_urls);
    json_t *out = json_string(s);
    free(s);
    return out;
}

struct json_t *w15_sessionhis_503dfd_content_display_text(const struct json_t *content) {
    char *s = content_display_text(content);
    json_t *out = json_string(s);
    free(s);
    return out;
}

struct json_t *w15_sessionhis_503dfd_coerce_message_text(const struct json_t *content, int image_urls) {
    char *s = coerce_message_text(content, image_urls);
    json_t *out = json_string(s);
    free(s);
    return out;
}

struct json_t *w15_sessionhis_503dfd_is_display_hidden_marker(const char *role, const char *text) {
    int ok = role && !strcmp(role, "user") && text;
    if (ok) {
        char *s = strip(dupstr(text));
        ok = !strncmp(s, "[System:", 8);
        free(s);
    }
    return json_bool(ok ? 1 : 0);
}

struct json_t *w15_sessionhis_503dfd_strip_trailing_screenshot_placeholders(const char *text) {
    char *buf = dupstr(text ? text : "");
    size_t len = strlen(buf), slen = strlen(SCREENSHOT_PLACEHOLDER);
    int count = 0;
    while (len > 0) {
        size_t start = len;
        while (start > 0 && buf[start - 1] != '\n') start--;
        if (len - start == slen && !memcmp(buf + start, SCREENSHOT_PLACEHOLDER, slen)) {
            count++;
            len = start > 0 ? start - 1 : 0;   /* drop the newline before the line */
            buf[len] = '\0';                    /* NUL at the new length */
        } else {
            break;
        }
    }
    json_t *out = json_object();
    json_set(out, "text", json_string(buf));
    json_set(out, "count", json_number((double)count));
    free(buf);
    return out;
}

struct json_t *w15_sessionhis_503dfd_coerce_seed_history(const struct json_t *value) {
    json_t *out = json_array();
    if (!value || value->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(value); i++) {
        json_t *item = json_get(value, i);
        if (!item || item->type != JSON_OBJECT) continue;
        const char *role = json_get_str(item, "role", "");
        if (strcmp(role, "user") && strcmp(role, "assistant") && strcmp(role, "system")) continue;
        json_t *content = json_obj_get(item, "content");
        if (!content || content->type == JSON_NULL) content = json_obj_get(item, "text");
        if (!content || content->type != JSON_STRING) continue;
        char *probe = strip(dupstr(content->str_val));
        int blank = !*probe;
        free(probe);
        if (blank) continue;
        json_t *row = json_object();
        json_set(row, "role", json_string(role));
        json_set(row, "content", json_copy(content));
        if (!strcmp(json_get_str(item, "display_kind", ""), "hidden"))
            json_set(row, "display_kind", json_string("hidden"));
        json_append(out, row);
    }
    return out;
}

struct json_t *w15_sessionhis_503dfd_inflight_text(const struct json_t *value) {
    char *s = strip(content_display_text(value));
    json_t *out = json_string(s);
    free(s);
    return out;
}

