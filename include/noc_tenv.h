#ifndef NOC_TENV_H
#define NOC_TENV_H
#include <stdint.h>

/* Tolerant env-var knob parsing (tui_gateway/_env.py).
 * PoP: tenv_env_int @ tui_gateway/_env.py:env_int
 * PoP: tenv_env_float @ tui_gateway/_env.py:env_float
 * PoP: tenv_env_number @ tui_gateway/_env.py:_env_number  (internal helper)
 */
int64_t tenv_env_int(const char *name, int64_t default_val);
double  tenv_env_float(const char *name, double default_val);

#endif /* NOC_TENV_H */
