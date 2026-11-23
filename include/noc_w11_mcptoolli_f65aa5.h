#ifndef NOC_W11_MCPTOOLLI_F65AA5_H
#define NOC_W11_MCPTOOLLI_F65AA5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_mcptoolli_f65aa5_filter_children @ tools/mcp_tool_lifecycle.py:_filter_mcp_children
 * cmdlines: pid-decimal-string -> argv array. Returns sorted kept pids (strings). */
struct json_t *w11_mcptoolli_f65aa5_filter_children(const struct json_t *pids,
        const struct json_t *cmdlines, int psutil_available,
        const char *supervisor_script);

/* PoP: w11_mcptoolli_f65aa5_take_reapable @ tools/mcp_tool_lifecycle.py:_take_reapable_pids
 * Returns {"ret":[pids,pgids,starts],"state":{...mutated ledgers...}}.
 * All pid/pgid/start numbers are decimal strings (JSON numbers are double). */
struct json_t *w11_mcptoolli_f65aa5_take_reapable(int include_active,
        const struct json_t *server_name,
        const struct json_t *orphan_pids, const struct json_t *orphan_servers,
        const struct json_t *stdio_pids, const struct json_t *stdio_pgids,
        const struct json_t *stdio_starttimes);

/* PoP: w11_mcptoolli_f65aa5_clear_cooldowns @ tools/mcp_tool_lifecycle.py:_clear_connect_cooldowns
 * keys null => clear all; else array of keys. Returns mutated maps. */
struct json_t *w11_mcptoolli_f65aa5_clear_cooldowns(const struct json_t *keys,
        const struct json_t *retry_after, const struct json_t *failures);

/* PoP: w11_mcptoolli_f65aa5_stop_loop_if_idle @ tools/mcp_tool_lifecycle.py:_stop_mcp_loop_if_idle */
int w11_mcptoolli_f65aa5_stop_loop_if_idle(int loop_result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_MCPTOOLLI_F65AA5_H */
