#ifndef NOC_W11_TURNRESPON_DF1EEB_H
#define NOC_W11_TURNRESPON_DF1EEB_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_turnrespon_df1eeb_codex_finish_reason @ agent/turn_response_check.py:_codex_finish_reason
 * Returns a newly allocated string ("incomplete" / "content_filter" / "stop"). */
char *w11_turnrespon_df1eeb_codex_finish_reason(
    const json_t *status, const json_t *incomplete_details);

/* PoP: w11_turnrespon_df1eeb_derive_finish_reason @ agent/turn_response_check.py:_derive_finish_reason
 * transport.normalize_response / response_finish_reason and
 * _should_treat_stop_as_truncated are injected as fixture fields. */
char *w11_turnrespon_df1eeb_derive_finish_reason(
    const char *api_mode, const json_t *status, const json_t *incomplete_details,
    int codex_tool_calls, const char *transport_finish, int treat_stop_truncated);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TURNRESPON_DF1EEB_H */
