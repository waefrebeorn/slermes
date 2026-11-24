/*
 * local_lineage.c -- real C11 port of the decisions of
 * persistence/sqlite/conversation/_store/local_lineage.py.
 *
 * PoP: w22_locallinea_3bb0fd_validate @ persistence/sqlite/conversation/_store/local_lineage.py:validate_local_lineage
 * PoP: w22_locallinea_3bb0fd_routed_entry @ persistence/sqlite/conversation/_store/local_lineage.py:_routed_entry
 * PoP: w22_locallinea_3bb0fd_advance @ persistence/sqlite/conversation/_store/local_lineage.py:advance_local_target
 * PoP: w22_locallinea_3bb0fd_policy_prefix @ persistence/sqlite/conversation/_store/local_lineage.py:POLICY_PREFIX
 * PoP: w22_locallinea_3bb0fd_legacy_prefix @ persistence/sqlite/conversation/_store/local_migration.py:LEGACY_PREFIX
 *
 * The sqlite connection is the seam: every entry point reads a `db` document
 * with the rows the source's SQL selects (see noc_w22_locallinea_3bb0fd.h).
 * `local_session_id()` and `legacy_lineage_root()` are injected for the same
 * reason -- they live in other modules -- and _commit_local_target's three
 * UPDATEs are reported as the commit result instead of being executed.
 */
#include "hermes_json.h"
#include "noc_w22_locallinea_3bb0fd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LL_POLICY_PREFIX "gateway.local_policy.v1:"
#define LL_LEGACY_PREFIX "gateway.local_legacy.v1:"

const char *w22_locallinea_3bb0fd_policy_prefix(void) { return LL_POLICY_PREFIX; }
const char *w22_locallinea_3bb0fd_legacy_prefix(void) { return LL_LEGACY_PREFIX; }

/* --- helpers ------------------------------------------------------------- */

static int ll_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* Python == over decoded JSON values; NULL is "no such value". */
static int ll_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++) if (!ll_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(a, i);
            if (!key) continue;
            if (!ll_eq(json_object_get_at(a, i), json_obj_get(b, key))) return 0;
        }
        return 1;
    }
    return 0;
}

static int ll_eq_str(const json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING || !s) return 0;
    return !strcmp(v->str_val ? v->str_val : "", s);
}

static json_t *ll_doc_get(const json_t *db, const char *key) {
    if (!db || db->type != JSON_OBJECT) return NULL;
    return json_obj_get(db, key);
}

/* session id -> its row (the row the SQL would have selected). */
static const json_t *ll_session(const json_t *db, const json_t *id) {
    json_t *sessions = ll_doc_get(db, "sessions");
    size_t i;
    if (!id || !sessions || sessions->type != JSON_ARRAY) return NULL;
    for (i = 0; i < json_len(sessions); i++) {
        json_t *row = json_get(sessions, i);
        if (row && row->type == JSON_OBJECT && ll_eq(json_obj_get(row, "id"), id)) return row;
    }
    return NULL;
}

/* _CHAIN_STEP_SQL(parent_id) -> the child id, or NULL when fetchone() is None. */
static const json_t *ll_chain(const json_t *db, const json_t *id) {
    json_t *chain = ll_doc_get(db, "chain");
    const json_t *child;
    if (!id || id->type != JSON_STRING || !chain || chain->type != JSON_OBJECT) return NULL;
    child = json_obj_get(chain, id->str_val);
    if (!child || child->type == JSON_NULL) return NULL;
    return child;
}

/* routing row -> routing entry. *error set to a Python exception name on raise. */
static json_t *ll_routing_entry(const json_t *db, const json_t *route, const char **error) {
    json_t *routing = ll_doc_get(db, "routing");
    json_t *text, *parsed;
    if (!route || route->type != JSON_STRING) { *error = "TypeError"; return NULL; }
    if (!routing || routing->type != JSON_OBJECT) return NULL;
    text = json_obj_get(routing, route->str_val);
    if (!text) return NULL;
    if (text->type != JSON_STRING) { *error = "TypeError"; return NULL; }
    parsed = json_parse(text->str_val, NULL);
    if (!parsed) { *error = "JSONDecodeError"; return NULL; }
    return parsed;
}

