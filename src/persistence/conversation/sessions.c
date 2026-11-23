/*
 * sessions.c — C home for upstream Hermes persistence/conversation/sessions.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: persistence_conversation_sessions_pending @ persistence/conversation/sessions.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const persistence_conversation_sessions_CONST_END_REASON_COMPRESSION = "compression";
static const char *const persistence_conversation_sessions_CONST_END_REASON_SESSION_RESET = "session_reset";
static const char *const persistence_conversation_sessions_CONST_END_REASON_ADOPTED = "adopted_by_profile";
static const char *const persistence_conversation_sessions_CONST_END_REASON_ORPHANED_COMPRESSION = "orphaned_compression";
static long long persistence_conversation_sessions_CONST_MAX_TITLE_LENGTH = 100;
static const char *const persistence_conversation_sessions_CONST_CANONICAL_BOT_CHAT_TITLE = "Bot Chat";
static const char *const persistence_conversation_sessions_CONST_LOCAL_CHAT_PREFIX = "local-";
static const char *const persistence_conversation_sessions_CONST_RETAG_GATE_PREFIX = "kanban_worker_source_retagged:";
static const char *const persistence_conversation_sessions_CONST_RETAG_SOURCE_FROM = "cli";
static const char *const persistence_conversation_sessions_CONST_RETAG_SOURCE_TO = "kanban";
static const char *const persistence_conversation_sessions_CONST_RUNTIME_LOCK_KEY = "browser_model_lock";
static const char *const persistence_conversation_sessions_CONST_YOLO_KEY = "yolo_mode";
static const char *const persistence_conversation_sessions_CONST_ONBOARDING_RUN_KEY = "onboarding_run";
static const char *const persistence_conversation_sessions_CONST_SKILL_SCAFFOLD_PREFIX = "[IMPORTANT: The user has invoked the ";
static const char *const persistence_conversation_sessions_CONST_AUTO_LOAD_SCAFFOLD_PREFIX = "[IMPORTANT: The \"";
static long long persistence_conversation_sessions_CONST_RECENT_BROWSE_CANDIDATE_CAP = 2048;
static long long persistence_conversation_sessions_CONST_RECENT_BROWSE_LINEAGE_CAP = 8192;
static long long persistence_conversation_sessions_CONST_ACTIVITY_DESCRIPTION_MAX = 120;
static const char *const persistence_conversation_sessions_CONST_ACTIVITY_PROVENANCE_UNKNOWN = "unknown";

void persistence_conversation_sessions_port_pending(void) { (void)0; }
