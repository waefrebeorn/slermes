#ifndef NOC_W14_METHODSPRO_1624BE_H
#define NOC_W14_METHODSPRO_1624BE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_methodspro_1624be_message_row_id @ tui_gateway/methods_prompt.py:_message_row_id */
struct json_t *w14_methodspro_1624be_message_row_id(const struct json_t *msg);
/* PoP: w14_methodspro_1624be_row_ids_of @ tui_gateway/methods_prompt.py:_row_ids_of (sorted) */
struct json_t *w14_methodspro_1624be_row_ids_of(const struct json_t *messages);
/* PoP: w14_methodspro_1624be_pdf_page_range @ tui_gateway/methods_prompt.py:_pdf_page_range
 * max_pages is the injected _PDF_ATTACH_MAX_PAGES seam; err shape is {code,error}. */
struct json_t *w14_methodspro_1624be_pdf_page_range(const struct json_t *params, const struct json_t *max_pages);
/* PoP: w14_methodspro_1624be_preview_restart_prompt @ tui_gateway/methods_prompt.py:_preview_restart_prompt
 * note/rules are the injected _PREVIEW_RESTART_HISTORY_NOTE / _PREVIEW_RESTART_RULES seams. */
struct json_t *w14_methodspro_1624be_preview_restart_prompt(const struct json_t *url, const struct json_t *cwd,
                                          const struct json_t *context, const struct json_t *has_history,
                                          const struct json_t *note, const struct json_t *rules);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_METHODSPRO_1624BE_H */
