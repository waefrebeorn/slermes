#ifndef NOC_W22_PETS_B45CC9_H
#define NOC_W22_PETS_B45CC9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/subcommands/pets.py (gate w22_pets_b45cc9).
 *
 * The module attaches the ``pets`` subcommand to the root parser and then
 * delegates the flag surface to ``hermes_cli.pets.register_cli`` inside a
 * guard: any failure is swallowed with one debug record and the root parser
 * still gets its subcommand.  The guard body needs the live argparse
 * subparsers object and the hermes_cli.pets module, so the port exposes the
 * declaration it hands to ``add_parser`` and the outcome of the guarded wiring.
 */

/* PoP: w22_pets_b45cc9_spec @ hermes_cli/subcommands/pets.py:build_pets_parser
 * The keyword arguments handed to ``subparsers.add_parser("pets", ...)``:
 * {"name": "pets", "help": ..., "description": ...} -- verbatim text. */
json_t *w22_pets_b45cc9_spec(void);

/* PoP: w22_pets_b45cc9_build @ hermes_cli/subcommands/pets.py:build_pets_parser
 * Runs the guarded wiring against a scripted bridge:
 *   req = {"register": "ok"|"error", "message": <str>}   (absent -> "ok")
 * ``register == "error"`` means the import-or-call raised; its ``str()`` is
 * logged as ``"pets CLI wiring failed: %s"`` at DEBUG on logger
 * ``hermes_cli.main`` and nothing propagates.
 *
 * Answer: {"name","help","description",
 *          "registered": bool, "log": <formatted record>|null}. */
json_t *w22_pets_b45cc9_build(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_PETS_B45CC9_H */
