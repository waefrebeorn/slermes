/*
 * _binding_api.c -- real C11 port of the API binding write ladder in upstream
 * Hermes persistence/sqlite/authority/_binding_api.py.
 *
 * PoP: w20_bindingapi_eaedd9_bind @ persistence/sqlite/authority/_binding_api.py:bind_api
 * PoP: w20_bindingapi_eaedd9_receipt @ persistence/sqlite/authority/_binding_api.py:api_binding_receipt
 * PoP: w20_bindingapi_eaedd9_declared_session @ persistence/sqlite/authority/_binding_api.py:declared_session
 * PoP: w20_bindingapi_eaedd9_heal_replacement @ persistence/sqlite/authority/_binding_api.py:_heal_replacement
 *
 * The SQLite connection is a scripted JSON snapshot: state_meta as a map, the
 * sessions and gateway_routing rows as arrays. Writes happen on private copies
 * and are only published when the whole transaction commits -- the Python
 * holder rolls the transaction back on a refusal, and so does this port.
 */
#include "hermes_json.h"
#include "noc_w20_bindingapi_eaedd9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAED_BINDING_PREFIX  "gateway.api.binding.v1."
#define BAED_DECLARED_PREFIX "gateway.api.conversation.v1."
#define BAED_RETIRED_PREFIX  "gateway.retired_session.v1."

static char *baed_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *baed_cat(const char *a, const char *b) {
    size_t na = a ? strlen(a) : 0, nb = b ? strlen(b) : 0;
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static const char *baed_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static const json_t *baed_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static const char *baed_param(const json_t *req, const char *key, const char *def) {
    const char *v = baed_str(baed_get(req, key));
    return v ? v : def;
}

static int baed_is_object(const json_t *v) { return v && v->type == JSON_OBJECT; }

/* Python truthiness for the values this ladder branches on. */
static int baed_truthy(const json_t *v) {
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

static const char *baed_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static json_t *baed_refused(const char *reason) {
    json_t *o = json_object();
    json_set(o, "status", json_string("refused"));
    json_set(o, "reason", json_string(reason));
    return o;
}

static json_t *baed_raised(const char *type, const char *message) {
    json_t *o = json_object();
    json_set(o, "status", json_string("raised"));
    json_set(o, "type", json_string(type));
    json_set(o, "message", json_string(message));
    return o;
}

/* `_DECLARED_PREFIX + key` on a non-string key: Python's TypeError. */
static json_t *baed_bad_key(const json_t *key) {
    char buf[128];
    snprintf(buf, sizeof buf, "can only concatenate str (not \"%s\") to str",
             baed_type_name(key));
    return baed_raised("TypeError", buf);
}

static json_t *baed_map_get(json_t *map, const char *key) {
    return (map && map->type == JSON_OBJECT) ? json_obj_get(map, key) : NULL;
}

/* INSERT ... ON CONFLICT(key) DO NOTHING */
static void baed_map_put_new(json_t *map, const char *key, const char *value) {
    if (!map || map->type != JSON_OBJECT || !key) return;
    if (json_obj_get(map, key)) return;
    json_set(map, key, json_string(value));
}

/* SQLite always projects every column: a seeded row missing one reads as NULL. */
static void baed_normalize(json_t *row, const char *const *keys, size_t n) {
    size_t i;
    if (!baed_is_object(row)) return;
    for (i = 0; i < n; i++)
        if (!json_obj_get(row, keys[i])) json_set(row, keys[i], json_null());
}

static void baed_normalize_all(json_t *rows, const char *const *keys, size_t n) {
    size_t i;
    if (!rows || rows->type != JSON_ARRAY) return;
    for (i = 0; i < rows->c.count; i++) baed_normalize(rows->c.items[i], keys, n);
}

static const char *const BAED_SESSION_KEYS[] = {
    "id", "source", "title", "hidden", "started_at",
    "session_key", "chat_id", "user_id", "chat_type", "origin"
};
static const char *const BAED_ROUTING_KEYS[] = {
    "session_key", "entry", "updated_at"
};

static json_t *baed_find_session(json_t *sessions, const char *id) {
    size_t i;
    if (!sessions || sessions->type != JSON_ARRAY || !id) return NULL;
    for (i = 0; i < sessions->c.count; i++) {
        json_t *row = sessions->c.items[i];
        const json_t *v = baed_get(row, "id");
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, id)) return row;
    }
    return NULL;
}

