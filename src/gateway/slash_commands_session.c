/*
 * slash_commands_session.c — real C11 port of upstream Hermes
 *   gateway/slash_commands_session.py pure helpers.
 * PoP: w18_slashcomma_782bf8_strip_resume_name @ gateway/slash_commands_session.py:_strip_resume_name
 * PoP: w18_slashcomma_782bf8_telegram_private_chat_owner @ gateway/slash_commands_session.py:_telegram_private_chat_owner
 */
#include "hermes_json.h"
#include "noc_w18_slashcomma_782bf8.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over ASCII whitespace. Writes NUL at the trimmed length. */
static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && is_ws(*a)) a++;
    const char *b = s + strlen(s);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, a, n); r[n] = '\0'; }
    return r;
}

static int ascii_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

char *w18_slashcomma_782bf8_strip_resume_name(const struct json_t *parts) {
    size_t count = (parts && parts->type == JSON_ARRAY) ? parts->c.count : 0;
    size_t cap = 1, len = 0;
    for (size_t i = 0; i < count; i++) {
        const json_t *it = parts->c.items[i];
        if (!it || it->type != JSON_STRING) continue;
        const char *v = it->str_val ? it->str_val : "";
        if (!strcmp(v, "--all") || !strcmp(v, "--cross-room")) continue;
        cap += strlen(v) + 1;
    }
    char *buf = (char *)malloc(cap);
    if (!buf) return dupstr("");
    buf[0] = '\0';
    int first = 1;
    for (size_t i = 0; i < count; i++) {
        const json_t *it = parts->c.items[i];
        if (!it || it->type != JSON_STRING) continue;
        const char *v = it->str_val ? it->str_val : "";
        if (!strcmp(v, "--all") || !strcmp(v, "--cross-room")) continue;
        if (!first) { buf[len++] = ' '; buf[len] = '\0'; }
        size_t vl = strlen(v);
        memcpy(buf + len, v, vl);
        len += vl; buf[len] = '\0';
        first = 0;
    }
    char *name = py_strip(buf);
    free(buf);
    if (!name) return dupstr("");
    size_t L = strlen(name);
    if (L >= 2) {
        char a = name[0], b = name[L - 1];
        int wrap = (a == '<' && b == '>') || (a == '[' && b == ']') ||
                   (a == '"' && b == '"') || (a == '\'' && b == '\'');
        if (wrap) {
            char *inner = (char *)malloc(L - 1);
            if (inner) {
                memcpy(inner, name + 1, L - 2);
                inner[L - 2] = '\0';
                char *res = py_strip(inner);
                free(inner); free(name);
                return res ? res : dupstr("");
            }
        }
    }
    return name;
}

struct json_t *w18_slashcomma_782bf8_telegram_private_chat_owner(const char *platform,
                                                                const char *chat_type,
                                                                const char *chat_id,
                                                                const char *user_id) {
    json_t *out = json_array();
    const char *pl = platform ? platform : "";
    const char *ct = chat_type ? chat_type : "";
    if (!strcmp(pl, "telegram") && ascii_ieq(ct, "dm")) {
        const char *cid = chat_id ? chat_id : "";
        const char *uid = user_id ? user_id : "";
        if (cid[0] && uid[0]) {
            json_append(out, json_string(cid));
            json_append(out, json_string(uid));
        }
    }
    return out;
}
