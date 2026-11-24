/*
 * noc_w23_migrate_73e740.h - C surface for upstream
 * hermes_cli/subcommands/migrate.py (gate w23_migrate_73e740).
 *
 * The `hermes migrate` command surface: which subcommands exist, their help and
 * description text, their declared options in declaration order, and the
 * handler defaults each one sets.
 *
 * Seams (never decided here): argparse itself, the handler callables
 * (cmd_migrate / cmd_migrate_xai / cmd_migrate_agent) and the agent-importers
 * catalogue (IMPORT_SOURCES) whose entries this table mirrors.
 */
#ifndef NOC_W23_MIGRATE_73E740_H
#define NOC_W23_MIGRATE_73E740_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of `hermes migrate <type>` subcommands (2 migrations + 6 agents). */
#define W23_MIGRATE_73E740_SUBCOMMANDS 8

/* path: [] or absent -> the `migrate` parser; [name] -> that subcommand.
 * Returns {help, description, options[], subcommands|none, defaults}, or
 * {"error":"unknown_command"} for an unknown/oversized/ill-typed path. */
json_t *w23_migrate_73e740_surface(const json_t *path);

/* The subcommand names, in the order argparse lists them. */
json_t *w23_migrate_73e740_subcommand_names(void);

#ifdef __cplusplus
}
#endif
#endif
