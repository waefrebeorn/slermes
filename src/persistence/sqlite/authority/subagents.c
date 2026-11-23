/*
 * subagents.c — C home for upstream Hermes persistence/sqlite/authority/subagents.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: persistence_sqlite_authority_subagents_pending @ persistence/sqlite/authority/subagents.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const persistence_sqlite_authority_subagents_CONST_OWED_SQL = "json_extract(record_json, '$.auto_promised') AND json_extract(record_json, '$.auto_gave_up') IS NULL";
static const char *const persistence_sqlite_authority_subagents_CONST_CANCEL_DEBT = "'$.auto_promised', json('false')";

void persistence_sqlite_authority_subagents_port_pending(void) { (void)0; }
