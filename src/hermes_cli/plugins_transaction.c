/*
 * plugins_transaction.c -- faithful C11 port of the non-interactive refusal
 * message in hermes_cli/plugins_transaction.py
 * (wave18 gate w18_pluginstra_7bc196).
 *
 * PoP: w18_pluginstra_7bc196_update_deps_message @ hermes_cli/plugins_transaction.py:_refresh_declared_dependencies
 *
 * The filesystem / PM admission half stays in Python. Only the pure
 * "added specs" computation and the refusal text are ported.
 */
#include "noc_w18_pluginstra_7bc196.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

json_t *w18_pluginstra_7bc196_update_deps_message(json_t *before_reqs, json_t *after_reqs,
                                                  bool before_installs, bool after_installs,
                                                  const char *target_name)
{
    size_t n = json_is_array(after_reqs) ? json_len(after_reqs) : 0;
    size_t bn = json_is_array(before_reqs) ? json_len(before_reqs) : 0;
    char **parts = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!parts) return json_null();
    size_t cnt = 0;

    /* added = tuple(spec for spec in after if spec not in before) */
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(after_reqs, i);
        const char *s = (v && json_is_string(v) && v->str_val) ? v->str_val : NULL;
        if (!s) continue;
        int found = 0;
        for (size_t j = 0; j < bn; j++) {
            json_t *b = json_get(before_reqs, j);
            if (b && json_is_string(b) && b->str_val && strcmp(b->str_val, s) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char *d = dupstr(s);                    /* copy out of the json node */
            if (d) parts[cnt++] = d;
        }
    }

    bool transition = after_installs && !before_installs;
    json_t *out;
    if (cnt == 0 && !transition) {
        out = json_null();
        goto done;
    }

    static const char PREFIX[] = "The update declares new Python dependencies (";
    static const char MID[] = "); run `hermes plugins update ";
    static const char SUFFIX[] = "` in a terminal to review them.";
    static const char FALLBACK[] = "in its pyproject.toml";
    const char *inner = NULL;
    size_t inner_len = 0;
    char *joined = NULL;
    if (cnt > 0) {
        for (size_t i = 0; i < cnt; i++) inner_len += strlen(parts[i]);
        inner_len += (cnt - 1) * 2;                 /* ", " separators */
        joined = (char *)malloc(inner_len + 1);
        if (!joined) { out = json_null(); goto done; }
        char *p = joined;
        for (size_t i = 0; i < cnt; i++) {
            if (i) { memcpy(p, ", ", 2); p += 2; }
            size_t l = strlen(parts[i]);
            memcpy(p, parts[i], l);
            p += l;
        }
        *p = '\0';                                  /* NUL at new length */
        inner = joined;
    } else {
        inner = FALLBACK;
        inner_len = strlen(FALLBACK);
    }

    const char *tn = target_name ? target_name : "";
    size_t total = strlen(PREFIX) + inner_len + strlen(MID)
                 + strlen(tn) + strlen(SUFFIX) + 1;
    char *buf = (char *)malloc(total);
    if (!buf) { free(joined); out = json_null(); goto done; }
    char *p = buf;
    memcpy(p, PREFIX, strlen(PREFIX)); p += strlen(PREFIX);
    memcpy(p, inner, inner_len); p += inner_len;
    memcpy(p, MID, strlen(MID)); p += strlen(MID);
    size_t tl = strlen(tn);
    memcpy(p, tn, tl); p += tl;
    memcpy(p, SUFFIX, strlen(SUFFIX)); p += strlen(SUFFIX);
    *p = '\0';
    out = json_string(buf);
    free(buf);
    free(joined);

done:
    for (size_t i = 0; i < cnt; i++) free(parts[i]);
    free(parts);
    return out;
}
