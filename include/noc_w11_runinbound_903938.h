#ifndef NOC_W11_RUNINBOUND_903938_H
#define NOC_W11_RUNINBOUND_903938_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_runinbound_903938_expand_alias @ gateway/run_inbound_hooks.py:GatewayInboundHooksMixin._hm_expand_alias_quick_command
 * Faithful port of the alias quick-command rewrite. Returns a NEW object
 * {"command": <name-or-null>, "text": <rewritten event.text>}.
 * `qcmd` is the quick-command dict; `command_args` is event.get_command_args();
 * `event_text` is the original event.text (kept when no target is found).
 * Python str.strip / lstrip("/") / split() semantics on ASCII whitespace. */
struct json_t *w11_runinbound_903938_expand_alias(const struct json_t *qcmd,
                                                   const char *command_args,
                                                   const char *event_text);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_RUNINBOUND_903938_H */