/* --- result shapes ------------------------------------------------------- */

static json_t *ll_raised(const char *type) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    return out;
}

static json_t *ll_refused(const char *reason) {
    json_t *out = json_object();
    json_set(out, "status", json_string("refused"));
    json_set(out, "reason", json_string(reason ? reason : "storage_unavailable"));
    return out;
}

/* --- validate_local_lineage --------------------------------------------- */

json_t *w22_locallinea_3bb0fd_validate(const json_t *receipt, const char *local_id,
                                       const char *root, const json_t *db) {
    json_t *lineage, *owned = NULL, *entry, *entry_sid;
    size_t i, j, n;

    if (!receipt || receipt->type != JSON_OBJECT) return ll_raised("TypeError");

    /* `('legacy_session_id' not in receipt and receipt['session_id'] !=
     *  local_session_id(...))` -> storage_unavailable */
    if (!json_obj_get(receipt, "legacy_session_id")) {
        json_t *sid = json_obj_get(receipt, "session_id");
        if (!sid) return ll_raised("KeyError");
        if (!ll_eq_str(sid, local_id)) return ll_refused("storage_unavailable");
    }

    /* `receipt.get('lineage', [root])` */
    lineage = json_obj_get(receipt, "lineage");
    if (!lineage) {
        owned = json_array();
        json_append(owned, json_string(root ? root : ""));
        lineage = owned;
    }
    if (lineage->type != JSON_ARRAY || json_len(lineage) == 0) {
        json_free(owned);
        return ll_refused("storage_unavailable");
    }
    n = json_len(lineage);

    /* len(set(lineage)) != len(lineage): unhashable -> TypeError, dup -> refuse */
    for (i = 0; i < n; i++) {
        json_t *v = json_get(lineage, i);
        if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT)) {
            json_free(owned);
            return ll_raised("TypeError");
        }
        for (j = 0; j < i; j++) {
            if (ll_eq(v, json_get(lineage, j))) {
                json_free(owned);
                return ll_refused("storage_unavailable");
            }
        }
    }
    if (!ll_eq_str(json_get(lineage, 0), root)) {
        json_free(owned);
        return ll_refused("storage_unavailable");
    }
    entry = json_obj_get(receipt, "entry");
    if (!entry) { json_free(owned); return ll_raised("KeyError"); }
    if (entry->type != JSON_OBJECT) { json_free(owned); return ll_raised("TypeError"); }
    entry_sid = json_obj_get(entry, "session_id");
    if (!entry_sid) { json_free(owned); return ll_raised("KeyError"); }
    if (!ll_eq(json_get(lineage, n - 1), entry_sid)) {
        json_free(owned);
        return ll_refused("storage_unavailable");
    }

    /* itertools.pairwise(lineage): each step must be parent -> child. */
    for (i = 1; i < n; i++) {
        json_t *parent_id = json_get(lineage, i - 1);
        json_t *child_id = json_get(lineage, i);
        const json_t *parent = ll_session(db, parent_id);
        const json_t *child = ll_session(db, child_id);
        json_t *end_reason;
        int valid = 0;

        if (!parent || !child) { json_free(owned); return ll_refused("storage_unavailable"); }
        if (!ll_eq(json_obj_get(child, "parent_session_id"), parent_id)) {
            json_free(owned);
            return ll_refused("storage_unavailable");
        }
        end_reason = json_obj_get(parent, "end_reason");
        if (ll_eq_str(end_reason, "session_reset")) {
            json_t *cfg = json_obj_get(child, "model_config");
            json_t *parsed;
            const char *text = "{}";
            if (cfg && ll_truthy(cfg)) {
                if (cfg->type != JSON_STRING) { json_free(owned); return ll_raised("TypeError"); }
                text = (cfg->str_val && cfg->str_val[0]) ? cfg->str_val : "{}";
            }
            parsed = json_parse(text, NULL);
            if (!parsed) { json_free(owned); return ll_raised("JSONDecodeError"); }
            if (parsed->type != JSON_OBJECT) {
                json_free(parsed);
                json_free(owned);
                return ll_raised("AttributeError");
            }
            valid = ll_eq(json_obj_get(parsed, "_reset_from"), parent_id);
            json_free(parsed);
        } else {
            const json_t *tip = ll_chain(db, parent_id);
            valid = ll_eq_str(end_reason, "compression") && tip != NULL && ll_eq(tip, child_id);
        }
        if (!valid) { json_free(owned); return ll_refused("storage_unavailable"); }
    }

    /* A chain tip must not continue past the last lineage member. */
    if (ll_chain(db, json_get(lineage, n - 1))) {
        json_free(owned);
        return ll_refused("storage_unavailable");
    }

    {
        json_t *out = json_object();
        json_set(out, "status", json_string("ok"));
        json_set(out, "tip", json_copy(json_get(lineage, n - 1)));
        json_free(owned);
        return out;
    }
}

