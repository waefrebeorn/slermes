#ifndef NOC_W16_KANBANDBC_DB6090_H
#define NOC_W16_KANBANDBC_DB6090_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/kanban_db_connect.py (gate w16_kanbandbc_db6090). */

/* _looks_like_tls_record_at(data, offset): TLS record header probe. */
json_t *w16_kanbandbc_db6090_looks_like_tls_record_at(const json_t *data, int offset);

/* _integrity_messages_ok(messages): PRAGMA integrity_check == single "ok". */
json_t *w16_kanbandbc_db6090_integrity_messages_ok(const json_t *messages);

/* _repairable_index_names(messages): distinct index names iff every row is repairable. */
json_t *w16_kanbandbc_db6090_repairable_index_names(const json_t *messages);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_KANBANDBC_DB6090_H */
