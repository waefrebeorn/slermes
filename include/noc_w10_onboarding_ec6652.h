#ifndef NOC_W10_ONBOARDING_EC6652_H
#define NOC_W10_ONBOARDING_EC6652_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_onboarding_ec6652_builds_onboarding @ agent/onboarding_session.py:builds_onboarding */
int w10_onboarding_ec6652_builds_onboarding(const struct json_t *valid_tool_names);

/* PoP: w10_onboarding_ec6652_keeps_toolsets @ agent/onboarding_session.py:keeps_toolsets */
int w10_onboarding_ec6652_keeps_toolsets(const struct json_t *enabled);

/* PoP: w10_onboarding_ec6652_with_onboarding @ agent/onboarding_session.py:with_onboarding
 * `all_toolsets`/`session_platforms` stand in for the toolsets package when enabled is null. */
struct json_t *w10_onboarding_ec6652_with_onboarding(const struct json_t *enabled,
                                                     const struct json_t *all_toolsets,
                                                     const struct json_t *session_platforms);

/* PoP: w10_onboarding_ec6652_carry_onboarding_toolset @ agent/onboarding_session.py:carry_onboarding_toolset */
struct json_t *w10_onboarding_ec6652_carry_onboarding_toolset(const struct json_t *enabled,
                                                              const struct json_t *agent_toolsets);

/* PoP: w10_onboarding_ec6652_append_turn_note @ agent/onboarding_session.py:append_turn_note
 * Returns the post-call note value; caller frees. */
char *w10_onboarding_ec6652_append_turn_note(const char *prior, const char *note);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_ONBOARDING_EC6652_H */
