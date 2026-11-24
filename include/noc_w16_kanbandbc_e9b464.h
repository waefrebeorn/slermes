#ifndef NOC_W16_KANBANDBC_E9B464_H
#define NOC_W16_KANBANDBC_E9B464_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/kanban_db_context.py (gate w16_kanbandbc_e9b464). */

/* _relative_age(ts, now): relative age string for an epoch-seconds value. */
json_t *w16_kanbandbc_e9b464_relative_age(const char *ts_str, const char *now_str);

/* _ctx_cap(s, limit): strip + truncate with a visible ellipsis. */
json_t *w16_kanbandbc_e9b464_ctx_cap(const char *s, int limit);

/* _ctx_metadata_line(metadata): canonical metadata line, or null. */
json_t *w16_kanbandbc_e9b464_ctx_metadata_line(const json_t *metadata);

/* _ctx_tail(items, cap, noun): [kept_items, omitted_note_or_null]. */
json_t *w16_kanbandbc_e9b464_ctx_tail(const json_t *items, int cap, const char *noun);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_KANBANDBC_E9B464_H */
