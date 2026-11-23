/*
 * run_inbound_media.c — C home for upstream Hermes gateway/run_inbound_media.py.
 *
 * PoP: w11_runinbound_aaf63b_rehome @ gateway/run_inbound_media.py:rehome_inbound_media
 *
 * The filesystem move is assumed to succeed (fixtures mark sources as existing)
 * and the hermes-home / agent-visible-path lookups are injected, so the string
 * decisions of the live function are reproduced exactly.
 */
#include "noc_w11_runinbound_aaf63b.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* base.rstrip("/") + "/" + tail.lstrip("/") -- pathlib PurePath join. */
static char *path_join(const char *base, const char *tail) {
    size_t bl = strlen(base);
    while (bl && base[bl - 1] == '/') --bl;
    const char *t = tail;
    while (*t == '/') ++t;
    size_t tl = strlen(t);
    char *o = (char *)malloc(bl + 1 + tl + 1);
    memcpy(o, base, bl);
    o[bl] = '/';
    memcpy(o + bl + 1, t, tl);
    o[bl + 1 + tl] = '\0';
    return o;
}

/* Path(src).relative_to(base); NULL when it would raise ValueError. */
static char *relative_to(const char *src, const char *base) {
    size_t bl = strlen(base);
    while (bl && base[bl - 1] == '/') --bl;
    size_t sl = strlen(src);
    if (sl == bl && !strncmp(src, base, bl)) return strdup("");
    if (sl > bl && !strncmp(src, base, bl) && src[bl] == '/')
        return strdup(src + bl + 1);
    return NULL;
}

static const char *map_lookup(const struct json_t *m, const char *key) {
    if (m && m->type == JSON_OBJECT) {
        const char *v = json_get_str(m, key, NULL);
        if (v) return v;
    }
    return key;
}

/* str.replace(old, new), all occurrences, single pass. */
static char *replace_all(const char *s, const char *old, const char *newv) {
    if (!old || !old[0]) return strdup(s);
    size_t ol = strlen(old), nl = strlen(newv);
    size_t cap = strlen(s) + 1, n = 0;
    char *o = (char *)malloc(cap);
    const char *p = s;
    const char *hit;
    while ((hit = strstr(p, old))) {
        size_t pre = (size_t)(hit - p);
        if (n + pre + nl + 1 > cap) {
            cap = (n + pre + nl + 1) * 2;
            o = (char *)realloc(o, cap);
        }
        memcpy(o + n, p, pre);
        n += pre;
        memcpy(o + n, newv, nl);
        n += nl;
        p = hit + ol;
    }
    size_t tail = strlen(p);
    if (n + tail + 1 > cap) {
        cap = n + tail + 1;
        o = (char *)realloc(o, cap);
    }
    memcpy(o + n, p, tail);
    n += tail;
    o[n] = '\0';
    return o;
}

struct json_t *w11_runinbound_aaf63b_rehome(const struct json_t *event,
                                             const char *active_home,
                                             const char *launch_home,
                                             const struct json_t *agent_map) {
    json_t *out = json_object();
    json_t *new_urls = json_array();

    const char *a = active_home ? active_home : "";
    const char *l = launch_home ? launch_home : "";
    int noop = (strcmp(a, l) == 0); /* hermes_home_key(active) == hermes_home_key(launch) */

    json_t *textv = json_obj_get(event, "text");
    json_t *pendv = json_obj_get(event, "_gateway_pending_stt_text");
    char *text = (textv && textv->type == JSON_STRING) ? strdup(textv->str_val) : NULL;
    char *pend = (pendv && pendv->type == JSON_STRING) ? strdup(pendv->str_val) : NULL;

    const json_t *urls = json_obj_get(event, "media_urls");
    size_t n = (urls && urls->type == JSON_ARRAY) ? json_len(urls) : 0;
    for (size_t i = 0; i < n; ++i) {
        const json_t *u = json_get(urls, i);
        const char *raw = (u && u->type == JSON_STRING) ? u->str_val : NULL;
        if (!raw || noop) { json_append(new_urls, u ? json_copy(u) : json_null()); continue; }

        char *lcache = path_join(l, "cache");
        char *rel = relative_to(raw, lcache);
        free(lcache);
        if (!rel) { json_append(new_urls, json_copy(u)); continue; }

        char *acache = path_join(a, "cache");
        char *dest = path_join(acache, rel);
        free(acache);
        free(rel);

        json_append(new_urls, json_string(dest));
        const char *agent_path = map_lookup(agent_map, dest);
        if (text && strstr(text, raw)) {
            char *nt = replace_all(text, raw, agent_path);
            free(text);
            text = nt;
        }
        const char *old_agent_path = map_lookup(agent_map, raw);
        if (pend && strstr(pend, old_agent_path)) {
            char *np = replace_all(pend, old_agent_path, agent_path);
            free(pend);
            pend = np;
        }
        free(dest);
    }

    json_set(out, "media_urls", new_urls);
    json_set(out, "text", text ? json_string(text) : json_null());
    json_set(out, "_gateway_pending_stt_text", pend ? json_string(pend) : json_null());
    free(text);
    free(pend);
    return out;
}
