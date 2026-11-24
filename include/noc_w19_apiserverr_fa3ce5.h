/* noc_w19_apiserverr_fa3ce5.h - public C API for gateway/platforms/api_server_response_store.py port. */
#ifndef NOC_W19_APISERVERR_FA3CE5_H
#define NOC_W19_APISERVERR_FA3CE5_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_apiserverr_fa3ce5_identity_bound @ gateway/platforms/api_server_response_store.py:ResponseStore._identity_bound
 * max_identities, or IDENTITY_RETENTION_FACTOR (10) * max_size when it is absent/falsy. */
long long w19_apiserverr_fa3ce5_identity_bound(int has_max_identities, double max_identities,
                                              double max_size);

/* PoP: w19_apiserverr_fa3ce5_put_plan @ gateway/platforms/api_server_response_store.py:ResponseStore.put
 * "idem:" bodies are write-once replay records (INSERT OR IGNORE) and settle their identity;
 * every other body replaces (INSERT OR REPLACE). */
json_t *w19_apiserverr_fa3ce5_put_plan(const char *response_id);

/* PoP: w19_apiserverr_fa3ce5_delete_result @ gateway/platforms/api_server_response_store.py:ResponseStore.delete
 * True when a body row or one of its settled identities was found and deleted. */
int w19_apiserverr_fa3ce5_delete_result(int row_deleted, int forgotten);

/* PoP: w19_apiserverr_fa3ce5_request_key_matches @ gateway/platforms/api_server_response_store.py:ResponseStore.request_key_matches
 * An unknown key matches (it will be bound); a bound key matches only its own fingerprint. */
int w19_apiserverr_fa3ce5_request_key_matches(int has_row, const char *stored_fingerprint,
                                             const char *fingerprint);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_APISERVERR_FA3CE5_H */
