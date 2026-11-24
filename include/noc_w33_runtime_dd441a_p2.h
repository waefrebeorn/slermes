/*
 * noc_w33_runtime_dd441a_p2.h - C surface for upstream
 * persistence/authority/runtime.py, gate w33_runtime_dd441a_p2.
 *
 * The command-parsing half of the session runtime authority: the closed
 * MutationCommand union validation (``parse_mutation``) with every command's
 * wire ``payload()`` shape, plus the three predicates it is built from
 * (``_reasoning_level``, ``_editor_fingerprint``, ``_GENERATION_REQUIRED``) and
 * the metadata field table.  Rows, epochs and transactions are seams.
 */
#ifndef NOC_W33_RUNTIME_DD441A_P2_H
#define NOC_W33_RUNTIME_DD441A_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W33_RUNTIME_DD441A_P2_METADATA_FIELDS 5
#define W33_RUNTIME_DD441A_P2_REASONING_EFFORTS 7
#define W33_RUNTIME_DD441A_P2_GENERATION_REQUIRED 6
#define W33_RUNTIME_DD441A_P2_COMPRESS_FIELDS 4

/* (operation, payload) -> {"ok": true, "command": <class>, "payload": {...}}
 * or {"ok": false, "error": "invalid_params"}. */
json_t *w33_runtime_dd441a_p2_parse_mutation(const json_t *operation, const json_t *payload);

/* A string level ``parse_reasoning_effort`` accepts -> true/false. */
json_t *w33_runtime_dd441a_p2_reasoning_level(const json_t *value);

/* {"fingerprint": str} and nothing else -> the string, else null. */
json_t *w33_runtime_dd441a_p2_editor_fingerprint(const json_t *value);

/* Whether the operation's mutation requires ``expected_generation``. */
json_t *w33_runtime_dd441a_p2_generation_required(const json_t *operation);

/* {"title": "str", "archived": "bool", ...} */
json_t *w33_runtime_dd441a_p2_metadata_fields(void);

/* The module's constant tables: reasoning efforts, generation-required
 * operations, the compress field types and the metadata field types. */
json_t *w33_runtime_dd441a_p2_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W33_RUNTIME_DD441A_P2_H */
