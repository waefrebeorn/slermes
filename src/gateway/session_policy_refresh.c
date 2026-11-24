/*
 * session_policy_refresh.c — C home for upstream Hermes
 * gateway/session_policy_refresh.py.
 *
 * PoP: w18_sessionpol_e8bda4_follow @ gateway/session_policy_refresh.py:_follow
 * PoP: w18_sessionpol_e8bda4_server_name @ gateway/session_policy_refresh.py:_server_name
 */
#include "noc_w18_sessionpol_e8bda4.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    char *p;
    size_t n;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

struct json_t *w18_sessionpol_e8bda4_follow(
    const struct json_t *target, const struct json_t *live, const char *key) {
    json_t *out = json_object();
    json_t *lv;

    if (!key) key = "";
    if (target && target->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < json_object_size(target); ++i) {
            const char *k = json_object_get_key_at(target, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(target, i)));
        }
    }
    lv = (live && live->type == JSON_OBJECT) ? json_obj_get(live, key) : NULL;
    if (lv) json_set(out, key, json_copy(lv));
    else json_object_del(out, key);
    return out;
}

char *w18_sessionpol_e8bda4_server_name(
    const struct json_t *servers, const char *name) {
    if (!name) return NULL;
    if (!strncmp(name, "mcp-", 4)) {
        const char *bare = name + 4;
        size_t i, n = 0;
        if (servers && servers->type == JSON_ARRAY) n = json_len(servers);
        for (i = 0; i < n; ++i) {
            json_t *s = json_get(servers, i);
            if (s && s->type == JSON_STRING && s->str_val && !strcmp(s->str_val, name)) {
                return dupstr(name); /* name itself is a server */
            }
        }
        return dupstr(bare);
    }
    return dupstr(name);
}
