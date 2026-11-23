#ifndef NOC_W13_METHODSSLA_FE7885_H
#define NOC_W13_METHODSSLA_FE7885_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_methodssla_fe7885_format_live_model_output @ tui_gateway/methods_slash.py:_format_live_model_output
 * `agent` is a JSON object; absent keys mirror getattr(..., ""). */
struct json_t *w13_methodssla_fe7885_format_live_model_output(const struct json_t *agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_METHODSSLA_FE7885_H */
