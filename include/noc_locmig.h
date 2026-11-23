#ifndef NOC_LOCMIG_H
#define NOC_LOCMIG_H
#include <stddef.h>

/* C port of persistence/sqlite/conversation/_store/local_migration.py.
 * PoP: locmig_legacy_lineage_root @ persistence/sqlite/conversation/_store/local_migration.py:legacy_lineage_root
 * PoP: LEGACY_PREFIX @ persistence/sqlite/conversation/_store/local_migration.py:LEGACY_PREFIX
 */
#define LOCMIG_LEGACY_PREFIX "gateway.local_legacy.v1:"

/* Faithful form of legacy_lineage_root(conn, receipt).
 * Python raises AuthorityRefusal('storage_unavailable'); here *refused is set
 * to 1 and NULL is returned. On success the root is written to out (when
 * provided) and returned. stored_value is row[0] (a serialized JSON object),
 * or NULL when row is None. */
const char *locmig_legacy_lineage_root(
    const char *session_id, const char *profile_id, const char *principal_id,
    int has_legacy, const char *legacy_session_id,
    const char *stored_value, char *out, size_t out_n, int *refused);

#endif /* NOC_LOCMIG_H */
