#ifndef PORT_AGENT_REASONING_TIMEOUTS_H
#define PORT_AGENT_REASONING_TIMEOUTS_H

/* C port of agent/reasoning_timeouts.py — pure stale-timeout floor resolver. */

/* Return the floor (seconds) for the first matching reasoning-model slug,
 * or -1.0 if none. Faithful to _match_any(). */
double reasoning_timeouts_match_any(const char *model_lower);

/* Resolve the stale-timeout floor for a (possibly aggregator-prefixed)
 * model slug. Returns the floor in seconds, or -1.0 when the model is not
 * in the allowlist / empty / not a string. Faithful to
 * get_reasoning_stale_timeout_floor() (None -> -1.0). */
double reasoning_timeouts_get_floor(const char *model);

#endif /* PORT_AGENT_REASONING_TIMEOUTS_H */
