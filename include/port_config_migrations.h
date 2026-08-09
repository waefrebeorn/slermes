/*
 * port_config_migrations.h — C11 port of hermes_cli/config_migrations.py
 */
#ifndef PORT_CONFIG_MIGRATIONS_H
#define PORT_CONFIG_MIGRATIONS_H

#include <stdbool.h>
#include "hermes_json.h"

/* PoP: support_floor_message @ hermes_cli/config_migrations.py:support_floor_message */
char *cm_support_floor_message(void);

/* PoP: _migrate_to_12 @ hermes_cli/config_migrations.py:_migrate_to_12 */
void cm_migrate_to_12(json_t *results, bool quiet);
/* PoP: _migrate_to_13 @ hermes_cli/config_migrations.py:_migrate_to_13 */
void cm_migrate_to_13(json_t *results, bool quiet);
/* PoP: _migrate_to_14 @ hermes_cli/config_migrations.py:_migrate_to_14 */
void cm_migrate_to_14(json_t *results, bool quiet);
/* PoP: _migrate_to_15 @ hermes_cli/config_migrations.py:_migrate_to_15 */
void cm_migrate_to_15(json_t *results, bool quiet);
/* PoP: _migrate_to_16 @ hermes_cli/config_migrations.py:_migrate_to_16 */
void cm_migrate_to_16(json_t *results, bool quiet);
/* PoP: _migrate_to_17 @ hermes_cli/config_migrations.py:_migrate_to_17 */
void cm_migrate_to_17(json_t *results, bool quiet);
/* PoP: _migrate_to_21 @ hermes_cli/config_migrations.py:_migrate_to_21 */
void cm_migrate_to_21(json_t *results, bool quiet);
/* PoP: _migrate_to_23 @ hermes_cli/config_migrations.py:_migrate_to_23 */
void cm_migrate_to_23(json_t *results, bool quiet);
/* PoP: _migrate_to_25 @ hermes_cli/config_migrations.py:_migrate_to_25 */
void cm_migrate_to_25(json_t *results, bool quiet);
/* PoP: _migrate_to_29 @ hermes_cli/config_migrations.py:_migrate_to_29 */
void cm_migrate_to_29(json_t *results, bool quiet);
/* PoP: _migrate_to_31 @ hermes_cli/config_migrations.py:_migrate_to_31 */
void cm_migrate_to_31(json_t *results, bool quiet);
/* PoP: _migrate_to_32 @ hermes_cli/config_migrations.py:_migrate_to_32 */
void cm_migrate_to_32(json_t *results, bool quiet);
/* PoP: _migrate_to_33 @ hermes_cli/config_migrations.py:_migrate_to_33 */
void cm_migrate_to_33(json_t *results, bool quiet);

/* PoP: run_migrations @ hermes_cli/config_migrations.py:run_migrations */
void cm_run_migrations(int current_ver, json_t *results, bool quiet);

#endif /* PORT_CONFIG_MIGRATIONS_H */
