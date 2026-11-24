#ifndef NOC_W22_CURATOR_233719_H
#define NOC_W22_CURATOR_233719_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/curator.py (gate w22_curator_233719).
 *
 * build_curator_parser attaches one argparse subcommand and delegates the
 * argument wiring to hermes_cli.curator.register_cli, logging (and swallowing)
 * any failure. The parser object stays upstream; what is ported is the
 * declarative spec the parser is created from and the guard's outcome.
 */

/* PoP: w22_curator_233719_parser_spec @ hermes_cli/subcommands/curator.py:build_curator_parser
 * add_parser("curator", help=..., description=...), rendered as data:
 * {"name","aliases":[],"help","description"}. */
json_t *w22_curator_233719_parser_spec(void);

/* PoP: w22_curator_233719_wiring_outcome @ hermes_cli/subcommands/curator.py:build_curator_parser
 * The `try: register_cli(parser) / except Exception as _exc: log.debug(...)` guard.
 * `exc` is {"type": <class name>, "message": <str(exc)>} or null/absent for a
 * clean registration. `except Exception` swallows every Exception subclass but
 * lets BaseException, SystemExit, KeyboardInterrupt and GeneratorExit through:
 *   clean   -> {"wired": true, "log": null}
 *   caught  -> {"wired": false, "log": {"logger":"hermes_cli.main","level":"DEBUG",
 *               "message":"curator CLI wiring failed: <message>"}}
 *   escapes -> {"wired": false, "propagated": <type>, "log": null} */
json_t *w22_curator_233719_wiring_outcome(const json_t *exc);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_CURATOR_233719_H */
