/*
 * bindings.c - real C11 port of upstream Hermes
 *   persistence/sqlite/authority/bindings.py record shaping.
 * Every body's SQL lives in the sibling _local/_binding_api/_binding_history modules;
 * what this module owns is the receipt/record conversion, ported here.
 * PoP: w19_bindings_fe89c1_new_receipt @ persistence/sqlite/authority/bindings.py:_new_receipt
 * PoP: w19_bindings_fe89c1_api_binding @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.api_binding
 * PoP: w19_bindings_fe89c1_binding_marks @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.binding_marks
 * PoP: w19_bindings_fe89c1_handoff_head @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.handoff_head
 * PoP: w19_bindings_fe89c1_legacy_adoption_scan @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.legacy_adoption_scan
 * PoP: w19_bindings_fe89c1_lineages @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.list_lineages
 */
#include "noc_w19_bindings_fe89c1.h"

#include <stdlib.h>
#include <string.h>

static json_t *copy_or_null(json_t *v) {
    return v ? json_copy(v) : json_null();
}

static int present(json_t *v) { return v && v->type != JSON_NULL; }

json_t *w19_bindings_fe89c1_new_receipt(json_t *binding) {
    json_t *out = json_object();
    json_t *b = (binding && binding->type == JSON_OBJECT) ? binding : NULL;
    json_set(out, "profile_id", copy_or_null(json_obj_get(b, "profile_id")));
    json_set(out, "principal_id", copy_or_null(json_obj_get(b, "principal_id")));
    json_set(out, "request_id", copy_or_null(json_obj_get(b, "request_id")));
    json_set(out, "session_id", copy_or_null(json_obj_get(b, "session_id")));
    json_set(out, "route", copy_or_null(json_obj_get(b, "route")));
    json_set(out, "entry", copy_or_null(json_obj_get(b, "entry")));
    json_set(out, "policy", copy_or_null(json_obj_get(b, "policy")));
    json_t *legacy = json_obj_get(b, "legacy_session_id");
    if (present(legacy)) json_set(out, "legacy_session_id", json_copy(legacy));
    json_t *lineage = json_obj_get(b, "lineage");
    if (present(lineage)) json_set(out, "lineage", json_copy(lineage));
    return out;
}

json_t *w19_bindings_fe89c1_api_binding(json_t *receipt) {
    if (!present(receipt) || receipt->type != JSON_OBJECT) return json_null();
    json_t *out = json_object();
    json_set(out, "profile_id", copy_or_null(json_obj_get(receipt, "profile_id")));
    json_set(out, "session_id", copy_or_null(json_obj_get(receipt, "session_id")));
    json_set(out, "route", copy_or_null(json_obj_get(receipt, "route")));
    json_set(out, "entry", copy_or_null(json_obj_get(receipt, "entry")));
    json_set(out, "declared_key", copy_or_null(json_obj_get(receipt, "declared_key")));

    json_t *storage = json_obj_get(receipt, "storage_source");
    json_set(out, "storage_source", present(storage) ? json_copy(storage) : json_string("api_server"));

    json_t *identity = json_obj_get(receipt, "room_identity");
    json_set(out, "room_identity", present(identity) ? json_copy(identity) : json_null());

    json_t *gen = json_obj_get(receipt, "room_generation");
    long long generation = 0;
    if (present(gen)) {
        if (gen->type == JSON_NUMBER) generation = (long long)gen->num_val;
        else if (gen->type == JSON_STRING) generation = strtoll(gen->str_val, NULL, 10);
        else if (gen->type == JSON_BOOL) generation = gen->bool_val ? 1 : 0;
    }
    json_set(out, "room_generation", json_number((double)generation));
    return out;
}

json_t *w19_bindings_fe89c1_binding_marks(json_t *policy, json_t *legacy, json_t *owner) {
    json_t *out = json_object();
    json_set(out, "policy", copy_or_null(policy));
    json_set(out, "legacy", copy_or_null(legacy));
    json_set(out, "lineage_owner", present(owner) ? json_copy(owner) : json_null());
    return out;
}

json_t *w19_bindings_fe89c1_handoff_head(json_t *owner, json_t *head) {
    json_t *out = json_array();
    json_append(out, present(owner) ? json_copy(owner) : json_null());
    json_append(out, copy_or_null(head));
    return out;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w19_bindings_fe89c1_legacy_adoption_scan(json_t *ids, json_t *marked) {
    json_t *out = json_object();
    json_t *ids_out = json_array();
    if (ids && ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ids); i++) json_append(ids_out, json_copy(json_get(ids, i)));
    }
    json_set(out, "session_ids", ids_out);

    const char *items[1024];
    int n = 0;
    if (marked && marked->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(marked) && n < 1024; i++) {
            json_t *v = json_get(marked, i);
            if (v && v->type == JSON_STRING && v->str_val) items[n++] = v->str_val;
        }
    }
    qsort(items, (size_t)n, sizeof(items[0]), cmp_str);
    json_t *marks = json_array();
    for (int i = 0; i < n; i++) {
        if (i > 0 && !strcmp(items[i], items[i - 1])) continue;
        json_append(marks, json_string(items[i]));
    }
    json_set(out, "marked", marks);
    return out;
}

json_t *w19_bindings_fe89c1_lineages(json_t *found) {
    json_t *out = json_array();
    if (!found || found->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(found); i++) {
        json_t *pair = json_get(found, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        json_t *rec = json_object();
        json_set(rec, "owner", json_copy(json_get(pair, 0)));
        json_set(rec, "lineage", json_copy(json_get(pair, 1)));
        json_append(out, rec);
    }
    return out;
}
