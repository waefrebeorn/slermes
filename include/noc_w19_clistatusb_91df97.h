#ifndef NOC_W19_CLISTATUSB_91DF97_H
#define NOC_W19_CLISTATUSB_91DF97_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_clistatusb_91df97_threshold_style @ hermes_cli/cli_status_bar_mixin.py:_threshold_style
 * ladder is [[bound, name], ...] descending; value is a JSON number. */
const char *w19_clistatusb_91df97_threshold_style(double value, const struct json_t *ladder,
                                                  const char *fallback);

/* PoP: w19_clistatusb_91df97_finite @ hermes_cli/cli_status_bar_mixin.py:_finite
 * Copy of v, or json_null() for None / NaN / negative / > 1e6. */
struct json_t *w19_clistatusb_91df97_finite(const struct json_t *v);

/* PoP: w19_clistatusb_91df97_context_style @ hermes_cli/cli_status_bar_mixin.py:_status_bar_context_style */
const char *w19_clistatusb_91df97_context_style(const struct json_t *percent_used);

/* PoP: w19_clistatusb_91df97_cache_style @ hermes_cli/cli_status_bar_mixin.py:_cache_hit_rate_style */
const char *w19_clistatusb_91df97_cache_style(double cache_pct);

/* PoP: w19_clistatusb_91df97_battery_style @ hermes_cli/cli_status_bar_mixin.py:_battery_status_style */
const char *w19_clistatusb_91df97_battery_style(const char *category);

/* PoP: w19_clistatusb_91df97_compression_style @ hermes_cli/cli_status_bar_mixin.py:_compression_count_style */
const char *w19_clistatusb_91df97_compression_style(double count);

/* PoP: w19_clistatusb_91df97_context_bar @ hermes_cli/cli_status_bar_mixin.py:_build_context_bar
 * Caller frees the returned malloc'd string. */
char *w19_clistatusb_91df97_context_bar(const struct json_t *percent_used, long long width);

/* PoP: w19_clistatusb_91df97_prompt_elapsed @
 * hermes_cli/cli_status_bar_mixin.py:_format_prompt_elapsed
 * `now` is the injected time.time() seam. Caller frees. */
char *w19_clistatusb_91df97_prompt_elapsed(const struct json_t *prompt_start_time,
                                           double prompt_duration, int live, double now);

/* PoP: w19_clistatusb_91df97_trim_text @ hermes_cli/cli_status_bar_mixin.py:_trim_status_bar_text
 * ASCII cell width (one column per byte). Caller frees. */
char *w19_clistatusb_91df97_trim_text(const char *text, long long max_width);

/* PoP: w19_clistatusb_91df97_title_badge @ hermes_cli/cli_status_bar_mixin.py:_status_title_badge
 * [badge, left_width] or json_null(). */
struct json_t *w19_clistatusb_91df97_title_badge(const char *title, long long width);

/* PoP: w19_clistatusb_91df97_right_align @ hermes_cli/cli_status_bar_mixin.py:_right_align_status_title
 * Caller frees. */
char *w19_clistatusb_91df97_right_align(const char *text, const char *title, long long width);

/* PoP: w19_clistatusb_91df97_minimal_chrome @ hermes_cli/cli_status_bar_mixin.py:_use_minimal_tui_chrome */
int w19_clistatusb_91df97_minimal_chrome(long long width);

/* PoP: w19_clistatusb_91df97_scrollback_width @ hermes_cli/cli_status_bar_mixin.py:_scrollback_box_width */
long long w19_clistatusb_91df97_scrollback_width(const struct json_t *width);

/* PoP: w19_clistatusb_91df97_cache_hit_rate @ hermes_cli/cli_status_bar_mixin.py:_cache_hit_rate
 * [cache_pct, label] or json_null(). */
struct json_t *w19_clistatusb_91df97_cache_hit_rate(const struct json_t *snapshot, long long precision);

/* PoP: w19_clistatusb_91df97_goal_segment @ hermes_cli/cli_status_bar_mixin.py:_status_bar_goal_segment
 * Caller frees. */
char *w19_clistatusb_91df97_goal_segment(const struct json_t *snapshot);

/* PoP: w19_clistatusb_91df97_stash_age @ hermes_cli/cli_status_bar_mixin.py:_fmt_stash_age
 * `now` is the injected time.monotonic() seam. Caller frees. */
char *w19_clistatusb_91df97_stash_age(double stashed_at, double now);

/* PoP: w19_clistatusb_91df97_field_set @ hermes_cli/cli_status_bar_mixin.py:_get_status_bar_field_set
 * Sorted string array, or json_null() for the "not customized" None. */
struct json_t *w19_clistatusb_91df97_field_set(const struct json_t *cli_config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CLISTATUSB_91DF97_H */
