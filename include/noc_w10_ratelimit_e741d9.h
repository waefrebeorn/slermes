#ifndef NOC_W10_RATELIMIT_E741D9_H
#define NOC_W10_RATELIMIT_E741D9_H

#include <hermes_json.h>

/* C11 ports of agent/rate_limit_credits.py.  Every symbol is prefixed
 * w10_ratelimit_e741d9_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t *.  Detection / config results normally obtained from
 * utils / hermes_cli.config are injected through the args object. */
json_t *w10_ratelimit_e741d9_is_nous_agent(json_t *a);
json_t *w10_ratelimit_e741d9_pct(json_t *a);
json_t *w10_ratelimit_e741d9_credits_spent_micros(json_t *a);
json_t *w10_ratelimit_e741d9_notices_enabled(json_t *a);
json_t *w10_ratelimit_e741d9_openrouter_cache_status(json_t *a);

#endif /* NOC_W10_RATELIMIT_E741D9_H */
