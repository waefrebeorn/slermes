#ifndef NOC_PROFCFG_H
#define NOC_PROFCFG_H
/*
 * Public C API for the port of hermes_cli/profiles_config.py (gate profcfg).
 * Symbol is module-prefixed to avoid collisions.
 *
 * LIMITATION: the live _migrate_profile_config_if_outdated performs filesystem
 * checks/writes, a HERMES_HOME override, lazy plugin imports and in-memory
 * config migration.  A pure C port cannot query that runtime state, so this
 * exposes the honest decision core: given the already-resolved facts
 * (file tooling support, config.yaml presence, current/latest schema version)
 * report whether migrate_config would be attempted.  No side effects are
 * fabricated and no profile directories are touched.
 */
int profcfg_should_migrate(int supports_file_tooling, int config_exists,
                           int current_ver, int latest_ver);

#endif /* NOC_PROFCFG_H */
