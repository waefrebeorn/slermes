/*
 * compaction_events.c — C home for upstream Hermes agent/compaction_events.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: agent_compaction_events_pending @ agent/compaction_events.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const agent_compaction_events_CONST_SCHEMA_NAME = "hermes.compaction";
static const char *const agent_compaction_events_CONST_SCHEMA_VERSION = "1";
static const char *const agent_compaction_events_CONST_COMPACTION_MARK = "compaction";
static const char *const agent_compaction_events_CONST_ATTEMPT_MARK = "compaction.attempt";

void agent_compaction_events_port_pending(void) { (void)0; }
