#ifndef NOC_W21_BROWSER_30D956_H
#define NOC_W21_BROWSER_30D956_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/browser.py (gate w21_browser_30d956).
 *
 * The module wires one subcommand (`browser`) with two actions and one
 * dispatch closure. The parser shape and the dispatch decision are both ported;
 * the three calls that touch the machine -- default-browser detection, the
 * profile directory and terminating the process tree -- stay behind the
 * caller's inputs (the OS/process seam), and `run_export_signins` is a
 * passthrough return code.
 */

/* PoP: w21_browser_30d956_parser_shape @ hermes_cli/subcommands/browser.py:build_browser_parser
 * The attached parser's observable shape:
 *   {"subcommands":[...], "close_options":[...], "export_options":[...],
 *    "export_hidden":bool, "has_dispatch":bool}
 * Options exclude the implicit -h/--help. `export_hidden` is
 * `help=argparse.SUPPRESS` on the export-signins subparser. */
json_t *w21_browser_30d956_parser_shape(void);

/* PoP: w21_browser_30d956_dispatch @ hermes_cli/subcommands/browser.py:_dispatch_browser
 * The dispatch closure over one parsed argv, with the OS calls supplied:
 *   req = {"action":<browser_action|null>, "browser":<--browser|null>,
 *          "detected":<detect_default_chromium()>, "profile":{browser:dir|null},
 *          "closed":bool, "msg":<str>, "export_rc":<int>}
 * Returns {"rc":<n>,"stdout":<...>,"stderr":<...>}; the parser-help branch
 * (unknown/absent action) reports stdout as "<HELP>" because the usage text is
 * argparse's, not this module's. */
json_t *w21_browser_30d956_dispatch(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_BROWSER_30D956_H */
