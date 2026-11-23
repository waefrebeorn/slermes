/*
 * provenance.c -- C home for upstream Hermes acp_adapter/provenance.py
 * (wave8 gate w8_provenance_7265b6).
 *
 * PoP: w8_provenance_7265b6_build @ acp_adapter/provenance.py:build_session_provenance
 * PoP: w8_provenance_7265b6_meta @ acp_adapter/provenance.py:session_provenance_meta
 *
 * The persistence handle is injected as a JSON object of session rows, so the
 * parent walk / compression-depth logic is reproduced without any database.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w8_provenance_7265b6.h"

#define W8_PROV_MAX_WALK 100

static json_t *prov_get_row(const json_t *sessions, const char *sid) {
    if (!sessions || sessions->type != JSON_OBJECT || !sid || !sid[0]) return NULL;
    json_t *row = json_obj_get(sessions, sid);
    return (row && row->type == JSON_OBJECT) ? row : NULL;
}

/* Mirrors row.<attr>; NULL represents Python None. */
static const char *prov_row_field(json_t *row, const char *key) {
    if (!row) return NULL;
    json_t *v = json_obj_get(row, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int prov_is_compression_end(json_t *row) {
    if (!row) return 0;
    const char *reason = prov_row_field(row, "end_reason");
    return (reason && !strcmp(reason, "compression")) ? 1 : 0;
}

static int prov_seen_has(const char *const *seen, size_t n, const char *id) {
    for (size_t i = 0; i < n; i++)
        if (seen[i] && !strcmp(seen[i], id)) return 1;
    return 0;
}

static json_t *prov_build(const json_t *sessions, const char *acp_id,
                          const char *current_id, const char *previous) {
    json_t *row = prov_get_row(sessions, current_id);
    if (!row) return json_null();

    const char *parent_id = prov_row_field(row, "parent_session_id");
    const char *root_id = current_id;
    int depth = 0;
    const char *cursor = parent_id;

    const char *seen[W8_PROV_MAX_WALK + 8];
    size_t nseen = 0;
    seen[nseen++] = current_id;
    for (int i = 0; i < W8_PROV_MAX_WALK; i++) {
        if (!cursor || !cursor[0] || prov_seen_has(seen, nseen, cursor)) break;
        seen[nseen++] = cursor;
        json_t *prow = prov_get_row(sessions, cursor);
        if (!prow) break;
        root_id = cursor;
        if (prov_is_compression_end(prow)) depth++;
        cursor = prov_row_field(prow, "parent_session_id");
    }

    int is_continuation = (parent_id && parent_id[0])
                              ? prov_is_compression_end(prov_get_row(sessions, parent_id))
                              : 0;

    json_t *out = json_object();
    json_set(out, "acpSessionId", json_string(acp_id ? acp_id : ""));
    json_set(out, "currentHermesSessionId", json_string(current_id ? current_id : ""));
    json_set(out, "rootHermesSessionId", json_string(root_id ? root_id : ""));
    json_set(out, "parentHermesSessionId", parent_id ? json_string(parent_id) : json_null());
    json_set(out, "sessionKind", json_string(is_continuation ? "continuation" : "root"));
    json_set(out, "compressionDepth", json_number((double)depth));
    if (previous && previous[0]) {
        json_set(out, "previousHermesSessionId", json_string(previous));
        if (strcmp(previous, current_id ? current_id : "") != 0) {
            json_set(out, "reason", json_string("compression"));
            json_set(out, "creatorKind", json_string("compression"));
        }
    }
    return out;
}

json_t *w8_provenance_7265b6_build(const json_t *sessions, const char *acp_id,
                                   const char *current_id, const char *previous) {
    return prov_build(sessions, acp_id, current_id, previous);
}

json_t *w8_provenance_7265b6_meta(const json_t *sessions, const char *acp_id,
                                  const char *current_id, const char *previous) {
    json_t *provenance = prov_build(sessions, acp_id, current_id, previous);
    if (!provenance || provenance->type == JSON_NULL) return provenance ? provenance : json_null();
    json_t *inner = json_object();
    json_set(inner, "sessionProvenance", provenance);
    json_t *out = json_object();
    json_set(out, "hermes", inner);
    return out;
}
