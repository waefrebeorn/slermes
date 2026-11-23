#ifndef NOC_W12_SETUPTERMI_48D5EA_H
#define NOC_W12_SETUPTERMI_48D5EA_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_setuptermi_48d5ea_report_binary @ hermes_cli/setup_terminal.py:_report_binary
 * Models the two report calls as an ordered JSON array of
 * {"kind":"info"|"warning","text":...}.  `found_prefix` is always supplied. */
struct json_t *w12_setuptermi_48d5ea_report_binary(const char *found,
                                                   const char *missing,
                                                   const char *install_hint,
                                                   const char *found_prefix);

/* PoP: w12_setuptermi_48d5ea_existing_secret_keeps @ hermes_cli/setup_terminal.py:_existing_secret_keeps
 * env_value / prompt_answer are injected seams for get_env_value() and
 * prompt_yes_no().  Returns {"keeps":bool,"info":string|null}. */
struct json_t *w12_setuptermi_48d5ea_existing_secret_keeps(const char *env_value,
                                                           const char *label,
                                                           const char *question,
                                                           int prompt_answer);

/* PoP: w12_setuptermi_48d5ea_prompt_secret_env @ hermes_cli/setup_terminal.py:_prompt_secret_env
 * `value` is the injected prompt() answer.  Returns the ordered array of
 * save_env_value / print_success calls the wizard would make. */
struct json_t *w12_setuptermi_48d5ea_prompt_secret_env(const char *value,
                                                       const char *label,
                                                       const char *env_var,
                                                       const char *confirm_msg);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SETUPTERMI_48D5EA_H */
