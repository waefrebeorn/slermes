/*
 * noc_w21_codexrunti_5cdf58.h -- public C API for the C11 port of upstream Hermes
 *   hermes_cli/subcommands/codex_runtime.py
 *
 * `hermes codex-runtime migrate` prints either MigrationReport.summary() or the
 * dataclass payload as JSON, and exits 1 when the report carries errors. Both
 * projections are ported here; the migration itself
 * (hermes_cli.codex_runtime_plugin_migration.migrate) stays upstream -- it
 * reads config and rewrites ~/.codex/config.toml.
 *
 * Every exported symbol carries the w21_codexrunti_5cdf58_ prefix.
 */
#ifndef NOC_W21_CODEXRUNTI_5CDF58_H
#define NOC_W21_CODEXRUNTI_5CDF58_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * cmd_codex_runtime_migrate(). state:
 *   {"dry_run":bool, "json":bool,
 *    "report":{"target_path":str|null,"migrated":[str],"skipped_keys_per_server":{name:[str]},
 *              "migrated_plugins":[str],"plugin_query_error":str|null,
 *              "wrote_permissions_default":str|null,"preserved_user_servers":[str],
 *              "errors":[str],"written":bool,"dry_run":bool}}
 * -> {"rc":int,"text":str} or {"rc":int,"payload":object}
 */
json_t *w21_codexrunti_5cdf58_migrate(const json_t *state);

/* MigrationReport.summary() for the same report object; caller frees. */
char *w21_codexrunti_5cdf58_summary(const json_t *report);

/*
 * build_codex_runtime_parser() table:
 *   {"dest":"codex_runtime_action","choices":["migrate"],"aliases":{},
 *    "helps":{"migrate":...},"func":"_print_help",
 *    "subs":{"migrate":{"func":"cmd_codex_runtime_migrate","options":[
 *        {"options":["--dry-run"],"dest":"dry_run","action":"_StoreTrueAction",
 *         "default":false,"help":...}, ...]}}}
 */
json_t *w21_codexrunti_5cdf58_parser(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_CODEXRUNTI_5CDF58_H */
