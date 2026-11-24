/*
 * noc_w33_portabilit_b54bb9.h - C surface for upstream
 * persistence/sqlite/housekeeping/_portability.py (gate w33_portabilit_b54bb9).
 *
 * The import/export contract of a session payload, without SQLite: payload
 * validation (ids, message lists, per-session/total ceilings, typed text,
 * integer and JSON columns, timestamps, non-finite numbers), the exported row
 * record, the foreign-origin match, the parent re-attachment plan and the flag
 * mask the row writer derives.  Every seam stays in the caller: the writer
 * connection, the system-prompt hash, the message writer, the clock and the
 * decoder's own error text.
 *
 * Wire shape of every entry point: {"ok":true,"value":...} on a value, or
 * {"ok":false,"kind":"ValueError","error":"..."} for the exception the live
 * function raises.
 */
#ifndef NOC_W33_PORTABILIT_B54BB9_H
#define NOC_W33_PORTABILIT_B54BB9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The budget constants, the column families and the INSERT statement text. */
json_t *w33_portabilit_b54bb9_constants(void);

/* The contract record of one session row dict: the 66 export columns required
 * in the row plus last_active/_reset_from/_branched_from when present. */
json_t *w33_portabilit_b54bb9_export_row(const json_t *session);

/* Column coercers: text or null, int or null, int inside SQLite's range or the
 * default, finite float or null, dict of JSON-backed column text. */
json_t *w33_portabilit_b54bb9_text_or_none(const json_t *value, const char *field);
json_t *w33_portabilit_b54bb9_int_or_none(const json_t *value, const char *field);
json_t *w33_portabilit_b54bb9_int_or_bounded(const json_t *value, const char *field,
                                            const json_t *fallback);
json_t *w33_portabilit_b54bb9_float_or_finite(const json_t *value, const char *field);
json_t *w33_portabilit_b54bb9_json_object_or_none(const json_t *value, const char *field);

/* First path of a NaN/inf number anywhere in a decoded value, else null. */
json_t *w33_portabilit_b54bb9_non_finite_path(const json_t *value, const char *path);

/* Decode JSON text, refusing a non-finite number anywhere in it. */
json_t *w33_portabilit_b54bb9_load_json(const char *text, const char *path);

/* Scalar message content that SQLite can bind and a serializer can emit. */
json_t *w33_portabilit_b54bb9_message_content(const json_t *content, const char *path);

/* One payload message / session: type-checked copies, ValueError naming the
 * offending field. */
json_t *w33_portabilit_b54bb9_normalize_message(const json_t *message, int index);
json_t *w33_portabilit_b54bb9_normalize_session(const json_t *raw, const char *session_id,
                                               const json_t *messages);

/* The whole payload -> [normalized_items, errors]; every rejected entry is
 * reported and no entry is written. */
json_t *w33_portabilit_b54bb9_validate_payload(const json_t *sessions);

/* The id of the session already imported from `origin`, else null. */
json_t *w33_portabilit_b54bb9_foreign_match(const json_t *rows, const json_t *origin);

/* [kept, detached] for the parent edges of an import; only a closing cycle edge
 * is dropped. */
json_t *w33_portabilit_b54bb9_parent_attach(const json_t *parent_updates,
                                           const json_t *store_rows);

/* The archived/auto_archived/pinned/hidden mask of one imported row. */
json_t *w33_portabilit_b54bb9_import_flags(const json_t *raw);

/* Live vs archived split of the imported messages, with each archived row's
 * compacted flag. */
json_t *w33_portabilit_b54bb9_message_split(const json_t *messages);

#ifdef __cplusplus
}
#endif
#endif
