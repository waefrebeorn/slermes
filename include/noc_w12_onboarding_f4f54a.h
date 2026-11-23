#ifndef NOC_W12_ONBOARDING_F4F54A_H
#define NOC_W12_ONBOARDING_F4F54A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_onboarding_f4f54a_reset @ hermes_cli/onboarding_due.py:reset_for_tests */
void w12_onboarding_f4f54a_reset(void);

/* PoP: w12_onboarding_f4f54a_invalidate @ hermes_cli/onboarding_due.py:invalidate */
void w12_onboarding_f4f54a_invalidate(void);

/* PoP: w12_onboarding_f4f54a_store @ hermes_cli/onboarding_due.py:_store
 * generation is an int64 decimal string fed through strtoll by the harness. */
int w12_onboarding_f4f54a_store(const struct json_t *value, long long generation,
                                const char *before, const char *after);

/* PoP: w12_onboarding_f4f54a_read @ hermes_cli/onboarding_due.py:read
 * Mirrors read() with compute() returning *value and stamp() yielding before then after. */
struct json_t *w12_onboarding_f4f54a_read(const struct json_t *value,
                                          const char *before, const char *after);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_ONBOARDING_F4F54A_H */
