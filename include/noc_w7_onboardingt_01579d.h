#ifndef NOC_W7_ONBOARDINT_01579D_H
#define NOC_W7_ONBOARDINT_01579D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: onboardingt_ui_language @ agent/onboarding_text.py:ui_language */
struct json_t *onboardingt_ui_language(const char *locale);
/* PoP: onboardingt_greeting @ agent/onboarding_text.py:greeting */
struct json_t *onboardingt_greeting(const char *locale);
/* PoP: onboardingt_question @ agent/onboarding_text.py:question */
struct json_t *onboardingt_question(const char *beat, const char *locale);
/* PoP: onboardingt_names @ agent/onboarding_text.py:_names */
struct json_t *onboardingt_names(const struct json_t *values);
/* PoP: onboardingt_facts_block @ agent/onboarding_text.py:facts_block */
struct json_t *onboardingt_facts_block(const struct json_t *facts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_ONBOARDINT_01579D_H */
