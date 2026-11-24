#ifndef NOC_W22_JOURNEY_B8F31D_H
#define NOC_W22_JOURNEY_B8F31D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/journey.py (gate w22_journey_b8f31d).
 *
 * build_journey_parser attaches one argparse subcommand (with the learning /
 * memory-graph aliases) and delegates the argument wiring to
 * hermes_cli.journey.register_cli, logging (and swallowing) any failure. The
 * parser object stays upstream; the declarative spec and the guard's outcome
 * are ported.
 */

/* PoP: w22_journey_b8f31d_parser_spec @ hermes_cli/subcommands/journey.py:build_journey_parser
 * add_parser("journey", aliases=[...], help=..., description=...), rendered as
 * data: {"name","aliases":[...],"help","description"}. */
json_t *w22_journey_b8f31d_parser_spec(void);

/* PoP: w22_journey_b8f31d_wiring_outcome @ hermes_cli/subcommands/journey.py:build_journey_parser
 * The `try: register_cli(parser) / except Exception as _exc: log.debug(...)` guard.
 * `exc` is {"type": <class name>, "message": <str(exc)>} or null/absent for a
 * clean registration. `except Exception` swallows every Exception subclass but
 * lets BaseException, SystemExit, KeyboardInterrupt and GeneratorExit through:
 *   clean   -> {"wired": true, "log": null}
 *   caught  -> {"wired": false, "log": {"logger":"hermes_cli.main","level":"DEBUG",
 *               "message":"journey CLI wiring failed: <message>"}}
 *   escapes -> {"wired": false, "propagated": <type>, "log": null} */
json_t *w22_journey_b8f31d_wiring_outcome(const json_t *exc);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_JOURNEY_B8F31D_H */
