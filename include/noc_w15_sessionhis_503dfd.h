#ifndef NOC_W15_SESSIONHIS_503DFD_H
#define NOC_W15_SESSIONHIS_503DFD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_sessionhis_503dfd_history_part_image_url @ tui_gateway/session_history.py:_history_part_image_url */
struct json_t *w15_sessionhis_503dfd_history_part_image_url(const struct json_t *part);
/* PoP: w15_sessionhis_503dfd_history_dict_text @ tui_gateway/session_history.py:_history_dict_text */
struct json_t *w15_sessionhis_503dfd_history_dict_text(const struct json_t *content, int image_urls);
/* PoP: w15_sessionhis_503dfd_content_display_text @ tui_gateway/session_history.py:_content_display_text */
struct json_t *w15_sessionhis_503dfd_content_display_text(const struct json_t *content);
/* PoP: w15_sessionhis_503dfd_coerce_message_text @ tui_gateway/session_history.py:_coerce_message_text */
struct json_t *w15_sessionhis_503dfd_coerce_message_text(const struct json_t *content, int image_urls);
/* PoP: w15_sessionhis_503dfd_is_display_hidden_marker @ tui_gateway/session_history.py:_is_display_hidden_marker */
struct json_t *w15_sessionhis_503dfd_is_display_hidden_marker(const char *role, const char *text);
/* PoP: w15_sessionhis_503dfd_strip_trailing_screenshot_placeholders @ tui_gateway/session_history.py:_strip_trailing_screenshot_placeholders */
struct json_t *w15_sessionhis_503dfd_strip_trailing_screenshot_placeholders(const char *text);
/* PoP: w15_sessionhis_503dfd_coerce_seed_history @ tui_gateway/session_history.py:_coerce_seed_history */
struct json_t *w15_sessionhis_503dfd_coerce_seed_history(const struct json_t *value);
/* PoP: w15_sessionhis_503dfd_inflight_text @ tui_gateway/session_history.py:_inflight_text */
struct json_t *w15_sessionhis_503dfd_inflight_text(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_SESSIONHIS_503DFD_H */
