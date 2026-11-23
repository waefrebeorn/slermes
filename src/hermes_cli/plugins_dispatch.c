/*
 * plugins_dispatch.c — real C11 port of the pure prompt-section / hook-identity
 * helpers of upstream Hermes hermes_cli/plugins_dispatch.py (gate
 * w15_pluginsdis_6f9432).
 *
 * PoP: w15_pluginsdis_6f9432_is_valid_system_prompt_section_id @ hermes_cli/plugins_dispatch.py:is_valid_system_prompt_section_id
 * PoP: w15_pluginsdis_6f9432_format_system_prompt_section @ hermes_cli/plugins_dispatch.py:format_system_prompt_section
 * PoP: w15_pluginsdis_6f9432_format_system_prompt_sections @ hermes_cli/plugins_dispatch.py:format_system_prompt_sections
 * PoP: w15_pluginsdis_6f9432_hook_call_identity @ hermes_cli/plugins_dispatch.py:_hook_call_identity
 */
#include "noc_w15_pluginsdis_6f9432.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

typedef struct { char *p; size_t n, cap; } lbuf_t;

static int lb_reserve(lbuf_t *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 128;
    while (cap < b->n + extra + 1) cap *= 2;
    char *q = (char *)realloc(b->p, cap);
    if (!q) return 0;
    b->p = q;
    b->cap = cap;
    return 1;
}
static void lb_init(lbuf_t *b) {
    b->p = NULL; b->n = 0; b->cap = 0;
    if (lb_reserve(b, 1) && b->p) b->p[0] = '\0';
}
static void lb_add(lbuf_t *b, const char *s, size_t len) {
    if (!lb_reserve(b, len)) return;
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}
static void lb_adds(lbuf_t *b, const char *s) { lb_add(b, s, strlen(s)); }

int w15_pluginsdis_6f9432_is_valid_system_prompt_section_id(const char *value) {
    if (!value) return 0;
    size_t n = strlen(value);
    if (n < 1 || n > 128) return 0;
    unsigned char c0 = (unsigned char)value[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* Python len() counts Unicode code points; UTF-8 continuation bytes are not
 * code-point starts, so count lead bytes. */
static size_t utf8_codepoints(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

char *w15_pluginsdis_6f9432_format_system_prompt_section(const char *section_id,
                                                        const char *content) {
    const char *sid = section_id ? section_id : "";
    const char *body = content ? content : "";
    char num[32];
    snprintf(num, sizeof(num), "%zu", utf8_codepoints(body));
    lbuf_t b;
    lb_init(&b);
    lb_adds(&b, "## Plugin Context: ");
    lb_adds(&b, sid);
    lb_adds(&b, "\n<!-- hermes-plugin-section-chars:");
    lb_adds(&b, num);
    lb_adds(&b, " -->\n\n");
    lb_adds(&b, body);
    if (!b.p) return dupstr("");
    return b.p;
}

char *w15_pluginsdis_6f9432_format_system_prompt_sections(json_t *sections) {
    lbuf_t b;
    lb_init(&b);
    if (!sections || sections->type != JSON_ARRAY || json_len(sections) == 0)
        return b.p ? b.p : dupstr("");
    lb_adds(&b, "<!-- hermes-plugin-sections:start -->\n");
    for (size_t i = 0; i < json_len(sections); i++) {
        json_t *obj = json_get(sections, i);
        const char *id = "";
        const char *content = "";
        if (obj && obj->type == JSON_OBJECT) {
            json_t *iv = json_obj_get(obj, "id");
            json_t *cv = json_obj_get(obj, "content");
            if (iv && iv->type == JSON_STRING && iv->str_val) id = iv->str_val;
            if (cv && cv->type == JSON_STRING && cv->str_val) content = cv->str_val;
        }
        if (i) lb_adds(&b, "\n\n");
        char *block = w15_pluginsdis_6f9432_format_system_prompt_section(id, content);
        if (block) { lb_adds(&b, block); free(block); }
    }
    lb_adds(&b, "\n<!-- hermes-plugin-sections:end -->");
    return b.p ? b.p : dupstr("");
}

char *w15_pluginsdis_6f9432_hook_call_identity(json_t *kwargs, const char *hook_name) {
    const char *fields[2];
    size_t nfields;
    if (hook_name && !strcmp(hook_name, "pre_command")) {
        fields[0] = "session_key"; nfields = 1;
    } else if (hook_name && (!strcmp(hook_name, "post_compaction") ||
                             !strcmp(hook_name, "on_session_open"))) {
        fields[0] = "session_id"; nfields = 1;
    } else {
        fields[0] = "tool_call_id"; fields[1] = "turn_id"; nfields = 2;
    }
    for (size_t i = 0; i < nfields; i++) {
        json_t *v = json_obj_get(kwargs, fields[i]);
        if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
            return dupstr(v->str_val); /* copy out before kwargs is freed */
    }
    return NULL;
}

