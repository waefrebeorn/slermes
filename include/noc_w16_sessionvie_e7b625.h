#ifndef NOC_W16_SESSIONVIE_E7B625_H
#define NOC_W16_SESSIONVIE_E7B625_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionvie_e7b625_seconds @ gateway/session_viewer_close.py:_seconds */
double w16_sessionvie_e7b625_seconds(const struct json_t *raw, double default_value);
/* PoP: w16_sessionvie_e7b625_section @ gateway/session_viewer_close.py:_section */
struct json_t *w16_sessionvie_e7b625_section(const struct json_t *config, const char *key);
/* PoP: w16_sessionvie_e7b625_pending_reason @ gateway/session_viewer_close.py:_PendingClose.from_record */
struct json_t *w16_sessionvie_e7b625_pending_reason(const char *record);
/* PoP: w16_sessionvie_e7b625_is_reset_reason @ gateway/session_viewer_close.py:_RESET_REASONS */
int w16_sessionvie_e7b625_is_reset_reason(const char *reason);
/* PoP: w16_sessionvie_e7b625_is_orphan_reason @ gateway/session_viewer_close.py:ORPHAN_REASONS */
int w16_sessionvie_e7b625_is_orphan_reason(const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONVIE_E7B625_H */
