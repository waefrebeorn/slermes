/*
 * port_webhook_coalesce.h — pure helpers from gateway/platforms/webhook_coalesce.py.
 *
 * validate_coalesce_config, the prompt-with-note text, and the group-key
 * placeholder/prefix logic are portable. The render callback, debounce timer,
 * and dispatch are runtime IO and stay unported.
 */
#ifndef PORT_WEBHOOK_COALESCE_H
#define PORT_WEBHOOK_COALESCE_H

/* PendingEvent.prompt_with_note(): appends the coalescing note when count>1,
 * else returns the prompt unchanged. Returns a malloc'd string, caller frees. */
char *wc_prompt_with_note(const char *prompt, long long count);

/* group_key pure core, called after the site's render produced `rendered_key`
 * for a stripped `key_template`. Returns NULL when an unresolved placeholder
 * `\{[a-zA-Z0-9_.]+\}` remains (entities must not collapse); else a malloc'd
 * "{route_name}|{rendered}". Caller frees. */
char *wc_group_key(const char *route_name, const char *rendered_key);

/* Validate a route's coalesce block (agent-mode only). route_json is the JSON
 * serialisation of the `route` dict. Returns 0 when valid; otherwise a
 * negative code and, when err_msg is non-NULL, a malloc'd message matching the
 * Python ValueError text. Codes: -1 exclusive-conflict, -2 missing key,
 * -3 non-positive numeric field. Caller frees err_msg. */
int wc_validate_coalesce_config(const char *route_name, const char *route_json,
                                char **err_msg);

#endif /* PORT_WEBHOOK_COALESCE_H */