/* --- _routed_entry ------------------------------------------------------ */

json_t *w22_locallinea_3bb0fd_routed_entry(const char *route, const char *tip,
                                           const json_t *db) {
    const char *error = NULL;
    json_t *route_v = json_string(route ? route : "");
    json_t *tip_v = json_string(tip ? tip : "");
    json_t *routed = ll_routing_entry(db, route_v, &error);
    json_t *out;

    json_free(route_v);
    if (error) { json_free(tip_v); json_free(routed); return ll_raised(error); }

    out = json_object();
    json_set(out, "status", json_string("ok"));
    if (routed && routed->type != JSON_OBJECT) {
        /* `routed['session_id']` on a non-mapping: TypeError */
        json_free(out);
        json_free(routed);
        json_free(tip_v);
        return ll_raised("TypeError");
    }
    if (routed) {
        json_t *sid = json_obj_get(routed, "session_id");
        if (!sid) {
            json_free(out);
            json_free(routed);
            json_free(tip_v);
            return ll_raised("KeyError");
        }
        if (!ll_eq(sid, tip_v)) {
            json_free(routed);
            routed = NULL;
        }
    }
    json_set(out, "value", routed ? routed : json_null());
    json_free(tip_v);
    return out;
}

/* --- advance_local_target ---------------------------------------------- */

