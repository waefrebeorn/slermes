/*
 * boot_bootstrap.c — C port of upstream Hermes hermes_cli/boot_bootstrap.py.
 *
 * PoP: w12_bootbootst_5d9f10_owed_config_migration @ hermes_cli/boot_bootstrap.py:owed_config_migration
 * PoP: w12_bootbootst_5d9f10_owed_steps @ hermes_cli/boot_bootstrap.py:_owed_steps
 * PoP: w12_bootbootst_5d9f10_read_last_known @ hermes_cli/boot_bootstrap.py:read_last_known
 * PoP: w12_bootbootst_5d9f10_current_install_identity @ hermes_cli/boot_bootstrap.py:current_install_identity
 * PoP: w12_bootbootst_5d9f10_needs_bootstrap @ hermes_cli/boot_bootstrap.py:needs_bootstrap
 *
 * Disk/git/environment reads are injected from the fixture; the decision logic
 * is mirrored exactly, including the duplicate "migrate_config" append.
 */
#include "noc_w12_bootbootst_5d9f10.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

/* (known.get("results") or {}).get("migrate_config") or {} */
static json_t *migration(const json_t *known) {
    json_t *results = jget(known, "results");
    if (!jtruthy(results)) return NULL;
    json_t *m = jget(results, "migrate_config");
    if (!jtruthy(m)) return NULL;
    return m;
}

struct json_t *w12_bootbootst_5d9f10_owed_config_migration(const struct json_t *known) {
    json_t *m = migration(known);
    if (!m || !jtruthy(jget(m, "failed_step"))) return json_null();
    return json_copy(m);
}

struct json_t *w12_bootbootst_5d9f10_owed_steps(const struct json_t *known) {
    json_t *out = json_array();
    json_t *results = jget(known, "results");
    if (jtruthy(results) && results->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(results); i++) {
            const char *key = json_object_get_key_at(results, i);
            json_t *val = json_object_get_at(results, i);
            if (val && val->type == JSON_OBJECT && jtruthy(jget(val, "owed")))
                json_append(out, json_string(key ? key : ""));
        }
    }
    json_t *m = migration(known);
    if (m && jtruthy(jget(m, "failed_step")))
        json_append(out, json_string("migrate_config"));
    return out;
}

struct json_t *w12_bootbootst_5d9f10_read_last_known(const char *text) {
    static const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    if (p[0] == bom[0] && p[1] == bom[1] && p[2] == bom[2]) p += 3;
    json_t *data = json_parse((const char *)p, NULL);
    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        return json_object();
    }
    return data;
}

struct json_t *w12_bootbootst_5d9f10_current_install_identity(int has_git, const struct json_t *git_head,
                                                              const struct json_t *stamp) {
    if (has_git) {
        if (!git_head || git_head->type != JSON_STRING || !git_head->str_val ||
            strlen(git_head->str_val) < 7)
            return json_null();
        return json_copy(git_head);
    }
    json_t *commit = jget(stamp, "commit");
    if (commit && commit->type == JSON_STRING && commit->str_val && strlen(commit->str_val) >= 7)
        return json_copy(commit);
    json_t *tag = jget(stamp, "tag");
    if (tag && tag->type == JSON_STRING && tag->str_val && *tag->str_val)
        return json_copy(tag);
    return json_null();
}

struct json_t *w12_bootbootst_5d9f10_needs_bootstrap(const struct json_t *identity,
                                                     const struct json_t *known) {
    if (!identity || identity->type != JSON_STRING || !identity->str_val || !*identity->str_val)
        return json_null();
    json_t *known_id = jget(known, "identity");
    if (known_id && known_id->type == JSON_STRING && identity->str_val &&
        !strcmp(known_id->str_val, identity->str_val))
        return json_null();
    return json_copy(identity);
}
