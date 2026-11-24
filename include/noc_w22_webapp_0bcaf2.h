#ifndef NOC_W22_WEBAPP_0BCAF2_H
#define NOC_W22_WEBAPP_0BCAF2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/webapp.py (gate w22_webapp_0bcaf2).
 *
 * build_webapp_parser registers the `hermes webapp` subcommand on an argparse
 * subparser collection and nothing else -- the handler is injected by the
 * caller. Everything it *decides* is the parser contract: which flags exist,
 * their dests, their help text, and which three are mutually exclusive. That
 * contract is data here, plus the argparse acceptance rules that follow from
 * it (prefix abbreviations, duplicates, unknown/ambiguous tokens, explicit
 * `--flag=value`, and the mutually-exclusive conflict message).
 *
 *   parser.add_argument("--no-open", action="store_true", help=...)
 *   parser.add_argument("--force-build", action="store_true", help=...)
 *   parser.add_argument("--skip-build", action="store_true", help=...)
 *   action = parser.add_mutually_exclusive_group()
 *   action.add_argument("--build-only" | "--status" | "--sign-out", store_true)
 *   parser.set_defaults(func=cmd_webapp)
 */

/* PoP: w22_webapp_0bcaf2_spec @ hermes_cli/subcommands/webapp.py:build_webapp_parser
 * The registered parser contract:
 *   {"prog_suffix": "webapp",
 *    "description": "...",
 *    "options": [{"flags": ["--no-open"], "dest": "no_open",
 *                 "action": "store_true", "default": false, "help": "..."}],
 *    "exclusive": [["--build-only", "--status", "--sign-out"]],
 *    "func": "cmd_webapp"}
 * Options appear in registration order. */
json_t *w22_webapp_0bcaf2_spec(void);

/* PoP: w22_webapp_0bcaf2_parse @ hermes_cli/subcommands/webapp.py:build_webapp_parser
 * One parse of the registered subcommand against argv (flags only, no values):
 *   {"ok": true, "values": {"no_open": false, ...}, "func": true}
 *   {"ok": false, "error": "argument --status: not allowed with argument --build-only", "exit": 2}
 *   {"ok": false, "error": "help", "exit": 0}          (-h/--help)
 * Errors carry argparse's own text, without the "hermes webapp: error: " prefix. */
json_t *w22_webapp_0bcaf2_parse(const json_t *flags);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_WEBAPP_0BCAF2_H */
