#ifndef NOC_W9_ONBOARDINGR_7B3735_H
#define NOC_W9_ONBOARDINGR_7B3735_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_onboardingr_7b3735_load @ agent/onboarding_record.py:load
 * Faithful port of load()'s guards and stored-record type filter:
 *   if db is None or not session_id: return None
 *   record = db.get_session_model_config_value(session_id, KEY)
 *   return record if isinstance(record, dict) else None
 * `db_none` reflects "db is None"; `record` is the value the store returned.
 * Returns json_null() or a deep copy of `record` (caller frees). */
struct json_t *w9_onboardingr_7b3735_load(int db_none, const char *session_id,
                                           const struct json_t *record);

/* PoP: w9_onboardingr_7b3735_announce @ agent/onboarding_record.py:_announce
 * Builds the ended payload {"run_id": run_id or "", "stored_session_id":
 * session_id, "outcome": outcome}. A NULL/empty run_id coerces to "".
 * Returns an owned JSON object (caller frees). */
struct json_t *w9_onboardingr_7b3735_announce(const char *run_id,
                                               const char *session_id,
                                               const char *outcome);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_ONBOARDINGR_7B3735_H */
