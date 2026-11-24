#ifndef NOC_W20_VOCABULARY_6F97E2_H
#define NOC_W20_VOCABULARY_6F97E2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w20_vocabulary_6f97e2_admission_fingerprint @ persistence/authority/vocabulary.py:admission_fingerprint
 * sha256 hex of json.dumps({"target": canonical_target, "payload": payload},
 * ensure_ascii=False, sort_keys=True, separators=(",", ":")). `payload_json` is the
 * payload's JSON text; NULL/absent reads as None. */
struct json_t *w20_vocabulary_6f97e2_admission_fingerprint(const char *canonical_target,
                                                           const char *payload_json);

/* PoP: w20_vocabulary_6f97e2_queue_max @ persistence/authority/vocabulary.py:ADMISSION_QUEUE_MAX */
struct json_t *w20_vocabulary_6f97e2_queue_max(void);

/* PoP: w20_vocabulary_6f97e2_retry_horizon @ persistence/authority/vocabulary.py:RETRY_HORIZON_S */
struct json_t *w20_vocabulary_6f97e2_retry_horizon(void);

/* PoP: w20_vocabulary_6f97e2_admitted_request_conflict @ persistence/authority/vocabulary.py:ADMITTED_REQUEST_CONFLICT */
struct json_t *w20_vocabulary_6f97e2_admitted_request_conflict(void);

/* PoP: w20_vocabulary_6f97e2_classify_label @ persistence/authority/vocabulary.py:LEGACY_LOCAL_SOURCES
 * Membership of one label in the three shared label sets, with the legacy
 * creation-source mapping resolved the way LEGACY_CREATION_SOURCES.get() does. */
struct json_t *w20_vocabulary_6f97e2_classify_label(const char *label);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_VOCABULARY_6F97E2_H */
