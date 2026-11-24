/* noc_w19_admissions_c8ef95.h - public C API for persistence/sqlite/authority/admissions.py port.
 * The repository's SQL verbs stay Python-side; the row -> contract-record projection each verb
 * returns through is ported here. */
#ifndef NOC_W19_ADMISSIONS_C8EF95_H
#define NOC_W19_ADMISSIONS_C8EF95_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_admissions_c8ef95_record @ persistence/sqlite/authority/admissions.py:_record
 * Project an admission row (an object keyed by the AdmissionRecord fields) onto the contract
 * record; a NULL row answers null. A missing field answers {"error": "KeyError: 'name'"} and an
 * out-of-vocabulary intent/status answers the dataclass's ValueError, exactly as the live code. */
json_t *w19_admissions_c8ef95_record(const json_t *row);

/* PoP: w19_admissions_c8ef95_claimed @ persistence/sqlite/authority/admissions.py:_claimed
 * The record `claim` started: the same projection plus transcript_mark and queued_owner_epoch. */
json_t *w19_admissions_c8ef95_claimed(const json_t *row);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADMISSIONS_C8EF95_H */
