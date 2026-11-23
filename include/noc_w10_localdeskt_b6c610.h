#ifndef NOC_W10_LOCALDESKT_B6C610_H
#define NOC_W10_LOCALDESKT_B6C610_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_localdeskt_b6c610_theme @ gateway/local_desktop_ui.py:_theme
 * Input keys: "payload", "apply_answer" (injected _ask result), "pick"
 * (injected _ask_result result), "elapsed" (injected monotonic delta).
 * Returns the JSON dict the theme callback hands back. Caller frees. */
struct json_t *w10_localdeskt_b6c610_theme(const struct json_t *in);

/* PoP: w10_localdeskt_b6c610_read_block @ gateway/local_desktop_ui.py:_read_block
 * Input keys: "start", "count". Returns the JSON params dict the block reader
 * passes to _ask, with None-valued keys omitted. Caller frees. */
struct json_t *w10_localdeskt_b6c610_read_block(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_LOCALDESKT_B6C610_H */
