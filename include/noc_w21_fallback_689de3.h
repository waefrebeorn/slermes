#ifndef NOC_W21_FALLBACK_689DE3_H
#define NOC_W21_FALLBACK_689DE3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/fallback.py (gate w21_fallback_689de3).
 *
 * `build_fallback_parser` is a registration function: its whole contract is the
 * parser tree it attaches (`hermes fallback {list,add,remove,clear,check}`),
 * the aliases argparse must accept, and the `func` default it hands dispatch.
 * argparse itself is the CLI seam, so the port carries the tree as data:
 *
 *   {"prog": "fallback", "help": ..., "description": ...,
 *    "subparsers": {"dest": "fallback_command", "choices": [
 *        {"name": "list", "aliases": ["ls"], "help": ..., "arguments": []},
 *        ...,
 *        {"name": "check", "aliases": [], "help": ...,
 *         "arguments": [{"flags": ["--live"], "action": "store_true", "help": ...},
 *                       {"flags": ["--json"], "action": "store_true", "help": ...}]}]},
 *    "defaults": {"func": "cmd_fallback"}}
 */

/* PoP: w21_fallback_689de3_parser_spec @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 * The attached parser tree, in registration order. Fresh node, caller frees. */
json_t *w21_fallback_689de3_parser_spec(void);

/* PoP: w21_fallback_689de3_subcommand @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 * The one choice argparse resolves for *name*, canonical name or alias ("ls"
 * and "rm" included); json_null() when no choice accepts it. */
json_t *w21_fallback_689de3_subcommand(const char *name);

/* PoP: w21_fallback_689de3_default_func @ hermes_cli/subcommands/fallback.py:build_fallback_parser
 * The name of the `func` default the parser dispatches to. */
const char *w21_fallback_689de3_default_func(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_FALLBACK_689DE3_H */
