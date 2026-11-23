#ifndef NOC_W8_MICROCOMPAC_666E99_H
#define NOC_W8_MICROCOMPAC_666E99_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* args: {entry, cc{summary_key,micro_key,...}} */
int w8_microcompac_666e99_is_summary_marker(const struct json_t *args);
int w8_microcompac_666e99_is_micro_marker(const struct json_t *args);

/* args: {content, cc{heading,end_marker}} */
struct json_t *w8_microcompac_666e99_rolling_summary_from_marker(const struct json_t *args);

/* args: {summary_text, cc{summary_prefix,heading,end_marker}} */
struct json_t *w8_microcompac_666e99_render_micro_marker_content(const struct json_t *args);

/* args: {exchange_start, exchange_end, state, cc{max_failures}} */
struct json_t *w8_microcompac_666e99_record_micro_failure(const struct json_t *args);

/* state -> {consecutive, last_cursor} after reset */
struct json_t *w8_microcompac_666e99_reset_micro_failure_tracking(const struct json_t *state);

/* args: {messages, start, tail_start} -> [a,b] or null */
struct json_t *w8_microcompac_666e99_find_one_exchange(const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_MICROCOMPAC_666E99_H */
