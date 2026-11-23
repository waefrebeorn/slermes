/* noc_w12_onboarding_37e376.h — public C API port of hermes_cli/onboarding_run.py */
#ifndef NOC_W12_ONBOARDING_37E376_H
#define NOC_W12_ONBOARDING_37E376_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core of _explicit_run(): given the value of
 * load_user_config_effective(root_config).get("onboarding"), return its bool
 * "run" field when it is a bool, else JSON null (Python None). */
json_t *w12_onboarding_37e376_explicit_run(const json_t *section);

/* should_run(): explicit is the bool|null result of _explicit_run().
 * When explicit is a bool it is returned; otherwise assume_fresh or fresh. */
bool w12_onboarding_37e376_should_run(const json_t *explicit, bool assume_fresh, bool fresh);

/* latch_first_install() decision: explicit bool|null (result of _explicit_run)
 * and fresh flag. Returns whether the run/trial latch would be written. */
bool w12_onboarding_37e376_latch(const json_t *explicit, bool fresh);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_ONBOARDING_37E376_H */