static json_t *baed_find_routing(json_t *routing, const char *route) {
    size_t i;
    if (!routing || routing->type != JSON_ARRAY || !route) return NULL;
    for (i = 0; i < routing->c.count; i++) {
        json_t *row = routing->c.items[i];
        const json_t *v = baed_get(row, "session_key");
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, route)) return row;
    }
    return NULL;
}

/* state_meta lookup by prefix + name. */
static const json_t *baed_lookup(json_t *sm, const char *prefix, const char *name) {
    char *key = baed_cat(prefix, name);
    const json_t *out = key ? baed_map_get(sm, key) : NULL;
    free(key);
    return out;
}

json_t *w20_bindingapi_eaedd9_receipt(const json_t *state_meta, const char *session_id) {
    const json_t *saved = baed_lookup((json_t *)state_meta, BAED_BINDING_PREFIX, session_id);
    const char *text = baed_str(saved);
    if (!text) return NULL;
    return json_parse(text, NULL);
}

json_t *w20_bindingapi_eaedd9_declared_session(const json_t *state_meta, const char *key) {
    const json_t *row = baed_lookup((json_t *)state_meta, BAED_DECLARED_PREFIX, key);
    const char *text = baed_str(row);
    return text ? json_string(text) : NULL;
}

int w20_bindingapi_eaedd9_heal_replacement(const json_t *state_meta, const char *holder) {
    return baed_lookup((json_t *)state_meta, BAED_BINDING_PREFIX, holder) ? 0 : 1;
}

/* --- bind_api ------------------------------------------------------------- */

