/*
 * onboarding_run.c — C home for upstream Hermes hermes_cli/onboarding_run.py.
 * PoP: w12_onboarding_37e376_explicit_run @ hermes_cli/onboarding_run.py:_explicit_run
 * PoP: w12_onboarding_37e376_should_run   @ hermes_cli/onboarding_run.py:should_run
 * PoP: w12_onboarding_37e376_latch        @ hermes_cli/onboarding_run.py:latch_first_install
 *
 * Faithful port of the flag-decision logic. The filesystem/network side effects
 * (read config, reserve_trial, atomic write) are injected by the caller as
 * already-resolved values, matching the seams used by the differential oracle.
 */
#include "noc_w12_onboarding_37e376.h"

json_t *w12_onboarding_37e376_explicit_run(const json_t *section)
{
    const json_t *value = NULL;
    if (section && section->type == JSON_OBJECT)
        value = json_obj_get(section, "run");
    if (value && value->type == JSON_BOOL)
        return json_bool(value->bool_val);
    return json_null();
}

bool w12_onboarding_37e376_should_run(const json_t *explicit, bool assume_fresh, bool fresh)
{
    if (explicit && explicit->type == JSON_BOOL)
        return explicit->bool_val;   /* value is not None: return it verbatim */
    return assume_fresh || fresh;
}

bool w12_onboarding_37e376_latch(const json_t *explicit, bool fresh)
{
    /* if _explicit_run() is not None or not probe().fresh: return False */
    if (explicit && explicit->type == JSON_BOOL)
        return false;
    if (!fresh)
        return false;
    return true;                     /* explicit is None and the machine is fresh */
}
