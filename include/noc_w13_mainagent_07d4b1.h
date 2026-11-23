#ifndef NOC_W13_MAINAGENT_07D4B1_H
#define NOC_W13_MAINAGENT_07D4B1_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_mainagent_07d4b1_dict_or_empty @ hermes_cli/main_agent_cmds.py:_dict_or_empty */
struct json_t *w13_mainagent_07d4b1_dict_or_empty(const struct json_t *value);
/* PoP: w13_mainagent_07d4b1_acp_flags @ hermes_cli/main_agent_cmds.py:cmd_acp (_ACP_FLAGS) */
struct json_t *w13_mainagent_07d4b1_acp_flags(const struct json_t *args);
/* PoP: w13_mainagent_07d4b1_acp_env @ hermes_cli/main_agent_cmds.py:cmd_acp (env writes) */
struct json_t *w13_mainagent_07d4b1_acp_env(const struct json_t *args);
/* PoP: w13_mainagent_07d4b1_monitoring_lines @ hermes_cli/main_agent_cmds.py:cmd_monitoring
 * Lines from the status branch; otlp_available stands in for otlp_exporter.is_available(). */
struct json_t *w13_mainagent_07d4b1_monitoring_lines(const struct json_t *mon, int otlp_available);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_MAINAGENT_07D4B1_H */
