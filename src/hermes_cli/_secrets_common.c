/*
 * _secrets_common.c — faithful C11 port of selected pure helpers in
 * hermes_cli/_secrets_common.py (gate w13_secretsco_6e907b).
 * PoP: w13_secretsco_6e907b_console_safe @ hermes_cli/_secrets_common.py:console_safe
 * PoP: w13_secretsco_6e907b_yn @ hermes_cli/_secrets_common.py:yn
 * PoP: w13_secretsco_6e907b_section_cfg @ hermes_cli/_secrets_common.py:section_cfg
 * PoP: w13_secretsco_6e907b_cfg_str @ hermes_cli/_secrets_common.py:cfg_str
 * PoP: w13_secretsco_6e907b_arg @ hermes_cli/_secrets_common.py:arg
 * PoP: w13_secretsco_6e907b_flag @ hermes_cli/_secrets_common.py:flag
 */
#include "noc_w13_secretsco_6e907b.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } sbuf;
static void sb_init(sbuf *b) {
    b->cap = 256; b->len = 0; b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void sb_grow(sbuf *b, size_t need) {
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    sb_grow(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sbuf *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

/* Python re.compile(r"[\x00-\x08\x0b-\x1f\x7f-\x9f]") */
static int is_control_char(unsigned char c) {
    return (c <= 0x08) || (c >= 0x0b && c <= 0x1f) || (c >= 0x7f && c <= 0x9f);
}

/* rich.markup.escape regex: (\*)(\[[a-z#/@][^[]*?]) */
static int tag_first_ok(unsigned char c) {
    return (c >= 'a' && c <= 'z') || c == '#' || c == '@' || c == '/';
}
/* end index (inclusive of ']') of a tag beginning at '[' index p, else -1 */
static long long tag_end(const char *s, size_t n, size_t p) {
    if (p + 1 >= n || !tag_first_ok((unsigned char)s[p + 1])) return -1;
    for (size_t k = p + 2; k < n; k++) {
        if (s[k] == '[') return -1;
        if (s[k] == ']') return (long long)k;
    }
    return -1;
}
static void rich_escape(const char *s, sbuf *out) {
    size_t n = strlen(s), i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') {
            size_t k = 0;
            while (i + k < n && s[i + k] == '\\') k++;
            size_t p = i + k;
            long long e = (p < n) ? tag_end(s, n, p) : -1;
            if (e >= 0) {
                for (size_t z = 0; z < k; z++) sb_add(out, "\\");
                for (size_t z = 0; z < k; z++) sb_add(out, "\\");
                sb_add(out, "\\");
                sb_addn(out, s + p, (size_t)e - p + 1);
                i = (size_t)e + 1;
                continue;
            }
            sb_add(out, "\\"); i++;
            continue;
        }
        if (c == '[') {
            long long e = tag_end(s, n, i);
            if (e >= 0) {
                sb_add(out, "\\");
                sb_addn(out, s + i, (size_t)e - i + 1);
                i = (size_t)e + 1;
                continue;
            }
        }
        sb_addn(out, &s[i], 1); i++;
    }
    size_t l = out->len;
    if (l >= 1 && out->p[l - 1] == '\\' && (l < 2 || out->p[l - 2] != '\\'))
        sb_add(out, "\\");
}

char *w13_secretsco_6e907b_console_safe(const char *text) {
    if (!text) return NULL;
    sbuf stripped; sb_init(&stripped);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (!is_control_char(*p)) sb_addn(&stripped, (const char *)p, 1);
    sbuf out; sb_init(&out);
    rich_escape(stripped.p ? stripped.p : "", &out);
    free(stripped.p);
    return sb_take(&out);
}

const char *w13_secretsco_6e907b_yn(bool b) {
    return b ? "[green]yes[/green]" : "[dim]no[/dim]";
}

static int json_falsy(const json_t *v) {
    if (!v) return 1;
    switch (v->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return !v->bool_val;
    case JSON_NUMBER: return v->num_val == 0.0;
    case JSON_STRING: return !v->str_val || v->str_val[0] == '\0';
    case JSON_ARRAY: case JSON_OBJECT: return v->c.count == 0;
    }
    return 1;
}

json_t *w13_secretsco_6e907b_section_cfg(const json_t *cfg, const char *section) {
    if (!cfg || cfg->type != JSON_OBJECT) return json_object();
    json_t *sec = json_obj_get(cfg, "secrets");
    if (json_falsy(sec) || sec->type != JSON_OBJECT) return json_object();
    json_t *v = json_obj_get(sec, section);
    if (json_falsy(v) || v->type != JSON_OBJECT) return json_object();
    return json_copy(v);
}

char *w13_secretsco_6e907b_cfg_str(const json_t *cfg, const char *key) {
    if (!cfg || cfg->type != JSON_OBJECT) return strdup("");
    json_t *v = json_obj_get(cfg, key);
    if (json_falsy(v)) return strdup("");
    char *raw;
    if (v->type == JSON_STRING) raw = strdup(v->str_val ? v->str_val : "");
    else if (v->type == JSON_BOOL) raw = strdup(v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9e15)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        raw = strdup(buf);
    } else {
        char *s = json_dumps(v, 0);
        raw = s ? s : strdup("");
    }
    char *a = raw;
    while (*a && isspace((unsigned char)*a)) a++;
    char *b = raw + strlen(raw);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *res = (char *)malloc((size_t)(b - a) + 1);
    memcpy(res, a, (size_t)(b - a));
    res[b - a] = '\0';
    free(raw);
    return res;
}

json_t *w13_secretsco_6e907b_arg(const char *name, const char *help_text, const json_t *kwargs) {
    json_t *arr = json_array();
    json_append(arr, json_string(name ? name : ""));
    json_t *d = json_object();
    json_set(d, "help", json_string(help_text ? help_text : ""));
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(kwargs); i++)
            json_set(d, json_object_get_key_at(kwargs, i),
                     json_copy(json_object_get_at(kwargs, i)));
    }
    json_append(arr, d);
    return arr;
}

json_t *w13_secretsco_6e907b_flag(const char *name, const char *help_text) {
    json_t *kw = json_object();
    json_set(kw, "action", json_string("store_true"));
    json_t *r = w13_secretsco_6e907b_arg(name, help_text, kw);
    json_free(kw);
    return r;
}
