/* port_cron_scheduler_preflight.c — cron pre-dispatch validation (preflight).

Port of the following functions from cron/scheduler.py:

  - _cron_preflight_enabled
  - _preflight_check_provider_key
  - _preflight_check_delivery
  - _preflight_check_skills
  - _preflight_job_config

These are the pure-logic preflight checks that run BEFORE any agent
machinery is invoked. They validate:
  1. Whether preflight is enabled in config
  2. Whether a provider key is configured/valid
  3. Whether delivery platform config is valid
  4. Whether skill subscription config is valid
  5. Orchestrate all checks together
*/
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

/* PoP: cron_scheduler_cron_preflight_enabled @ cron/scheduler.py:_cron_preflight_enabled */
/* Whether cron pre-dispatch configuration validation is enabled.
 * Default ON; only the literal boolean false under "cron.preflight" opts
 * out (mirrors cron_model_drift_guard_enabled semantics). */
int cron_scheduler_cron_preflight_enabled(const json_t *cfg)
{
    /* cron_cfg = (cfg or {}).get("cron") */
    if (!cfg || cfg->type != JSON_OBJECT) return 1;
    json_t *cron_cfg = json_obj_get(cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return 1;
    /* cron_cfg.get("preflight", True) is not False */
    json_t *pf = json_obj_get(cron_cfg, "preflight");
    if (pf && pf->type == JSON_BOOL && !pf->bool_val) return 0;
    return 1;
}
