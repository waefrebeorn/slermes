#ifndef NOC_W6_LEGACYCLI_B0CE00_H
#define NOC_W6_LEGACYCLI_B0CE00_H

/* Port of upstream Hermes agent/legacy_cli.py::main / _build_parser argument
 * resolution. `argv_json` is a JSON array of argument strings (the caller's
 * argv[1:]). Returns a JSON object describing the resolved call:
 *   {"error":true}                    -- argparse rejected the arguments
 *   {"called":false}                  -- no query and no --list-tools: help only
 *   {"called":true,"query":<str|null>,"model":...,"api_key":...,"base_url":...,
 *    "max_turns":<int>,"enabled_toolsets":...,"disabled_toolsets":...,
 *    "list_tools":<bool>,"save_trajectories":<bool>,"save_sample":<bool>,
 *    "verbose":<bool>,"log_prefix_chars":<int>}
 * Caller owns the returned node.
 */
#include "hermes_json.h"

json_t *w6_legacycli_b0ce00_resolve(const char *argv_json);

#endif /* NOC_W6_LEGACYCLI_B0CE00_H */
