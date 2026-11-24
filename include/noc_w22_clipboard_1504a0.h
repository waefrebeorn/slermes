#ifndef NOC_W22_CLIPBOARD_1504A0_H
#define NOC_W22_CLIPBOARD_1504A0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/clipboard.py (gate w22_clipboard_1504a0).
 *
 * `build_clipboard_parser` only mutates an argparse tree: it touches no
 * gateway, session store or clipboard tool, so the tree itself is the ported
 * behaviour. The oracle hands the live function a recording subparsers object
 * and compares the recorded add_parser/add_subparsers/add_argument/
 * set_defaults calls with the tree built here.
 *
 * Shape (both sides):
 *   {"parsers": [
 *      {"name": "clipboard",
 *       "help": "...", "description": "...",          (only when not None)
 *       "subparsers": {"dest": "clipboard_action", "required": true},
 *       "arguments": [],
 *       "children": [ {"name": "image", "help": ..., "description": ...,
 *                      "arguments": [{"names": ["--out"], "required": true,
 *                                     "metavar": "PATH", "help": "File to write"},
 *                                    {"names": ["--json"], "action": "store_true",
 *                                     "help": "Print one JSON object on stdout"}],
 *                      "defaults": {"func": "_cmd_clipboard"}} ]} ]}
 */

/* PoP: w22_clipboard_1504a0_parser_tree @ hermes_cli/subcommands/clipboard.py:build_clipboard_parser
 * The subcommand tree attached to `subparsers`, with no live argparse needed. */
json_t *w22_clipboard_1504a0_parser_tree(void);

/* PoP: w22_clipboard_1504a0_cmd_clipboard @ hermes_cli/subcommands/clipboard.py:_cmd_clipboard
 * The handler is a lazy import that forwards the parsed namespace verbatim:
 *   {"delegated_to": "hermes_cli.clipboard_cmd.cmd_clipboard",
 *    "args": <namespace>, "same": true}
 * `same` records that the exact object was handed on unmodified. */
json_t *w22_clipboard_1504a0_cmd_clipboard(const json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_CLIPBOARD_1504A0_H */
