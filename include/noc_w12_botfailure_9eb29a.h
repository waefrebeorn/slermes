#ifndef NOC_W12_BOTFAILURE_9EB29A_H
#define NOC_W12_BOTFAILURE_9EB29A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_botfailure_9eb29a_retry_action @ tools/bot_failure_reasons.py:retry_action */
char *w12_botfailure_9eb29a_retry_action(const char *reason);

/* PoP: w12_botfailure_9eb29a_turn_failure_text @ tools/bot_failure_reasons.py:turn_failure_text
 * NULL streams are skipped; returns malloc'd joined text. */
char *w12_botfailure_9eb29a_turn_failure_text(const char *stdout_text, const char *stderr_text);

/* PoP: w12_botfailure_9eb29a_classify_agent_error @ tools/bot_failure_reasons.py:classify_agent_error
 * Returns malloc'd reason code. */
char *w12_botfailure_9eb29a_classify_agent_error(const char *text);

/* PoP: w12_botfailure_9eb29a_result_retry_action @ tools/bot_failure_reasons.py:result_retry_action
 * result: a JSON object (an _run_agent result dict). Returns malloc'd retry action. */
char *w12_botfailure_9eb29a_result_retry_action(const struct json_t *result);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_BOTFAILURE_9EB29A_H */
