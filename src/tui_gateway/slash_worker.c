/*
 * slash_worker.c — C home for upstream Hermes tui_gateway/slash_worker.py.
 *
 * PoP: w11_slashworke_1620bb_slash_base @ tui_gateway/slash_worker.py:_slash_base
 * PoP: w11_slashworke_1620bb_parse_line @ tui_gateway/slash_worker.py:main
 * PoP: w11_slashworke_1620bb_reply     @ tui_gateway/slash_worker.py:_reply
 * PoP: w11_slashworke_1620bb_skill_refused_message @ tui_gateway/slash_worker.py:SkillSlashRefused
 *
 * Pure protocol helpers of the persistent slash-command worker. The heavy
 * HermesCLI process loop is out of scope; the JSON-line protocol and command
 * normalization are ported verbatim.
 */
#include "noc_w11_slashworke_1620bb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char *sw_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *sw_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

char *w11_slashworke_1620bb_slash_base(const char *command) {
    const char *p = command ? command : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    char *cmd = sw_dupn(p, (size_t)(e - p));       /* (command or "").strip() */
    if (cmd[0] == '/') {                           /* .removeprefix("/") */
        size_t n = strlen(cmd);
        memmove(cmd, cmd + 1, n);                  /* includes terminating NUL */
    }
    const char *q = cmd;
    while (*q && isspace((unsigned char)*q)) q++;  /* split(maxsplit=1) skips ws */
    const char *t = q;
    while (*t && !isspace((unsigned char)*t)) t++;
    char *tok = sw_dupn(q, (size_t)(t - q));
    for (char *c = tok; *c; c++) *c = (char)tolower((unsigned char)*c);
    free(cmd);
    return tok;
}

json_t *w11_slashworke_1620bb_parse_line(const char *raw) {
    const char *a = raw ? raw : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    if (a == b) return json_null();                /* blank line: skip */
    char *line = sw_dupn(a, (size_t)(b - a));
    json_t *req = json_parse(line, NULL);
    free(line);
    if (!req) return json_null();                  /* malformed: no frame */
    json_t *out = json_object();
    json_t *id = json_obj_get(req, "id");
    json_set(out, "id", id ? json_copy(id) : json_null());
    json_t *cmd = json_obj_get(req, "command");
    json_set(out, "command", cmd ? json_copy(cmd) : json_string(""));
    json_free(req);
    return out;
}

char *w11_slashworke_1620bb_skill_refused_message(const char *base) {
    size_t n = strlen("skill command refused before process: /") + strlen(base ? base : "") + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "skill command refused before process: /%s", base ? base : "");
    return out;
}

/* ---- Python json.dumps-compatible serializer (default separators) ---- */
typedef struct { char *buf; size_t cap, len; } sw_sb;

static void sw_putc(sw_sb *s, char c) {
    if (s->len + 1 >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 128;
        char *nb = (char *)realloc(s->buf, s->cap);
        if (!nb) return;
        s->buf = nb;
    }
    s->buf[s->len++] = c;
}
static void sw_puts(sw_sb *s, const char *v) { while (*v) sw_putc(s, *v++); }

static void sw_ser(sw_sb *s, const json_t *n) {
    if (!n) { sw_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_NULL:   sw_puts(s, "null"); break;
        case JSON_BOOL:   sw_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char b[64];
            if (n->num_val == (double)(long long)n->num_val)
                snprintf(b, sizeof(b), "%.0f", n->num_val);
            else snprintf(b, sizeof(b), "%.17g", n->num_val);
            sw_puts(s, b);
            break;
        }
        case JSON_STRING: {
            char *q = json_escape_string(n->str_val ? n->str_val : "");
            sw_puts(s, q ? q : "\"\"");
            free(q);
            break;
        }
        case JSON_ARRAY:
            sw_putc(s, '[');
            for (size_t i = 0; i < json_len(n); i++) {
                if (i) sw_puts(s, ", ");
                sw_ser(s, json_get(n, i));
            }
            sw_putc(s, ']');
            break;
        case JSON_OBJECT:
            sw_putc(s, '{');
            for (size_t i = 0; i < json_object_size(n); i++) {
                if (i) sw_puts(s, ", ");
                char *q = json_escape_string(json_object_get_key_at(n, i));
                sw_puts(s, q ? q : "\"\"");
                free(q);
                sw_puts(s, ": ");
                sw_ser(s, json_object_get_at(n, i));
            }
            sw_putc(s, '}');
            break;
    }
}

char *w11_slashworke_1620bb_reply(json_t *fields) {
    sw_sb s = {NULL, 0, 0};
    sw_ser(&s, fields);
    sw_putc(&s, '\n');
    sw_putc(&s, '\0');
    return s.buf;
}
