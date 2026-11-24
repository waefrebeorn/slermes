/*
 * noc_w24_messagessq_1dd421.h - C surface for upstream
 * persistence/sqlite/conversation/_store/messages_sql.py (gate w24_messagessq_1dd421).
 *
 * The statement/column layer of the message functions: the literal transcript
 * statements, the write-column table the INSERT derives (and the schema keys it
 * unions), the timestamp coercion every write passes through, and the
 * tool_calls normalisers.  The connection and the write itself stay seams.
 */
#ifndef W24_MESSAGESSQ_1DD421_H
#define W24_MESSAGESSQ_1DD421_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The INSERT's column list: \w+ over the text between the first "(" and ")". */
json_t *w24_messagessq_1dd421_write_columns(void);

/* set(write_columns) | {"id","compacted","display_order"}, sorted. */
json_t *w24_messagessq_1dd421_schema_keys(void);

/* req: {"value": any, "default": number} -> _coerce_timestamp(value, default). */
json_t *w24_messagessq_1dd421_coerce_ts(const json_t *req);

/* tool_calls: list | str (JSON text) -> the parsed value, [] on bad JSON. */
json_t *w24_messagessq_1dd421_parse_tool_calls(const json_t *tool_calls);

/* 0 for None, len for a list, 1 for any other value. */
json_t *w24_messagessq_1dd421_tool_calls_count(const json_t *tool_calls);

/* len(parse(raw)) for a list, else (scalar if parsed else 0). */
json_t *w24_messagessq_1dd421_tool_calls_len(const json_t *raw, double scalar);

/* Every literal statement and clause this module owns. */
json_t *w24_messagessq_1dd421_constants(void);

#ifdef __cplusplus
}
#endif
#endif
