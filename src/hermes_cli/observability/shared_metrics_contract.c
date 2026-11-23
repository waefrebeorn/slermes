/*
 * shared_metrics_contract.c — C home for upstream Hermes hermes_cli/observability/shared_metrics_contract.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_observability_shared_metrics_contract_pending @ hermes_cli/observability/shared_metrics_contract.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SCHEMA_KEY = "hermes.metrics.schema_version";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_COMMIT_TICKET_KEY = "hermes.shared_metrics.commit_ticket";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SCHEMA_VERSION = "hermes.metrics.event.v4";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_CALL_SCOPE = "hermes.model_call";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_CALL_PROFILE_MODEL = "unknown";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TASK_SCOPE = "hermes.task_run";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_CALL_SCOPE = "hermes.tool_call";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CLIENT_ACTIVE_MARK = "hermes.client.active";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_APPROVAL_MARK = "hermes.tool_approval";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SKILL_LIFECYCLE_MARK = "hermes.skill.lifecycle";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SKILL_LOAD_MARK = "hermes.skill.load";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_INSTALL_SNAPSHOT_MARK = "hermes.install.snapshot";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SESSION_MARK = "hermes.session";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SETUP_COMPLETED_MARK = "hermes.setup.completed";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_TOKENS_MARK = "hermes.model_tokens";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_COMPRESSION_MARK = "hermes.compression";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_SWITCH_MARK = "hermes.model_switch";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FALLBACK_MARK = "hermes.fallback";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SLASH_COMMAND_MARK = "hermes.slash_command";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_EXTENSION_INSTALL_MARK = "hermes.extension.install";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_STARTUP_LATENCY_MARK = "hermes.startup.latency";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SUBSCRIBER_NAME = "hermes.nemo_relay.shared_metrics";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CLIENT_ACTIVE_METRIC = "hermes.client.active";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_LEGACY_MODEL_CALL_METRIC = "hermes.model_call.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_ROUTE_METRIC = "hermes.model_route.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TASK_STARTED_METRIC = "hermes.task_run.started";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TASK_FINISHED_METRIC = "hermes.task_run.finished";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TASK_DURATION_METRIC = "hermes.task_run.duration";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_CALL_METRIC = "hermes.tool_call.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_LATENCY_METRIC = "hermes.tool_call.latency";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_APPROVAL_METRIC = "hermes.tool_approval.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SKILL_LIFECYCLE_METRIC = "hermes.skill.lifecycle.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SKILL_LOAD_METRIC = "hermes.skill.load.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_USAGE_METRIC = "hermes.tool.usage.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_INSTALL_SNAPSHOT_METRIC = "hermes.install.snapshot";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SESSION_METRIC = "hermes.session.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MILESTONE_METRIC = "hermes.install.milestone";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SETUP_COMPLETED_METRIC = "hermes.setup.completed";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_TOKENS_METRIC = "hermes.model_tokens.sum";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_COMPRESSION_METRIC = "hermes.compression.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_SWITCH_METRIC = "hermes.model_switch.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FALLBACK_METRIC = "hermes.fallback.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_SLASH_COMMAND_METRIC = "hermes.slash_command.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_EXTENSION_INSTALL_METRIC = "hermes.extension.install.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MEMORY_OP_MARK = "hermes.memory.op";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CURATOR_RUN_MARK = "hermes.curator.run";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_DELEGATION_RUN_MARK = "hermes.delegation.run";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_EXECUTION_BACKEND_MARK = "hermes.execution_backend";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MEMORY_OP_METRIC = "hermes.memory.op.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CURATOR_RUN_METRIC = "hermes.curator.run.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_DELEGATION_RUN_METRIC = "hermes.delegation.run.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_EXECUTION_BACKEND_METRIC = "hermes.execution_backend.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_TOOL_QUALITY_MARK = "hermes.model_tool_quality";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_FRICTION_MARK = "hermes.model_friction";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CONTEXT_PEAK_MARK = "hermes.context_peak";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_TOOL_QUALITY_METRIC = "hermes.model_tool_quality.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_FRICTION_METRIC = "hermes.model_friction.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_CONTEXT_PEAK_METRIC = "hermes.context_peak.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_STARTUP_LATENCY_METRIC = "hermes.startup.latency";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_ENGAGEMENT_TURN_MARK = "hermes.engagement.turn";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_ENGAGEMENT_DAY_METRIC = "hermes.engagement.day.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_ENGAGEMENT_SURFACE_METRIC = "hermes.engagement.surface_day.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_SWITCH_AFTER_MARK = "hermes.model_switch_after";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_SWITCH_AFTER_METRIC = "hermes.model_switch_after.count";
static long long hermes_cli_observability_shared_metrics_contract_CONST_MODEL_IDENTIFIER_MAX_LENGTH = 256;
static long long hermes_cli_observability_shared_metrics_contract_CONST_PROVIDER_IDENTIFIER_MAX_LENGTH = 64;
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FEATURE_ADOPTION_METRIC = "hermes.feature_adoption.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FEATURE_USED_MARK = "hermes.feature.used";
static long long hermes_cli_observability_shared_metrics_contract_CONST_FEATURE_DISABLED_NAME_MAX_LENGTH = 64;
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FILE_EDIT_MARK = "hermes.file_edit";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_LOOP_GUARD_MARK = "hermes.loop_guard";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_RECOVERY_MARK = "hermes.tool_recovery";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TERMINAL_OUTCOME_MARK = "hermes.terminal.outcome";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_REPLY_ISSUE_MARK = "hermes.model_reply_issue";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_FILE_EDIT_METRIC = "hermes.file_edit.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_LOOP_GUARD_METRIC = "hermes.loop_guard.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TOOL_RECOVERY_METRIC = "hermes.tool_recovery.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_TERMINAL_OUTCOME_METRIC = "hermes.terminal.outcome.count";
static const char *const hermes_cli_observability_shared_metrics_contract_CONST_MODEL_REPLY_ISSUE_METRIC = "hermes.model_reply_issue.count";
static long long hermes_cli_observability_shared_metrics_contract_CONST_DESKTOP_SETTING_KEY_MAX_LENGTH = 96;

void hermes_cli_observability_shared_metrics_contract_port_pending(void) { (void)0; }
