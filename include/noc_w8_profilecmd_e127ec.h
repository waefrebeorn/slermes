#ifndef NOC_W8_PROFILECMD_E127EC_H
#define NOC_W8_PROFILECMD_E127EC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_profilecmd_e127ec_confirm @ hermes_cli/profile_cmd.py:_confirm
 * The upstream reads an answer via input(); here the already-read answer is
 * injected (EOF / Ctrl-C == ""). True iff answer.strip().lower() in {"y","yes"}. */
int w8_profilecmd_e127ec_confirm(const char *answer);

/* PoP: w8_profilecmd_e127ec_is_active @ hermes_cli/profile_cmd.py:_is_active
 * name == active or (active == "default" and is_default). */
int w8_profilecmd_e127ec_is_active(const char *name, int is_default, const char *active);

/* PoP: w8_profilecmd_e127ec_action_keys @ hermes_cli/profile_cmd.py:PROFILE_ACTIONS
 * Sorted string keys of the dispatch table (the None "bare command" key omitted). */
struct json_t *w8_profilecmd_e127ec_action_keys(void);

/* PoP: w8_profilecmd_e127ec_has_action @ hermes_cli/profile_cmd.py:cmd_profile
 * True iff PROFILE_ACTIONS.get(action) resolves to a handler. */
int w8_profilecmd_e127ec_has_action(const char *action);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_PROFILECMD_E127EC_H */
