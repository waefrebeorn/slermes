/* noc_w19_summarywri_99693d.h - public C API for plugins/platforms/teams/summary_writer.py port. */
#ifndef NOC_W19_SUMMARYWRI_99693D_H
#define NOC_W19_SUMMARYWRI_99693D_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_summarywri_99693d_parse_bool @ plugins/platforms/teams/summary_writer.py:_parse_bool
 * bool passthrough; string "1/true/yes/on" -> 1, "0/false/no/off" -> 0; anything else -> default. */
int w19_summarywri_99693d_parse_bool(json_t *value, int default_value);

/* PoP: w19_summarywri_99693d_text @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._text
 * str(value or "").strip() or default. Returns a malloc'd string (caller frees). */
char *w19_summarywri_99693d_text(json_t *value, const char *fallback);

/* PoP: w19_summarywri_99693d_bullet_lines @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._bullet_lines
 * ["- " + str(item).strip() for item with text] or ["- None"]. */
json_t *w19_summarywri_99693d_bullet_lines(json_t *values);

/* PoP: w19_summarywri_99693d_title @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._title
 * payload.title, else the meeting-id heading, else the fallback heading. malloc'd. */
char *w19_summarywri_99693d_title(json_t *payload);

/* PoP: w19_summarywri_99693d_render_markdown @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._render_summary_markdown
 * The markdown summary body. malloc'd. */
char *w19_summarywri_99693d_render_markdown(json_t *payload);

/* PoP: w19_summarywri_99693d_render_html @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._render_summary_html
 * The Graph html summary body (html.escape applied). malloc'd. */
char *w19_summarywri_99693d_render_html(json_t *payload);

/* PoP: w19_summarywri_99693d_resolve_delivery_config @ plugins/platforms/teams/summary_writer.py:TeamsSummaryWriter._resolve_delivery_config
 * Merge platform extra / token / home channel, then config, then the TEAMS_* env fallbacks.
 * platform_cfg: {"extra":{}, "token":"", "home_channel_chat_id":""} or NULL for no platform config.
 * env: the environment map the module reads (TEAMS_DELIVERY_MODE ...). */
json_t *w19_summarywri_99693d_resolve_delivery_config(json_t *platform_cfg, json_t *config,
                                                     json_t *env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SUMMARYWRI_99693D_H */
