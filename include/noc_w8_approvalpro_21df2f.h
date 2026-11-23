#ifndef NOC_W8_APPROVALPRO_21DF2F_H
#define NOC_W8_APPROVALPRO_21DF2F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_approvalpro_21df2f_consent @ tools/approval_prompt.py:_consent */
const char *w8_approvalpro_21df2f_consent(const char *choice, const char *unresolved);

/* PoP: w8_approvalpro_21df2f_attempt @ tools/approval_prompt.py:_attempt
 * All four fields are optional JSON values (null allowed); caller frees the result. */
struct json_t *w8_approvalpro_21df2f_attempt(const char *name, const struct json_t *choice,
                              const struct json_t *failure, const struct json_t *fallback);

/* PoP: w8_approvalpro_21df2f_unanswered @ tools/approval_prompt.py:Unanswered
 * {"choice":"cancelled","cause":<cause>}; caller frees. */
struct json_t *w8_approvalpro_21df2f_unanswered(const char *cause);

/* PoP: w8_approvalpro_21df2f_cli_decision @ tools/approval_prompt.py:_ask_human
 * once_inputs / deny_inputs are JSON arrays of strings (the i18n split inputs). */
const char *w8_approvalpro_21df2f_cli_decision(const char *choice, int once_only, int allow_permanent,
                                const struct json_t *once_inputs, const struct json_t *deny_inputs);

/* PoP: w8_approvalpro_21df2f_i18n_key @ tools/approval_prompt.py:_CLI_CHOICE_I18N
 * NULL when the decision has no i18n key. */
const char *w8_approvalpro_21df2f_i18n_key(const char *decision);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_APPROVALPRO_21DF2F_H */