/*
 * run_status_filter.c — C home for upstream Hermes gateway/run_status_filter.py.
 *
 * PoP: w11_runstatus_8163ea_hidden_outside_dm @ gateway/run_status_filter.py:hidden_outside_dm
 *
 * The DM-only pattern is _template_regex(CONTEXT_OVERFLOW_BLOCKED_WARNING_TEMPLATE),
 * i.e. fullmatch of the emit-site template with tokens/threshold replaced by
 * Python's canonical grouped-number regex and reason by [\s\S]+?.
 */
#include "noc_w11_runstatus_8163ea.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* "⚠" is U+26A0 = E2 9A A0 in UTF-8. */
static const char PREFIX[] = "\xE2\x9A\xA0 Context is over the compression threshold (~";
static const char MID1[]   = " tokens >= ";
static const char MID2[]   = ") but compression is currently blocked (";
static const char SUFFIX[] = "). The model may stop responding. Run /new to start a fresh session or /compress to retry immediately.";

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* (?:0|[1-9][0-9]{0,2}(?:,[0-9]{3})*) -- Python _CANONICAL_COUNT. */
static int canonical_count(const char *s, size_t n) {
    if (n == 0) return 0;
    if (n == 1 && s[0] == '0') return 1;
    if (s[0] < '1' || s[0] > '9') return 0;
    size_t i = 1;
    while (i < n && i < 3 && s[i] >= '0' && s[i] <= '9') ++i; /* [0-9]{0,2} */
    while (i < n) {
        if (s[i] != ',' || i + 3 >= n) return 0;
        for (int j = 1; j <= 3; ++j)
            if (s[i + j] < '0' || s[i + j] > '9') return 0;
        i += 4;
    }
    return i == n;
}

static int match_warning(const char *msg) {
    if (!msg) return 0;
    size_t pl = strlen(PREFIX), m1 = strlen(MID1), m2 = strlen(MID2), sl = strlen(SUFFIX);
    if (strncmp(msg, PREFIX, pl)) return 0;
    const char *p = msg + pl;

    const char *a = p;
    while ((a = strstr(a, MID1))) {
        if (canonical_count(p, (size_t)(a - p))) {
            const char *q = a + m1;
            const char *b = q;
            while ((b = strstr(b, MID2))) {
                if (canonical_count(q, (size_t)(b - q))) {
                    const char *r = b + m2;
                    size_t rl = strlen(r);
                    if (rl > sl && !strcmp(r + rl - sl, SUFFIX)) return 1;
                }
                ++b;
            }
        }
        ++a;
    }
    return 0;
}

/* str(getattr(chat_type,"value",chat_type) or "").strip().lower() */
static char *chat_kind(const struct json_t *chat_type) {
    const struct json_t *v = chat_type;
    if (chat_type && chat_type->type == JSON_OBJECT && json_has(chat_type, "value"))
        v = json_obj_get(chat_type, "value");
    int falsy = 1;
    char raw[64] = "";
    if (v) {
        switch (v->type) {
            case JSON_NULL:   break;
            case JSON_BOOL:   falsy = v->bool_val ? 0 : 1;
                              snprintf(raw, sizeof(raw), "%s", v->bool_val ? "True" : "False"); break;
            case JSON_NUMBER: falsy = v->num_val == 0.0;
                              snprintf(raw, sizeof(raw), "%.15g", v->num_val); break;
            case JSON_STRING: falsy = !(v->str_val && v->str_val[0] != '\0');
                              snprintf(raw, sizeof(raw), "%s", v->str_val ? v->str_val : ""); break;
            case JSON_ARRAY:
            case JSON_OBJECT: falsy = v->c.count == 0;
                              snprintf(raw, sizeof(raw), "%s", ""); break;
            default: break;
        }
    }
    if (falsy) return strdup("");
    /* str.strip(): trim leading and trailing whitespace only, then lower(). */
    const char *s = raw;
    while (*s && is_ws(*s)) ++s;
    size_t n = strlen(s);
    while (n && is_ws(s[n - 1])) --n;
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    for (char *p = out; *p; ++p)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

int w11_runstatus_8163ea_hidden_outside_dm(const struct json_t *chat_type,
                                            const char *message) {
    char *kind = chat_kind(chat_type);
    int dm = (!strcmp(kind, "dm") || !strcmp(kind, "private") ||
              !strcmp(kind, "direct") || !strcmp(kind, ""));
    int res = 0;
    if (!dm) res = match_warning(message) ? 1 : 0;
    free(kind);
    return res;
}
