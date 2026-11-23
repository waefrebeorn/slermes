/*
 * send_targets.c — faithful C11 port of upstream Hermes
 * plugins/platforms/slack/send_targets.py (gate sltarget).
 * PoP: sltarget_parse_send_target @ plugins/platforms/slack/send_targets.py:_parse_send_target
 */
#include "noc_sltarget.h"
#include <stdlib.h>
#include <string.h>

static int slt_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int slt_upper_alnum(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
static int slt_name_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}
static char *slt_dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *slt_cat2(const char *prefix, const char *s, size_t n) {
    size_t pl = strlen(prefix);
    char *p = (char *)malloc(pl + n + 1);
    if (!p) return NULL;
    memcpy(p, prefix, pl);
    memcpy(p + pl, s, n);
    p[pl + n] = '\0';
    return p;
}

int sltarget_parse_send_target(const char *ref, char **chat, char **thread) {
    const char *s;
    const char *e;
    size_t len, j, k, id_len;
    char c0;

    *chat = *thread = NULL;
    if (!ref) return 0;
    s = ref;
    while (*s && slt_is_ws((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && slt_is_ws((unsigned char)*(e - 1))) e--;
    len = (size_t)(e - s);
    if (len == 0) return 0;
    c0 = s[0];

    /* Forms 1 & 2: [CGD][A-Z0-9]{8,}  (optionally :<thread>) */
    if (c0 == 'C' || c0 == 'G' || c0 == 'D') {
        j = 1;
        while (j < len && slt_upper_alnum(s[j])) j++;
        id_len = j;                        /* first char is in [A-Z0-9] too */
        if (id_len < 9) return 0;
        if (j == len) {                    /* _SEND_TARGET_RE: bare id */
            *chat = slt_dup_n(s, j);
            return *chat ? 1 : 0;
        }
        if (s[j] == ':') {                 /* _SEND_THREAD_TARGET_RE */
            size_t t_start = j + 1;
            if (t_start >= len) return 0;  /* [^\s:]+ needs >= 1 char */
            for (k = t_start; k < len; k++) {
                if (s[k] == ':' || slt_is_ws((unsigned char)s[k])) return 0;
            }
            *chat = slt_dup_n(s, j);
            *thread = slt_dup_n(s + t_start, len - t_start);
            if (!*chat || !*thread) { free(*chat); free(*thread); *chat = *thread = NULL; return 0; }
            return 1;
        }
        return 0;
    }

    /* Form 3: U[A-Z0-9]{8,} -> user:<id> */
    if (c0 == 'U') {
        j = 1;
        while (j < len && slt_upper_alnum(s[j])) j++;
        if (j != len || j < 9) return 0;
        *chat = slt_cat2("user:", s, j);
        return *chat ? 1 : 0;
    }

    /* Form 4: <@U[A-Z0-9]{8,}(?:\|[^>]+)?> -> user:<id> */
    if (c0 == '<' && len >= 3 && s[1] == '@' && s[2] == 'U') {
        j = 3;
        while (j < len && slt_upper_alnum(s[j])) j++;
        if (j - 2 < 9) return 0;           /* U + >=8 alnum */
        if (j == len) return 0;            /* no closing '>' */
        if (s[j] == '>') {
            if (j != len - 1) return 0;    /* fullmatch: '>' must be last */
            *chat = slt_cat2("user:", s + 2, j - 2);
            return *chat ? 1 : 0;
        }
        if (s[j] == '|') {
            size_t label_start = j + 1;
            k = label_start;
            while (k < len && s[k] != '>') k++;
            if (k == len || k == label_start) return 0;  /* [^>]+ needs >= 1 */
            if (k != len - 1) return 0;
            *chat = slt_cat2("user:", s + 2, j - 2);
            return *chat ? 1 : 0;
        }
        return 0;
    }

    /* Form 5: @[A-Za-z0-9._-]{1,80} -> user_name:<name> */
    if (c0 == '@') {
        j = 1;
        while (j < len && slt_name_char(s[j])) j++;
        if (j == len && j - 1 >= 1 && j - 1 <= 80) {
            *chat = slt_cat2("user_name:", s + 1, j - 1);
            return *chat ? 1 : 0;
        }
        return 0;
    }

    return 0;
}
