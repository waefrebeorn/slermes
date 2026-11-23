/*
 * cache_bounds.c — C home for upstream Hermes agent/cache_bounds.py.
 *
 * PoP: w6_cachebounds_876475_drop_expired @ agent/cache_bounds.py:drop_expired
 * PoP: w6_cachebounds_876475_evict_oldest @ agent/cache_bounds.py:evict_oldest
 * PoP: w6_cachebounds_876475_publish_lru @ agent/cache_bounds.py:publish_lru
 *
 * Thread locks and event-loop identity are runtime concerns and are not
 * reimplemented. The tested behaviour is the pure dict-eviction logic:
 * stamp-based expiry, oldest-stamp eviction across sibling tables and LRU
 * insertion-order eviction, all driven by injected tables so the gate is
 * deterministic.
 */
#include "noc_w6_cachebounds_876475.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* stamp_of: dict values carry {"stamp": number}; a bare number is its own stamp. */
static double stamp_of(const json_t *v)
{
    json_t *s;
    if (v && v->type == JSON_OBJECT) {
        s = json_obj_get(v, "stamp");
        return (s && s->type == JSON_NUMBER) ? s->num_val : 0.0;
    }
    if (v && v->type == JSON_NUMBER) return v->num_val;
    return 0.0;
}

static json_t *copy_tables(const json_t *siblings)
{
    json_t *out = json_array();
    size_t n, i;
    if (siblings && siblings->type == JSON_ARRAY) {
        n = json_len(siblings);
        for (i = 0; i < n; i++) {
            json_t *s = json_get(siblings, i);
            json_append(out, (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object());
        }
    }
    return out;
}

static void del_from_tables(json_t *tables, const char *key)
{
    size_t n = json_len(tables), i;
    for (i = 0; i < n; i++) {
        json_t *t = json_get(tables, i);
        if (t && t->type == JSON_OBJECT) json_obj_del(t, key);
    }
}

struct json_t *w6_cachebounds_876475_drop_expired(const struct json_t *values,
                                                  const struct json_t *stamps,
                                                  double ttl, double now)
{
    json_t *vo = (values && values->type == JSON_OBJECT) ? json_copy(values) : json_object();
    json_t *so = (stamps && stamps->type == JSON_OBJECT) ? json_copy(stamps) : json_object();
    size_t n = json_object_size(vo), i;
    char **keys = (char **)malloc((n ? n : 1) * sizeof(char *));
    json_t *out;

    if (!keys) return json_object();
    for (i = 0; i < n; i++)
        keys[i] = dup_str(json_object_get_key_at(vo, i));
    for (i = 0; i < n; i++) {
        json_t *sv = json_obj_get(so, keys[i]);
        double stamp = (sv && sv->type == JSON_NUMBER) ? sv->num_val : 0.0;
        if (now - stamp >= ttl) {
            json_obj_del(vo, keys[i]);
            json_obj_del(so, keys[i]);
        }
    }
    for (i = 0; i < n; i++) free(keys[i]);
    free(keys);

    out = json_object();
    json_set(out, "values", vo);
    json_set(out, "stamps", so);
    return out;
}

struct json_t *w6_cachebounds_876475_evict_oldest(const struct json_t *cache, const char *key,
                                                  double limit, const struct json_t *siblings)
{
    json_t *c = (cache && cache->type == JSON_OBJECT) ? json_copy(cache) : json_object();
    json_t *sibs = copy_tables(siblings);
    size_t n = json_object_size(c), i;
    int present = (key && json_obj_get(c, key) != NULL);
    const char *oldest = NULL;
    double best = 0.0;
    json_t *out;

    if (!present && !((double)n < limit) && n > 0) {
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(c, i);
            double st = stamp_of(json_object_get_at(c, i));
            if (oldest == NULL || st < best) {
                oldest = k;
                best = st;
            }
        }
        if (oldest) {
            char *tmp = dup_str(oldest);
            json_obj_del(c, tmp);
            del_from_tables(sibs, tmp);
            free(tmp);
        }
    }
    out = json_object();
    json_set(out, "cache", c);
    json_set(out, "siblings", sibs);
    return out;
}

struct json_t *w6_cachebounds_876475_publish_lru(const struct json_t *table, const char *key,
                                                 const struct json_t *value, double limit,
                                                 const struct json_t *siblings)
{
    json_t *t = (table && table->type == JSON_OBJECT) ? json_copy(table) : json_object();
    json_t *sibs = copy_tables(siblings);
    json_t *evicted = json_array();
    const char *kname = key ? key : "";
    json_t *out;

    json_obj_del(t, kname);
    json_set(t, kname, value ? json_copy(value) : json_null());

    while ((double)json_object_size(t) > limit) {
        const char *stale = json_object_get_key_at(t, 0);
        char *tmp;
        if (!stale) break;
        tmp = dup_str(stale);
        json_append(evicted, json_string(tmp));
        json_obj_del(t, tmp);
        del_from_tables(sibs, tmp);
        free(tmp);
    }
    out = json_object();
    json_set(out, "evicted", evicted);
    json_set(out, "table", t);
    json_set(out, "siblings", sibs);
    return out;
}