json_t *w22_locallinea_3bb0fd_advance(const char *parent_session_id,
                                      const char *child_session_id,
                                      const json_t *entry, const json_t *db,
                                      const char *local_id, const char *root) {
    json_t *pid = json_string(parent_session_id ? parent_session_id : "");
    const json_t *parent = ll_session(db, pid);
    json_t *logical = parent ? json_obj_get(parent, "chat_id") : NULL;
    json_t *meta, *raw, *receipt = NULL, *routed = NULL, *out = NULL;

    json_free(pid);

    /* `if not logical_id: return` */
    if (!ll_truthy(logical)) {
        out = json_object();
        json_set(out, "action", json_string("noop"));
        return out;
    }
    if (logical->type != JSON_STRING) return ll_raised("TypeError");

    {
        char key[512];
        size_t i;
        snprintf(key, sizeof(key), "%s%s", LL_POLICY_PREFIX,
                 logical->str_val ? logical->str_val : "");
        meta = ll_doc_get(db, "state_meta");
        raw = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, key) : NULL;
        if (!raw) {
            /* No creation receipt: a legacy or local-* owner refuses, else noop. */
            json_t *legacy_ids = ll_doc_get(db, "legacy_ids");
            int legacy = 0;
            if (!strncmp(logical->str_val ? logical->str_val : "", "local-", 6)) {
                return ll_refused("storage_unavailable");
            }
            if (legacy_ids && legacy_ids->type == JSON_ARRAY) {
                for (i = 0; i < json_len(legacy_ids); i++) {
                    if (ll_eq(json_get(legacy_ids, i), logical)) { legacy = 1; break; }
                }
            }
            if (legacy) return ll_refused("storage_unavailable");
            out = json_object();
            json_set(out, "action", json_string("noop"));
            return out;
        }
    }

    /* --- the source's try block: KeyError/TypeError/ValueError -> storage_unavailable */
    {
        json_t *sid, *entry_in, *entry_sid, *principal, *route;
        const char *error = NULL;

        if (raw->type != JSON_STRING) goto storage;
        receipt = json_parse(raw->str_val, NULL);
        if (!receipt || receipt->type != JSON_OBJECT) goto storage;

        sid = json_obj_get(receipt, "session_id");
        if (!sid) goto storage;
        if (!ll_eq(sid, logical)) goto refused_storage;
        entry_in = json_obj_get(receipt, "entry");
        if (!entry_in || entry_in->type != JSON_OBJECT) goto storage;
        entry_sid = json_obj_get(entry_in, "session_id");
        if (!entry_sid) goto storage;
        if (!ll_eq_str(entry_sid, parent_session_id)) goto refused_storage;
        principal = json_obj_get(receipt, "principal_id");
        if (!principal) goto storage;
        if (!ll_eq(principal, json_obj_get(parent, "user_id"))) goto refused_storage;
        route = json_obj_get(receipt, "route");
        if (!route) goto storage;
        if (!ll_eq(route, json_obj_get(parent, "session_key"))) goto refused_storage;

        routed = ll_routing_entry(db, route, &error);
        if (error) goto storage;
        if (routed) {
            json_t *rsid;
            json_t *parent_v = json_string(parent_session_id ? parent_session_id : "");
            if (routed->type != JSON_OBJECT) { json_free(parent_v); goto storage; }
            rsid = json_obj_get(routed, "session_id");
            if (!rsid) { json_free(parent_v); goto storage; }
            if (!ll_eq(rsid, parent_v)) {
                json_free(routed);
                routed = NULL;
            }
            json_free(parent_v);
        }
        if (!routed) goto refused_conflict;

        {
            /* candidate = dict(entry or routed, session_id=child_session_id) */
            json_t *base = (entry && ll_truthy(entry)) ? (json_t *)entry : routed;
            json_t *candidate, *lineage_in, *newlineage;
            size_t i;
            if (base->type != JSON_OBJECT) goto storage;
            candidate = json_object();
            for (i = 0; i < json_object_size(base); i++) {
                const char *k = json_object_get_key_at(base, i);
                if (!k) continue;
                json_set(candidate, k, json_copy(json_object_get_at(base, i)));
            }
            json_set(candidate, "session_id",
                     json_string(child_session_id ? child_session_id : ""));
            json_set(receipt, "entry", candidate);

            lineage_in = json_obj_get(receipt, "lineage");
            newlineage = json_array();
            if (lineage_in) {
                if (lineage_in->type != JSON_ARRAY) {
                    json_free(newlineage);
                    goto storage;
                }
                for (i = 0; i < json_len(lineage_in); i++)
                    json_append(newlineage, json_copy(json_get(lineage_in, i)));
            } else {
                json_append(newlineage, json_copy(logical));
            }
            json_append(newlineage,
                        json_string(child_session_id ? child_session_id : ""));
            json_set(receipt, "lineage", newlineage);
        }

        {
            json_t *check = w22_locallinea_3bb0fd_validate(receipt, local_id, root, db);
            const char *status = json_get_str(check, "status", "");
            if (!strcmp(status, "refused")) {
                json_t *ref = ll_refused(json_get_str(check, "reason", "storage_unavailable"));
                json_free(check);
                json_free(receipt);
                json_free(routed);
                return ref;
            }
            if (strcmp(status, "ok")) {
                json_free(check);
                goto storage;
            }
            json_free(check);
        }

        /* _commit_local_target stores `candidate` as the route's entry, so the
         * routing row that lands is the candidate, not the stale routed row. */
        out = json_object();
        json_set(out, "action", json_string("commit"));
        json_set(out, "routed", json_copy(json_obj_get(receipt, "entry")));
        json_set(out, "receipt", receipt);
        return out;
    }

refused_storage:
    json_free(receipt);
    json_free(routed);
    return ll_refused("storage_unavailable");
refused_conflict:
    json_free(receipt);
    json_free(routed);
    return ll_refused("admission_conflict");
storage:
    json_free(receipt);
    json_free(routed);
    return ll_refused("storage_unavailable");
}