json_t *w20_bindingapi_eaedd9_bind(const json_t *request) {
    const json_t *req = request;
    const char *session_id = baed_param(req, "session_id", "");
    const char *route = baed_param(req, "route", "");
    const char *storage_source = baed_param(req, "storage_source", "");
    const char *title = baed_param(req, "title", "");
    const json_t *declared_v = baed_get(req, "declared_key");
    const json_t *caller_v = baed_get(req, "caller_key");
    const json_t *source = baed_get(req, "source");
    const json_t *entry = baed_get(req, "entry");
    const json_t *receipt_in = baed_get(req, "receipt");
    const json_t *epoch_v = baed_get(req, "epoch");
    const json_t *runtime_epoch_v = baed_get(req, "runtime_epoch");
    const json_t *gate = baed_get(req, "gate");
    const json_t *walked_v = baed_get(req, "walked_from");
    const char *declared_key = baed_str(declared_v);
    const char *caller_key = baed_str(caller_v);
    const char *walked_from = baed_str(walked_v);
    const json_t *now_v = baed_get(req, "now");
    double now_num = now_v && now_v->type == JSON_NUMBER ? now_v->num_val : 0;
    json_t *sm = NULL, *sessions = NULL, *routing = NULL, *receipt = NULL;
    json_t *result = NULL;
    json_t *row = NULL;
    const json_t *saved;
    const char *legacy_key = NULL;
    double epoch_num;

    /* _epoch(conn, epoch): type(epoch) is not int or row is None or row[0] != epoch.
     * A request without runtime_epoch means the row was seeded with the epoch. */
    epoch_num = epoch_v ? epoch_v->num_val : 0;
    if (!epoch_v || epoch_v->type != JSON_NUMBER || epoch_num != (double)(long long)epoch_num)
        return baed_refused("stale_epoch");
    if (runtime_epoch_v) {
        if (runtime_epoch_v->type != JSON_NUMBER || runtime_epoch_v->num_val != epoch_num)
            return baed_refused("stale_epoch");
    }

    /* raise_gate(gate) */
    if (gate && gate->type != JSON_NULL) {
        const char *reason = baed_str(baed_get(gate, "reason"));
        if (reason) return baed_refused(reason);
    }

    /* `_DECLARED_PREFIX + key` before the write: a non-str declared key is a TypeError. */
    if (declared_v && declared_v->type != JSON_NULL && declared_v->type != JSON_STRING &&
        baed_truthy(declared_v))
        return baed_bad_key(declared_v);

    /* the working copies */
    sm = json_copy(baed_get(req, "state_meta"));
    if (!baed_is_object(sm)) { json_free(sm); sm = json_object(); }
    sessions = json_copy(baed_get(req, "sessions"));
    if (!sessions || sessions->type != JSON_ARRAY) { json_free(sessions); sessions = json_array(); }
    routing = json_copy(baed_get(req, "routing"));
    if (!routing || routing->type != JSON_ARRAY) { json_free(routing); routing = json_array(); }
    baed_normalize_all(sessions, BAED_SESSION_KEYS,
                       sizeof BAED_SESSION_KEYS / sizeof *BAED_SESSION_KEYS);
    baed_normalize_all(routing, BAED_ROUTING_KEYS,
                       sizeof BAED_ROUTING_KEYS / sizeof *BAED_ROUTING_KEYS);
    /* `receipt = dict(receipt)`: a mapping copies, an empty sequence is empty,
     * and a non-iterable is the caller's TypeError. */
    receipt = json_copy(receipt_in);
    if (!baed_is_object(receipt)) {
        json_free(receipt);
        receipt = json_object();
        if (receipt_in && receipt_in->type == JSON_STRING && receipt_in->str_val &&
            receipt_in->str_val[0] && !receipt_in->str_val[1]) {
            json_free(sm); json_free(sessions); json_free(routing);
            return baed_raised("ValueError",
                "dictionary update sequence element #0 has length 1; 2 is required");
        }
        if (receipt_in && (receipt_in->type == JSON_NUMBER || receipt_in->type == JSON_BOOL)) {
            char buf[96];
            json_t *err;
            snprintf(buf, sizeof buf, "'%s' object is not iterable", baed_type_name(receipt_in));
            json_free(sm); json_free(sessions); json_free(routing);
            err = baed_raised("TypeError", buf);
            return err;
        }
    }

    /* retired session marker -> not_found */
    if (baed_lookup(sm, BAED_RETIRED_PREFIX, session_id)) {
        result = baed_refused("not_found");
        goto done;
    }
    /* a walk's passed fence was reclaimed since -> chain_moved */
    if (walked_from && walked_from[0] && !json_get_bool(req, "fence_ok", true)) {
        result = baed_refused("chain_moved");
        goto done;
    }

    /* saved binding */
    saved = baed_lookup(sm, BAED_BINDING_PREFIX, session_id);
    if (saved && saved->type != JSON_NULL) {
        json_t *binding = json_parse(baed_str(saved) ? baed_str(saved) : "", NULL);
        const json_t *bss = baed_get(binding, "storage_source");
        const char *bss_s = baed_str(bss);
        if (!bss_s) bss_s = "api_server";
        if (strcmp(bss_s, storage_source) != 0) {
            json_free(binding);
            result = baed_refused("permission_denied");
            goto done;
        }
        if (declared_key && declared_key[0]) {
            const char *bdk_s = baed_str(baed_get(binding, "declared_key"));
            if (!bdk_s || strcmp(bdk_s, declared_key) != 0) {
                json_free(binding);
                result = baed_refused("admission_conflict");
                goto done;
            }
        }
        /* an identical earlier binding: the write is a no-op */
        json_free(binding);
        goto commit;
    }

    /* declared-key registry */
    if (declared_key && declared_key[0]) {
        const json_t *existing = baed_lookup(sm, BAED_DECLARED_PREFIX, declared_key);
        const char *existing_s = baed_str(existing);
        if (existing_s && strcmp(existing_s, session_id) != 0) {
            result = baed_refused("admission_conflict");
            goto done;
        }
        {
            char *key = baed_cat(BAED_DECLARED_PREFIX, declared_key);
            baed_map_put_new(sm, key, session_id);
            free(key);
        }
    }

    /* the session row */
    row = baed_find_session(sessions, session_id);
    if (row) {
        const char *row_source = baed_str(baed_get(row, "source"));
        if (!row_source || strcmp(row_source, storage_source) != 0) {
            result = baed_refused("permission_denied");
            goto done;
        }
    }
    if (!strcmp(storage_source, "bot_room")) {
        if (row) {
            const char *row_title = baed_str(baed_get(row, "title"));
            if (!row_title || strcmp(row_title, title) != 0) {
                result = baed_refused("admission_conflict");
                goto done;
            }
        }
        {
            size_t i;
            for (i = 0; i < sessions->c.count; i++) {
                json_t *other = sessions->c.items[i];
                const char *oid = baed_str(baed_get(other, "id"));
                const char *otitle = baed_str(baed_get(other, "title"));
                if (oid && strcmp(oid, session_id) != 0 && otitle && !strcmp(otitle, title)) {
                    result = baed_refused("admission_conflict");
                    goto done;
                }
            }
        }
    }

    /* legacy caller-declared key adoption (#139835) */
    if (declared_v && baed_truthy(declared_v)) legacy_key = baed_str(declared_v);
    else legacy_key = caller_key;
    if (row) {
        const char *sk = baed_str(baed_get(row, "session_key"));
        if (sk && sk[0] && strcmp(sk, route) != 0) {
            if (!legacy_key || !legacy_key[0] || strcmp(sk, legacy_key) != 0) {
                result = baed_refused("admission_conflict");
                goto done;
            }
            {
                const json_t *owner = baed_lookup(sm, BAED_DECLARED_PREFIX, legacy_key);
                const char *owner_s = baed_str(owner);
                if (owner_s && strcmp(owner_s, session_id) != 0) {
                    result = baed_refused("admission_conflict");
                    goto done;
                }
            }
            {
                char *key = baed_cat(BAED_DECLARED_PREFIX, legacy_key);
                baed_map_put_new(sm, key, session_id);
                free(key);
            }
            json_set(receipt, "declared_key", json_string(legacy_key));
        }
    }

    /* the route's held session */
    {
        json_t *held_row = baed_find_routing(routing, route);
        if (held_row) {
            const char *hs = baed_str(baed_get(baed_get(held_row, "entry"), "session_id"));
            if (!hs) {
                result = baed_raised("KeyError", "'session_id'");
                goto done;
            }
            if (strcmp(hs, session_id) != 0 &&
                !w20_bindingapi_eaedd9_heal_replacement(sm, hs)) {
                result = baed_refused("admission_conflict");
                goto done;
            }
        }
    }

    if (!source || source->type != JSON_OBJECT) {
        char buf[96];
        if (source && source->type == JSON_STRING)
            snprintf(buf, sizeof buf, "string indices must be integers, not 'str'");
        else
            snprintf(buf, sizeof buf, "%s indices must be integers or slices, not str",
                     baed_type_name(source));
        result = baed_raised("TypeError", buf);
        goto done;
    }
    if (!baed_get(source, "user_id")) {
        result = baed_raised("KeyError", "'user_id'");
        goto done;
    }
    if (!entry || entry->type != JSON_OBJECT) {
        result = baed_refused("invalid_params");
        goto done;
    }

    /* INSERT INTO sessions ... ON CONFLICT(id) DO NOTHING */
    if (!row) {
        json_t *fresh = json_object();
        json_set(fresh, "id", json_string(session_id));
        json_set(fresh, "source", json_string(storage_source));
        json_set(fresh, "title", json_string(title));
        json_set(fresh, "hidden", json_number(!strcmp(storage_source, "bot_room") ? 1 : 0));
        json_set(fresh, "started_at", json_number(now_num));
        json_set(fresh, "session_key", json_null());
        json_set(fresh, "chat_id", json_null());
        json_set(fresh, "user_id", json_null());
        json_set(fresh, "chat_type", json_null());
        json_set(fresh, "origin", json_null());
        json_append(sessions, fresh);
        row = fresh;
    }
    /* UPDATE sessions SET session_key=?, chat_id=?, user_id=?, chat_type=?, origin_json=? */
    json_set(row, "session_key", json_string(route));
    json_set(row, "chat_id", json_string(session_id));
    json_set(row, "user_id", json_copy(baed_get(source, "user_id")));
    json_set(row, "chat_type", json_string("dm"));
    json_set(row, "origin", json_copy(source));

    /* INSERT gateway_routing ... ON CONFLICT(scope,session_key) DO UPDATE SET entry_json */
    {
        json_t *held_row = baed_find_routing(routing, route);
        if (held_row) {
            json_set(held_row, "entry", json_copy(entry));
        } else {
            json_t *fresh = json_object();
            json_set(fresh, "session_key", json_string(route));
            json_set(fresh, "updated_at", json_number(now_num));
            json_set(fresh, "entry", json_copy(entry));
            json_append(routing, fresh);
        }
    }

    /* INSERT INTO state_meta(key,value) VALUES(BINDING+, json(receipt)) */
    {
        char *key = baed_cat(BAED_BINDING_PREFIX, session_id);
        char *text = json_dumps(receipt, 0);
        if (key && text) json_set(sm, key, json_string(text));
        free(key);
        free(text);
    }

commit:
    result = json_object();
    json_set(result, "status", json_string("ok"));
    {
        json_t *value = json_object();
        json_set(value, "epoch", json_number(epoch_num));
        json_set(value, "state_meta", sm);
        json_set(value, "sessions", sessions);
        json_set(value, "routing", routing);
        json_set(result, "value", value);
        sm = sessions = routing = NULL;
    }
done:
    json_free(sm);
    json_free(sessions);
    json_free(routing);
    json_free(receipt);
    return result;
}
