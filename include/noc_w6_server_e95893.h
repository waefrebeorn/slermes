#ifndef NOC_W6_SERVER_E95893_H
#define NOC_W6_SERVER_E95893_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_server_e95893_flatten_history_text @ acp_adapter/server.py:_flatten_history_text
 * Returns a new JSON string (caller frees). */
struct json_t *w6_server_e95893_flatten_history_text(const struct json_t *value);

/* PoP: w6_server_e95893_history_reasoning_text @ acp_adapter/server.py:_history_reasoning_text
 * Returns a new JSON string (caller frees). */
struct json_t *w6_server_e95893_history_reasoning_text(const struct json_t *message);

/* PoP: w6_server_e95893_content_refused_notice @ acp_adapter/server.py:_content_refused_notice
 * Returns a new JSON string or JSON null (caller frees). */
struct json_t *w6_server_e95893_content_refused_notice(const struct json_t *message);

/* PoP: w6_server_e95893_attach_interrupted_prompt @ acp_adapter/server.py:_attach_interrupted_prompt
 * NULL strings are treated as empty. Returns a new JSON string (caller frees). */
struct json_t *w6_server_e95893_attach_interrupted_prompt(const char *interrupted,
                                                         const char *guidance);

/* PoP: w6_server_e95893_history_tool_call_name_args @ acp_adapter/server.py:_history_tool_call_name_args
 * Returns a new JSON array [name, args-dict] (caller frees). */
struct json_t *w6_server_e95893_history_tool_call_name_args(const struct json_t *tool_call);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_SERVER_E95893_H */
