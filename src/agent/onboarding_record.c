/*
 * onboarding_record.c - real C11 port of upstream Hermes agent/onboarding_record.py.
 *
 * PoP: w9_onboardingr_7b3735_load     @ agent/onboarding_record.py:load
 * PoP: w9_onboardingr_7b3735_announce @ agent/onboarding_record.py:_announce
 *
 * The module's observable, state-free surface is ported here: load()'s guard
 * chain + stored-record dict filter, and the ended-payload builder in
 * _announce().  DB/config/UUID side effects are injected by the oracle via
 * seams (db_none, the store's returned record), never reproduced as stubs.
 */
#include "hermes_json.h"
#include "noc_w9_onboardingr_7b3735.h"
#include <string.h>

struct json_t *w9_onboardingr_7b3735_load(int db_none, const char *session_id,
                                           const struct json_t *record)
{
    if (db_none || !session_id || session_id[0] == '\0')
        return json_null();
    if (!record || record->type != JSON_OBJECT)
        return json_null();
    return json_copy(record);
}

struct json_t *w9_onboardingr_7b3735_announce(const char *run_id,
                                               const char *session_id,
                                               const char *outcome)
{
    struct json_t *obj = json_object();
    const char *rid = (run_id && run_id[0] != '\0') ? run_id : "";
    json_set(obj, "run_id", json_string(rid));
    json_set(obj, "stored_session_id", json_string(session_id ? session_id : ""));
    json_set(obj, "outcome", json_string(outcome ? outcome : ""));
    return obj;
}
