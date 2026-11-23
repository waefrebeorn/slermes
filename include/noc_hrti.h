#ifndef NOC_HRTI_H
#define NOC_HRTI_H
#include "hermes_json.h"
/* PoP: hrti_validate_task_input @ gateway/hosted_room_task_input.py:validate_task_input */
#define HRTI_MAX_TASK_INPUT_EVENTS 128
/* Returns NULL on success (*out = new json_t object), else the exact ValueError
 * message from the live validator.  int64 fields arrive as decimal strings. */
const char *hrti_validate_task_input(const json_t *value, json_t **out);
/* PoP: hrti_max_task_input_events @ gateway/hosted_room_task_input.py:MAX_TASK_INPUT_EVENTS */
int hrti_max_task_input_events(void);
#endif
