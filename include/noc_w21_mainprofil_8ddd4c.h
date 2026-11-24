/*
 * noc_w21_mainprofil_8ddd4c.h -- public C API for the port of upstream
 *   hermes_cli/main_profile_recovery.py
 *
 * `_apply_profile_override` fails closed when the sticky `active_profile`
 * names a deleted profile.  Only the escape hatches may still reach their
 * handler in the default home: `profile list`, `profile use default`, and an
 * `uninstall` that removes no user data.
 */
#ifndef NOC_W21_MAINPROFIL_8DDD4C_H
#define NOC_W21_MAINPROFIL_8DDD4C_H

#include "hermes_json.h"

/*
 * command_argv(argv) -> array
 *
 * The subcommand and its arguments, excluding top-level flags and their
 * values -- the same scan hermes_cli._parser.command_argv performs against
 * build_top_level_parser()'s value-taking flags.  Stops at the first
 * non-flag token, or after a bare "--".
 */
json_t *w21_mainprofil_8ddd4c_command_argv(const json_t *argv);

/*
 * is_stale_profile_recovery_command(argv) -> bool
 *
 * 1 for `profile list`, `profile use default` (case-insensitive), or an
 * `uninstall ...` whose parsed namespace sets neither --data nor --full.
 * The uninstall args go through the real subparser's option surface, so
 * abbreviations ("--dat", "--fu") resolve and `--help` (exit 0) passes, while
 * an ambiguous prefix, a mutually-exclusive clash, or a missing value fails
 * closed.
 */
int w21_mainprofil_8ddd4c_is_stale_profile_recovery_command(const json_t *argv);

#endif /* NOC_W21_MAINPROFIL_8DDD4C_H */
