#ifndef NOC_W6_VAULT_9AB785_H
#define NOC_W6_VAULT_9AB785_H
#include <stddef.h>

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_vault_actions @ hermes_cli/vault.py:register_cli
 * Sub-command names registered on the `vault_action` subparser. */
const char *const *w6_vault_actions(size_t *count);

/* PoP: w6_vault_handler @ hermes_cli/vault.py:vault_command
 * Handler function name a given action/subaction dispatches to, or NULL when
 * no _vault_handler default is set (vault_command falls back to _cmd_list). */
const char *w6_vault_handler(const char *action, const char *subaction);

/* PoP: w6_vault_arg_meta @ hermes_cli/vault.py:register_cli
 * argparse descriptor for an option of a given action (path "key/migrate" for
 * nested subcommands). Returns {"choices":..., "default":null,
 * "required":bool, "metavar":...} or JSON null for an unknown option.
 * Caller frees. */
struct json_t *w6_vault_arg_meta(const char *action, const char *opt);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_VAULT_9AB785_H */
