/*
 * session_detached.c — C home for upstream Hermes gateway/session_detached.py.
 *
 * PoP: w10_sessiondet_9727db_detached_key @ gateway/session_detached.py:detached_key
 * PoP: w10_sessiondet_9727db_detached_session_id @ gateway/session_detached.py:detached_session_id
 * PoP: w10_sessiondet_9727db_own_detached_key @ gateway/session_detached.py:own_detached_key
 * PoP: w10_sessiondet_9727db_detached_route_of @ gateway/session_detached.py:detached_route_of
 * PoP: w10_sessiondet_9727db_running_key @ gateway/session_detached.py:running_key
 *
 * Dependency-free string helpers of the detached-lane protocol.  All splitting
 * is on ':' exactly as Python str.split(':'); a missing/foreign namespace falls
 * back to 'agent:main'.
 */
#include "noc_w10_sessiondet_9727db.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ':'.join(parts[:2]) when parts[0]=='agent' and len(parts)>1, else 'agent:main'. */
static char *route_namespace(const char *route) {
    if (!route) route = "";
    const char *c1 = strchr(route, ':');
    if (!c1 || (size_t)(c1 - route) != 5 || strncmp(route, "agent", 5) != 0)
        return xstrdup("agent:main");
    const char *c2 = strchr(c1 + 1, ':');
    if (!c2) return xstrdup("agent:main");
    size_t n = (size_t)(c2 - route);
    char *out = (char *)malloc(n + 1);
    if (!out) return xstrdup("agent:main");
    memcpy(out, route, n);
    out[n] = '\0';
    return out;
}

struct json_t *w10_sessiondet_9727db_detached_key(const char *route, const char *session_id) {
    char *ns = route_namespace(route);
    if (!session_id) session_id = "";
    size_t n = strlen(ns) + strlen(":detached:") + strlen(session_id) + 1;
    char *buf = (char *)malloc(n);
    if (!buf) { free(ns); return json_string(""); }
    snprintf(buf, n, "%s:detached:%s", ns, session_id);
    free(ns);
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

struct json_t *w10_sessiondet_9727db_detached_session_id(const char *key) {
    if (!key) return json_null();
    const char *c1 = strchr(key, ':');
    if (!c1) return json_null();
    const char *c2 = strchr(c1 + 1, ':');
    if (!c2) return json_null();
    const char *c3 = strchr(c2 + 1, ':');
    if (!c3) return json_null();
    if (strchr(c3 + 1, ':') != NULL) return json_null();           /* len(parts) != 4 */
    if ((size_t)(c1 - key) != 5 || strncmp(key, "agent", 5) != 0) return json_null();
    if ((size_t)(c3 - c2 - 1) != 8 || strncmp(c2 + 1, "detached", 8) != 0) return json_null();
    return json_string(c3 + 1);
}

struct json_t *w10_sessiondet_9727db_own_detached_key(const char *key, const char *session_id) {
    json_t *sid = w10_sessiondet_9727db_detached_session_id(key);
    int ok = 0;
    if (sid && sid->type == JSON_STRING && session_id)
        ok = (strcmp(sid->str_val, session_id) == 0);
    json_free(sid);
    return json_bool(ok != 0);
}

struct json_t *w10_sessiondet_9727db_detached_route_of(const struct json_t *source) {
    const json_t *v = (source && source->type == JSON_OBJECT) ? json_obj_get(source, "detached_route") : NULL;
    if (v && v->type == JSON_STRING) return json_string(v->str_val);
    return json_null();
}

struct json_t *w10_sessiondet_9727db_running_key(const struct json_t *running, const char *route, const char *session_id) {
    json_t *key = w10_sessiondet_9727db_detached_key(route, session_id);
    const char *kval = (key && key->type == JSON_STRING) ? key->str_val : "";
    int found = 0;
    if (running && running->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(running); i++) {
            const json_t *it = json_get(running, i);
            if (it && it->type == JSON_STRING && strcmp(it->str_val, kval) == 0) { found = 1; break; }
        }
    }
    json_free(key);
    if (found) return json_string(kval);
    return json_string(route ? route : "");
}
