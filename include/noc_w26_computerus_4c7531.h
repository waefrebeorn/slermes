/*
 * noc_w26_computerus_4c7531.h - C surface for upstream
 * hermes_cli/subcommands/computer_use.py (gate w26_computerus_4c7531).
 *
 * The pure decision core of ``hermes computer-use``: the plugin-backend gate
 * that decides whether the cua-driver checks apply at all, the non-JSON
 * permissions report, and the argparse command surface itself (a literal
 * table).  The provider lookup, the driver probe and stdout are seams: the
 * caller injects what they returned.
 */
#ifndef NOC_W26_COMPUTERUS_4C7531_H
#define NOC_W26_COMPUTERUS_4C7531_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {configured_backend, default_backend, lookup_error, doctor,
 *       provider_rc, available, name, display_name} ->
 * {"exit": bool, "code": int|null, "lines": [printed line, ...]}. */
json_t *w26_computerus_4c7531_backend_gate(const json_t *req);

/* req: {"st": {...}, "hint": str|null} -> the human-readable permissions
 * report: {"exit": bool, "code": int|null, "lines": [...]}. */
json_t *w26_computerus_4c7531_perms_status(const json_t *req);

/* The ``hermes computer-use`` argparse tree: name/help/options per node. */
json_t *w26_computerus_4c7531_surface(void);

#ifdef __cplusplus
}
#endif
#endif
