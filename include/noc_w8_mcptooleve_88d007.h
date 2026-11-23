#ifndef NOC_W8_MCPTOOLEVE_88D007_H
#define NOC_W8_MCPTOOLEVE_88D007_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_mcptooleve_88d007_events_enabled @ tools/mcp_tool_events.py:events_enabled */
int w8_mcptooleve_88d007_events_enabled(const struct json_t *config);

/* PoP: w8_mcptooleve_88d007_callback_url @ tools/mcp_tool_events.py:_callback_url
 * Returns a malloc'd string; caller frees. */
char *w8_mcptooleve_88d007_callback_url(const char *url_base, const char *route_name);

/* PoP: w8_mcptooleve_88d007_event_prompt @ tools/mcp_tool_events.py:_event_prompt
 * Literal {eventId}/{timestamp}/{data} placeholders are preserved verbatim.
 * Returns a malloc'd string; caller frees. */
char *w8_mcptooleve_88d007_event_prompt(const char *server_name, const char *name,
                                        const char *instructions);

/* PoP: w8_mcptooleve_88d007_server_routes @ tools/mcp_tool_events.py:_server_routes
 * Routes whose mcp_event.server equals server_name. Returns object (caller frees). */
struct json_t *w8_mcptooleve_88d007_server_routes(const struct json_t *subscriptions,
                                                  const char *server_name);

/* PoP: w8_mcptooleve_88d007_subscribe_args_error @ tools/mcp_tool_events.py:_subscribe_args_error
 * Pre-approval validation error string (caller frees), or NULL when valid. */
char *w8_mcptooleve_88d007_subscribe_args_error(const struct json_t *args);

/* PoP: w8_mcptooleve_88d007_subscribe_params @ tools/mcp_tool_events.py:_subscribe_params
 * events/subscribe params. Returns object (caller frees). */
struct json_t *w8_mcptooleve_88d007_subscribe_params(const char *route_name,
        const struct json_t *route, const char *url_base, const struct json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_MCPTOOLEVE_88D007_H */
