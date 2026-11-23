/* Public C API for the agent/auxiliary_hooks.py parity port. */
#ifndef NOC_W7_AUXILIARYHO_D1A407_H
#define NOC_W7_AUXILIARYHO_D1A407_H
#include "hermes_json.h"

json_t *w7_auxiliaryho_d1a407_constants(void);
/* turn: null or {"lease":{session_id,platform},"task_id":...,"turn_id":...} */
json_t *w7_auxiliaryho_d1a407_parent_turn_identity(json_t *turn);
json_t *w7_auxiliaryho_d1a407_system_prompt(json_t *messages, json_t *kwargs);
json_t *w7_auxiliaryho_d1a407_first_choice_message(json_t *response);
json_t *w7_auxiliaryho_d1a407_field(json_t *obj, const char *name);
json_t *w7_auxiliaryho_d1a407_message_count(json_t *messages, json_t *input_value);
json_t *w7_auxiliaryho_d1a407_retry_counts(json_t *raw);
#endif /* NOC_W7_AUXILIARYHO_D1A407_H */
