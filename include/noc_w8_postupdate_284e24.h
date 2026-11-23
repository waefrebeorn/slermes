#ifndef NOC_W8_POSTUPDATE_284E24_H
#define NOC_W8_POSTUPDATE_284E24_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/post_update.py (gate w8_postupdate_284e24).
 *
 * PoP: w8_postupdate_run_steps_ex @ hermes_cli/post_update.py:run_steps
 *
 * Faithful port of run_steps(steps): run each (name, func) in order; a step
 * that raises contributes {"ok": False, "error": str(exc)} while a step that
 * returns contributes its value verbatim, keyed by name.  Steps are injected
 * as JSON objects {"name": str, "out": <value>} or {"name": str, "raise": str};
 * the LIVE oracle builds matching callables, so no filesystem or PM work is
 * needed.  Returns a JSON object mapping name -> result.
 */
json_t *w8_postupdate_run_steps_ex(const json_t *steps);

#endif /* NOC_W8_POSTUPDATE_284E24_H */
