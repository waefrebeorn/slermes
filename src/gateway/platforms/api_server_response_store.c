/*
 * api_server_response_store.c - C home for gateway/platforms/api_server_response_store.py.
 * Ports the pure decisions its SQL methods carry: the identity count bound, the body write verb,
 * the delete verdict and the request-key fingerprint match. The connection work itself stays
 * Python (live sqlite3), which is where these decisions are applied.
 * PoP: w19_apiserverr_fa3ce5_identity_bound @ gateway/platforms/api_server_response_store.py:ResponseStore._identity_bound
 * PoP: w19_apiserverr_fa3ce5_put_plan @ gateway/platforms/api_server_response_store.py:ResponseStore.put
 * PoP: w19_apiserverr_fa3ce5_delete_result @ gateway/platforms/api_server_response_store.py:ResponseStore.delete
 * PoP: w19_apiserverr_fa3ce5_request_key_matches @ gateway/platforms/api_server_response_store.py:ResponseStore.request_key_matches
 */
#include "noc_w19_apiserverr_fa3ce5.h"
#include <string.h>

/* Module constants, mirrored from the Python source. */
#define MAX_STORED_RESPONSES 100
#define IDENTITY_RETENTION_FACTOR 10

long long w19_apiserverr_fa3ce5_identity_bound(int has_max_identities, double max_identities,
                                              double max_size) {
    /* return self._max_identities or IDENTITY_RETENTION_FACTOR * self._max_size */
    if (has_max_identities && max_identities != 0.0) return (long long)max_identities;
    return (long long)(IDENTITY_RETENTION_FACTOR * max_size);
}

json_t *w19_apiserverr_fa3ce5_put_plan(const char *response_id) {
    const char *rid = response_id ? response_id : "";
    int idem = strncmp(rid, "idem:", 5) == 0;
    json_t *out = json_object();
    json_set(out, "verb", json_string(idem ? "INSERT OR IGNORE" : "INSERT OR REPLACE"));
    json_set(out, "settles", json_bool(idem));
    return out;
}

int w19_apiserverr_fa3ce5_delete_result(int row_deleted, int forgotten) {
    return (row_deleted > 0 || forgotten > 0) ? 1 : 0;
}

int w19_apiserverr_fa3ce5_request_key_matches(int has_row, const char *stored_fingerprint,
                                             const char *fingerprint) {
    if (!has_row) return 1;  /* row is None: the key was never bound */
    if (!stored_fingerprint) return fingerprint ? 0 : 1;
    if (!fingerprint) return 0;
    return strcmp(stored_fingerprint, fingerprint) == 0 ? 1 : 0;
}
