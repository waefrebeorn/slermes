/*
 * local_migration.c — C port of
 *   persistence/sqlite/conversation/_store/local_migration.py
 * PoP: locmig_legacy_lineage_root @ persistence/sqlite/conversation/_store/local_migration.py:legacy_lineage_root
 * PoP: LEGACY_PREFIX @ persistence/sqlite/conversation/_store/local_migration.py:LEGACY_PREFIX
 *
 * Python:
 *   root = receipt.get('legacy_session_id', receipt['session_id'])
 *   if 'legacy_session_id' in receipt:
 *       if root != receipt['session_id']: raise AuthorityRefusal(...)
 *       row = conn.execute('SELECT value ...', (LEGACY_PREFIX + root,)).fetchone()
 *       expected = {k: receipt[k] for k in ('session_id','profile_id',
 *                   'principal_id','legacy_session_id')}
 *       if row is None or json.loads(row[0]) != expected: raise ...
 *   return root
 */
#include "noc_locmig.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int json_eq_string(const json_t *obj, const char *key, const char *val) {
    json_t *v = json_obj_get(obj, key);
    return v && v->type == JSON_STRING && val && strcmp(v->str_val, val) == 0;
}

const char *locmig_legacy_lineage_root(
    const char *session_id, const char *profile_id, const char *principal_id,
    int has_legacy, const char *legacy_session_id,
    const char *stored_value, char *out, size_t out_n, int *refused) {
    if (refused) *refused = 0;

    const char *sid = session_id ? session_id : "";
    const char *root = has_legacy ? legacy_session_id : sid;

    if (has_legacy) {
        if (!root || strcmp(root, sid) != 0) {
            if (refused) *refused = 1;
            return NULL;
        }
        if (!stored_value) { /* row is None */
            if (refused) *refused = 1;
            return NULL;
        }
        json_t *stored = json_parse(stored_value, NULL);
        int ok = 0;
        if (stored && stored->type == JSON_OBJECT && json_object_size(stored) == 4) {
            if (json_eq_string(stored, "session_id", sid) &&
                json_eq_string(stored, "profile_id", profile_id ? profile_id : "") &&
                json_eq_string(stored, "principal_id", principal_id ? principal_id : "") &&
                json_eq_string(stored, "legacy_session_id", legacy_session_id)) {
                ok = 1;
            }
        }
        json_free(stored);
        if (!ok) {
            if (refused) *refused = 1;
            return NULL;
        }
    }

    if (!root) root = "";
    if (out && out_n) {
        snprintf(out, out_n, "%s", root);
        return out;
    }
    return root;
}
