/*
 * noc_w24_pause_6e1faf.h - C surface for upstream
 * hermes_cli/subcommands/pause.py (gate w24_pause_6e1faf).
 *
 * The emergency-stop subcommand's value layer: the two printed reports
 * (pause/resume) and the argparse surface (help, description, --reason
 * default).  The sentinel file itself, the estop state and process exit stay
 * seams: they arrive as inputs, never as I/O.
 */
#ifndef W24_PAUSE_6E1FAF_H
#define W24_PAUSE_6E1FAF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"reason": any|null, "engaged": bool, "state": obj|null, "path": str}
 * -> {"text": <everything cmd_pause prints>, "rc": 0} */
json_t *w24_pause_6e1faf_pause_output(const json_t *req);

/* req: {"disengaged": bool, "path": str}
 * -> {"text": <everything cmd_resume prints>, "rc": 0} */
json_t *w24_pause_6e1faf_resume_output(const json_t *req);

/* req: {"which": "pause"|"resume", "field": "help"|"description"|"reason_default"|"all"}
 * -> the field value, or {"help","description","reason_default"} for "all" */
json_t *w24_pause_6e1faf_parser_spec(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
