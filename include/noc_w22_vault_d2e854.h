#ifndef NOC_W22_VAULT_D2E854_H
#define NOC_W22_VAULT_D2E854_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/vault.py (gate w22_vault_d2e854).
 *
 * The module is the `hermes vault` subcommand's parser spec plus its mount
 * order. `hermes_cli.vault.register_cli` (which adds add/list/rm) is the seam:
 * the caller reports whether it ran, and the spec/mount decisions live here.
 */

/* PoP: w22_vault_d2e854_spec @ hermes_cli/subcommands/vault.py:build_vault_parser
 * name / help / description exactly as add_parser is given them, plus the
 * defaults func name and the number of register_cli calls the builder makes. */
json_t *w22_vault_d2e854_spec(void);

/* PoP: w22_vault_d2e854_mount @ hermes_cli/subcommands/vault.py:build_vault_parser
 * `subparsers.add_parser("vault", ...)` on a parent whose prog is *parent_prog*
 * and whose subparsers already hold *taken* (an array of names). A name already
 * taken fails the way argparse does: ValueError "conflicting subparser: vault".
 * Success returns {"ok": true, "prog": "<parent> vault", "name", "help",
 * "description", "defaults_func", "register_calls"}. */
json_t *w22_vault_d2e854_mount(const char *parent_prog, const json_t *taken);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_VAULT_D2E854_H */
