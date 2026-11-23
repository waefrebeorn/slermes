#ifndef NOC_W13_AGENTFACTO_542E88_H
#define NOC_W13_AGENTFACTO_542E88_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_agentfacto_542e88_parse_model_config @ tui_gateway/agent_factory.py:_parse_model_config */
struct json_t *w13_agentfacto_542e88_parse_model_config(const struct json_t *raw, int quiet);

/* PoP: w13_agentfacto_542e88_row_follows_profile @ tui_gateway/agent_factory.py:_row_follows_profile */
int w13_agentfacto_542e88_row_follows_profile(const struct json_t *row);

/* PoP: w13_agentfacto_542e88_display_mouse_tracking @ tui_gateway/agent_factory.py:_display_mouse_tracking */
const char *w13_agentfacto_542e88_display_mouse_tracking(const struct json_t *display);

/* PoP: w13_agentfacto_542e88_coerce_statusbar @ tui_gateway/agent_factory.py:_coerce_statusbar */
const char *w13_agentfacto_542e88_coerce_statusbar(const struct json_t *raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_AGENTFACTO_542E88_H */
