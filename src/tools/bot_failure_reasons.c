/*
 * bot_failure_reasons.c — C home for upstream Hermes tools/bot_failure_reasons.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: tools_bot_failure_reasons_pending @ tools/bot_failure_reasons.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const tools_bot_failure_reasons_CONST_RUNTIME_OFFLINE = "runtime_offline";
static const char *const tools_bot_failure_reasons_CONST_QUEUED_EXPIRED = "queued_expired";
static const char *const tools_bot_failure_reasons_CONST_DELIVERY_TIMEOUT = "delivery_timeout";
static const char *const tools_bot_failure_reasons_CONST_AGENT_BLOCKED = "agent_blocked";
static const char *const tools_bot_failure_reasons_CONST_CANCELLED = "cancelled";
static const char *const tools_bot_failure_reasons_CONST_PROVIDER_AUTH_OR_ACCESS = "provider_auth_or_access";
static const char *const tools_bot_failure_reasons_CONST_PROVIDER_QUOTA_LIMIT = "provider_quota_limit";
static const char *const tools_bot_failure_reasons_CONST_PROVIDER_RATE_LIMIT = "provider_rate_limit";
static const char *const tools_bot_failure_reasons_CONST_PROVIDER_SERVER_ERROR = "provider_server_error";
static const char *const tools_bot_failure_reasons_CONST_CONTEXT_OVERFLOW = "context_overflow";
static const char *const tools_bot_failure_reasons_CONST_MISSING_CONFIG = "missing_config";
static const char *const tools_bot_failure_reasons_CONST_MODEL_UNAVAILABLE = "model_unavailable";
static const char *const tools_bot_failure_reasons_CONST_TARGET_SCOPE_UNRESOLVED = "target_scope_unresolved";
static const char *const tools_bot_failure_reasons_CONST_UNKNOWN = "unknown";
static const char *const tools_bot_failure_reasons_CONST_HELD_NOT_SENT = "runtime_coordination_required";
static const char *const tools_bot_failure_reasons_CONST_RETRY_RESUME = "resume";
static const char *const tools_bot_failure_reasons_CONST_RETRY_COMPRESS_THEN_RESUME = "compress_then_resume";
static const char *const tools_bot_failure_reasons_CONST_RETRY_NONE = "none";

void tools_bot_failure_reasons_port_pending(void) { (void)0; }
