#ifndef NOC_CONVSCAF_H
#define NOC_CONVSCAF_H
/*
 * Public C API for the port of agent/conversation_compression_scaffolding.py
 * (gate convscaf). All exported symbols are module-prefixed.
 */
#include "hermes_json.h"

/* Faithful port of _message_text; returns a malloc'd JSON string (caller frees). */
json_t *convscaf_message_text(const json_t *message);

/* Faithful ports of the two predicates; return 1/0. */
int convscaf_has_scaffolding_flag(const json_t *message);
int convscaf_is_user_role_scaffolding(const json_t *message);

#endif /* NOC_CONVSCAF_H */
