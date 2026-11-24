/*
 * shared_metrics_drift.c -- real C11 port of the packaging-time conformance of
 * rows an earlier build stored of upstream Hermes
 *   hermes_cli/observability/shared_metrics_drift.py.
 *
 * PoP: w21_sharedmetr_b6c0a5_shape_known @ hermes_cli/observability/shared_metrics_drift.py:counter_dimensions_shape_is_known
 * PoP: w21_sharedmetr_b6c0a5_conform @ hermes_cli/observability/shared_metrics_drift.py:conform_counter_dimensions
 * PoP: w21_sharedmetr_b6c0a5_metric_identifier @ hermes_cli/observability/shared_metrics_contract.py:_metric_identifier
 *
 * The module itself is pure; everything it reads is the shared-metrics product
 * contract (_METRIC_FIELDS, _LEGACY_METRIC_FIELDS, _COUNTER_DIMENSION_VALUES,
 * _IDENTIFIER_FIELDS, _metric_identifier). Those closed tables are mirrored
 * below verbatim from the live contract (generated, not transcribed), so a
 * contract edit upstream shows up as an oracle mismatch rather than as silent
 * drift.
 *
 * Catalog-backed enums (a _CatalogValues, filled from the installed plugin
 * catalog at run time) are the module's one data seam: the caller passes the
 * catalog snapshot it packaged with, exactly as _CatalogValues.values() would.
 */

#include "hermes_json.h"
#include "noc_w21_sharedmetr_b6c0a5.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- generated from hermes_cli/observability/shared_metrics_contract.py --- */

typedef struct { const char *metric; const char *const *fields; } smd_field_set;
typedef struct { const char *metric; const char *field; const char *const *values; } smd_enum_entry;
typedef struct { const char *metric; const char *field; int max_length; } smd_ident_entry;
typedef struct { const char *metric; const char *field; } smd_field_pair;

static const char *const SMD_MF_0[] = {"cause", "model", "provider", NULL};
static const char *const SMD_MF_1[] = {NULL};
static const char *const SMD_MF_2[] = {"context_fill_bucket", "failure_class", "outcome", "trigger", NULL};
static const char *const SMD_MF_3[] = {"described_first", "outcome", "searches_before_bucket", NULL};
static const char *const SMD_MF_4[] = {"limit_hit", "model", "peak_fill_bucket", "provider", "window_bucket", NULL};
static const char *const SMD_MF_5[] = {"delivery_kind", "duration_bucket", "outcome", NULL};
static const char *const SMD_MF_6[] = {"archived_bucket", "created_bucket", "merged_bucket", "outcome", "patched_bucket", "trigger", NULL};
static const char *const SMD_MF_7[] = {"depth", "mode", "outcome", "subagent_count_bucket", NULL};
static const char *const SMD_MF_8[] = {"action", "count_bucket", "via", NULL};
static const char *const SMD_MF_9[] = {"direction", "setting", "signal", "target", NULL};
static const char *const SMD_MF_10[] = {"area", NULL};
static const char *const SMD_MF_11[] = {"detail", "kind", NULL};
static const char *const SMD_MF_12[] = {"active_minutes_bucket", "bot_count_bucket", "messages_sent_bucket", "mode", NULL};
static const char *const SMD_MF_13[] = {"event", "step", NULL};
static const char *const SMD_MF_14[] = {"active_minutes_bucket", "active_profile_count_bucket", "gateway_peak_busy_chats_bucket", "primary_model", "primary_provider", "surfaces_used_count", NULL};
static const char *const SMD_MF_15[] = {"active_minutes_bucket", "surface", NULL};
static const char *const SMD_MF_16[] = {"backend", "error_class", "kind", "outcome", NULL};
static const char *const SMD_MF_17[] = {"failure_class", "kind", "name", "outcome", "registry", "source", NULL};
static const char *const SMD_MF_18[] = {"error_class", "from_provider", "to_provider", NULL};
static const char *const SMD_MF_19[] = {"days_since_install_bucket", "feature", NULL};
static const char *const SMD_MF_20[] = {"event", "kind", "name", "surface", NULL};
static const char *const SMD_MF_21[] = {"match_strategy", "mode", "outcome", "tool", NULL};
static const char *const SMD_MF_22[] = {"disposition", "platform", "wait_bucket", NULL};
static const char *const SMD_MF_23[] = {"first_response_bucket", "platform", NULL};
static const char *const SMD_MF_24[] = {"install_age_bucket", "milestone", NULL};
static const char *const SMD_MF_25[] = {"duration_bucket", "failed_stage", "failure_class", "installer", "outcome", NULL};
static const char *const SMD_MF_26[] = {"behind_bucket", "compression_prepare_ahead", "cron_job_count_bucket", "display_language", "gpu_class", "install_age_bucket", "local_model_provider_used", "main_provider", "mcp_server_count_bucket", "memory_provider", "messaging_platform_count_bucket", "plugin_count_bucket", "profile_count_bucket", "ram_bucket", "release_channel", "skill_count_bucket", "terminal_backend", "version_age_bucket", NULL};
static const char *const SMD_MF_27[] = {"detector", "model", "provider", "signal", NULL};
static const char *const SMD_MF_28[] = {"failure_class", "op", "origin", "outcome", "provider", NULL};
static const char *const SMD_MF_29[] = {"call_role", "locality", "model_family", "outcome", "provider_family", NULL};
static const char *const SMD_MF_30[] = {"model", "provider", "signal", NULL};
static const char *const SMD_MF_31[] = {"issue", "model", "provider", NULL};
static const char *const SMD_MF_32[] = {"call_role", "error_class", "model", "outcome", "provider", "ttft_bucket", NULL};
static const char *const SMD_MF_33[] = {"execution_surface", "from_provider", "to_provider", NULL};
static const char *const SMD_MF_34[] = {"model", "provider", "turns_before_switch_bucket", NULL};
static const char *const SMD_MF_35[] = {"aux_task", "call_role", "model", "provider", "token_type", NULL};
static const char *const SMD_MF_36[] = {"call_role", "issue", "model", "provider", NULL};
static const char *const SMD_MF_37[] = {"failure_class", "outcome", "platform", NULL};
static const char *const SMD_MF_38[] = {"error_class", "event", "platform", NULL};
static const char *const SMD_MF_39[] = {"event", "failure_class", "origin", "reason", "wait_bucket", NULL};
static const char *const SMD_MF_40[] = {"crash_class", "exit_kind", "process_kind", NULL};
static const char *const SMD_MF_41[] = {"event", "failure_class", "provider", "surface", NULL};
static const char *const SMD_MF_42[] = {"active_duration_bucket", "entrypoint", "execution_surface", "failed_turn_count_bucket", "last_outcome", "message_count_bucket", "model_call_count_bucket", "platform", "tool_call_count_bucket", "turn_count_bucket", NULL};
static const char *const SMD_MF_43[] = {"provider", "surface", NULL};
static const char *const SMD_MF_44[] = {"action", "provenance", NULL};
static const char *const SMD_MF_45[] = {"post_patch_state", "provenance", "reuse_state", "skill_name", "use_count_bucket", NULL};
static const char *const SMD_MF_46[] = {"command", "execution_surface", NULL};
static const char *const SMD_MF_47[] = {"latency_bucket", "surface", NULL};
static const char *const SMD_MF_48[] = {"api_calls_bucket", "model", "outcome", "provider", "tokens_bucket", "tool_calls_bucket", NULL};
static const char *const SMD_MF_49[] = {"duration_bucket", "execution_surface", "outcome", "retry_count_bucket", NULL};
static const char *const SMD_MF_50[] = {"end_reason", "entrypoint", "execution_surface", "failure_class", "outcome", "platform", "termination", NULL};
static const char *const SMD_MF_51[] = {"entrypoint", "execution_surface", "platform", NULL};
static const char *const SMD_MF_52[] = {"backend", "command_kind", "outcome", NULL};
static const char *const SMD_MF_53[] = {"error_class", "outcome", "tool_name", NULL};
static const char *const SMD_MF_54[] = {"attribution", "outcome", NULL};
static const char *const SMD_MF_55[] = {"connector_share", "model", "provider", "size_bucket", NULL};
static const char *const SMD_MF_56[] = {"approval_outcome", "outcome", "tool_category", NULL};
static const char *const SMD_MF_57[] = {"latency_bucket", "retry_count_bucket", "tool_category", NULL};
static const char *const SMD_MF_58[] = {"toolset", "used", NULL};
static const char *const SMD_MF_59[] = {"original_size_bucket", "tool", "truncated", NULL};
static const char *const SMD_MF_60[] = {"enabled_tool_count_bucket", "execution_surface", "tool_schema_tokens_bucket", NULL};
static const char *const SMD_MF_61[] = {"model", "next_outcome", "next_tool", "provider", "tool", NULL};
static const char *const SMD_MF_62[] = {"hits_bucket", "outcome", "scope", NULL};
static const char *const SMD_MF_63[] = {"model", "provider", "tool_name", NULL};
static const char *const SMD_MF_64[] = {"apply_mode", "duration_bucket", "failed_stage", "failure_class", "from_version_age_bucket", "kind", "outcome", NULL};
static const char *const SMD_MF_65[] = {"duration_bucket", "outcome", "stage", NULL};
static const char *const SMD_MF_66[] = {"model", "provider", "reason", "tokens_bucket", NULL};

static const smd_field_set METRIC_FIELDS[] = {
    {"hermes.cache_break.count", SMD_MF_0},
    {"hermes.client.active", SMD_MF_1},
    {"hermes.compression.count", SMD_MF_2},
    {"hermes.connector_chain.count", SMD_MF_3},
    {"hermes.context_peak.count", SMD_MF_4},
    {"hermes.cron.run", SMD_MF_5},
    {"hermes.curator.run.count", SMD_MF_6},
    {"hermes.delegation.run.count", SMD_MF_7},
    {"hermes.desktop.action_use", SMD_MF_8},
    {"hermes.desktop.dislike", SMD_MF_9},
    {"hermes.desktop.feature_use", SMD_MF_10},
    {"hermes.desktop.friction", SMD_MF_11},
    {"hermes.desktop.mode_use", SMD_MF_12},
    {"hermes.desktop.onboarding", SMD_MF_13},
    {"hermes.engagement.day.count", SMD_MF_14},
    {"hermes.engagement.surface_day.count", SMD_MF_15},
    {"hermes.execution_backend.count", SMD_MF_16},
    {"hermes.extension.install.count", SMD_MF_17},
    {"hermes.fallback.count", SMD_MF_18},
    {"hermes.feature_adoption.count", SMD_MF_19},
    {"hermes.feature_disabled.count", SMD_MF_20},
    {"hermes.file_edit.count", SMD_MF_21},
    {"hermes.gateway.busy_arrival.count", SMD_MF_22},
    {"hermes.gateway.reply_latency", SMD_MF_23},
    {"hermes.install.milestone", SMD_MF_24},
    {"hermes.install.run", SMD_MF_25},
    {"hermes.install.snapshot", SMD_MF_26},
    {"hermes.loop_guard.count", SMD_MF_27},
    {"hermes.memory.op.count", SMD_MF_28},
    {"hermes.model_call.count", SMD_MF_29},
    {"hermes.model_friction.count", SMD_MF_30},
    {"hermes.model_reply_issue.count", SMD_MF_31},
    {"hermes.model_route.count", SMD_MF_32},
    {"hermes.model_switch.count", SMD_MF_33},
    {"hermes.model_switch_after.count", SMD_MF_34},
    {"hermes.model_tokens.sum", SMD_MF_35},
    {"hermes.model_tool_quality.count", SMD_MF_36},
    {"hermes.platform.delivery", SMD_MF_37},
    {"hermes.platform.health", SMD_MF_38},
    {"hermes.prepared_compaction.count", SMD_MF_39},
    {"hermes.process.exit", SMD_MF_40},
    {"hermes.provider_setup.count", SMD_MF_41},
    {"hermes.session.count", SMD_MF_42},
    {"hermes.setup.completed", SMD_MF_43},
    {"hermes.skill.lifecycle.count", SMD_MF_44},
    {"hermes.skill.load.count", SMD_MF_45},
    {"hermes.slash_command.count", SMD_MF_46},
    {"hermes.startup.latency", SMD_MF_47},
    {"hermes.task_cost.count", SMD_MF_48},
    {"hermes.task_run.duration", SMD_MF_49},
    {"hermes.task_run.finished", SMD_MF_50},
    {"hermes.task_run.started", SMD_MF_51},
    {"hermes.terminal.outcome.count", SMD_MF_52},
    {"hermes.tool.usage.count", SMD_MF_53},
    {"hermes.tool_approval.count", SMD_MF_54},
    {"hermes.tool_batch.count", SMD_MF_55},
    {"hermes.tool_call.count", SMD_MF_56},
    {"hermes.tool_call.latency", SMD_MF_57},
    {"hermes.tool_enabled_unused.count", SMD_MF_58},
    {"hermes.tool_output_truncation.count", SMD_MF_59},
    {"hermes.tool_overhead.count", SMD_MF_60},
    {"hermes.tool_recovery.count", SMD_MF_61},
    {"hermes.tool_search.count", SMD_MF_62},
    {"hermes.tool_unavailable.count", SMD_MF_63},
    {"hermes.update.run", SMD_MF_64},
    {"hermes.update.stage", SMD_MF_65},
    {"hermes.wasted_tokens.count", SMD_MF_66},
    {NULL, NULL}
};

static const char *const SMD_LG_0[] = {"context_fill_bucket", "outcome", "trigger", NULL};
static const char *const SMD_LG_1[] = {"active_minutes_bucket", "active_profile_count_bucket", "primary_model", "primary_provider", "surfaces_used_count", NULL};
static const char *const SMD_LG_2[] = {"kind", "name", "outcome", "source", NULL};
static const char *const SMD_LG_3[] = {"cron_job_count_bucket", "mcp_server_count_bucket", "memory_provider", "plugin_count_bucket", "profile_count_bucket", "skill_count_bucket", NULL};
static const char *const SMD_LG_4[] = {"cron_job_count_bucket", "display_language", "install_age_bucket", "main_provider", "mcp_server_count_bucket", "memory_provider", "messaging_platform_count_bucket", "plugin_count_bucket", "profile_count_bucket", "skill_count_bucket", "terminal_backend", NULL};
static const char *const SMD_LG_5[] = {"behind_bucket", "cron_job_count_bucket", "display_language", "gpu_class", "install_age_bucket", "local_model_provider_used", "main_provider", "mcp_server_count_bucket", "memory_provider", "messaging_platform_count_bucket", "plugin_count_bucket", "profile_count_bucket", "ram_bucket", "release_channel", "skill_count_bucket", "terminal_backend", "version_age_bucket", NULL};
static const char *const SMD_LG_6[] = {"op", "origin", "outcome", "provider", NULL};
static const char *const SMD_LG_7[] = {"model", "provider", NULL};
static const char *const SMD_LG_8[] = {"call_role", "error_class", "model", "outcome", "provider", NULL};
static const char *const SMD_LG_9[] = {"active_duration_bucket", "entrypoint", "execution_surface", "failed_turn_count_bucket", "last_outcome", "platform", "turn_count_bucket", NULL};
static const char *const SMD_LG_10[] = {"post_patch_state", "provenance", "reuse_state", "use_count_bucket", NULL};
static const char *const SMD_LG_11[] = {"duration_bucket", "end_reason", "entrypoint", "execution_surface", "model_call_count_bucket", "outcome", "retry_count_bucket", "termination", "tool_call_count_bucket", NULL};
static const char *const SMD_LG_12[] = {"entrypoint", "execution_surface", NULL};
static const char *const SMD_LG_13[] = {"approval_outcome", "latency_bucket", "outcome", "retry_count_bucket", "tool_category", NULL};
static const char *const SMD_LG_14[] = {"apply_mode", "duration_bucket", "failed_stage", "from_version_age_bucket", "kind", "outcome", NULL};

static const smd_field_set LEGACY_SETS[] = {
    {"hermes.compression.count", SMD_LG_0},
    {"hermes.engagement.day.count", SMD_LG_1},
    {"hermes.extension.install.count", SMD_LG_2},
    {"hermes.install.snapshot", SMD_LG_3},
    {"hermes.install.snapshot", SMD_LG_4},
    {"hermes.install.snapshot", SMD_LG_5},
    {"hermes.memory.op.count", SMD_LG_6},
    {"hermes.model_route.count", SMD_LG_7},
    {"hermes.model_route.count", SMD_LG_8},
    {"hermes.session.count", SMD_LG_9},
    {"hermes.skill.load.count", SMD_LG_10},
    {"hermes.task_run.finished", SMD_LG_11},
    {"hermes.task_run.started", SMD_LG_12},
    {"hermes.tool_call.count", SMD_LG_13},
    {"hermes.update.run", SMD_LG_14},
    {NULL, NULL}
};

/* Closed enum sets. Catalog-backed fields are absent here on purpose: they
 * travel in the caller's catalog snapshot. */
static const char *const SMD_EN_0[] = {"cache_expired", "compression", "model_switch", "provider_reported_miss", "system_prompt_rebuild", "toolset_change", NULL};
static const char *const SMD_EN_1[] = {"50_to_75", "75_to_90", "90_to_100", "gte_100", "lt_50", "unknown", NULL};
static const char *const SMD_EN_2[] = {"attempt_superseded", "aux_model_fallback", "commit_fence_cancelled", "empty_post_handoff_window", "empty_transcript", "exception", "explicit_interrupt", "feasibility_skip", "insufficient_messages", "lock_contended", "no_compressible_window", "no_progress", "none", "other", "pool_saturated", "rollback", "session_split_failed", "snapshot_stale", "stall_deterministic_fallback", "stall_interrupted", "summary_auth_failure", "summary_empty_content_failure", "summary_generation_aborted", "summary_generation_failed", "summary_network_failure", "summary_overload_degraded", "summary_overload_failure", "summary_truncated_failure", "unknown", "would_grow", NULL};
static const char *const SMD_EN_3[] = {"failed", "skipped", "success", NULL};
static const char *const SMD_EN_4[] = {"auto", "manual", "other", "overflow", NULL};
static const char *const SMD_EN_5[] = {"no", "yes", NULL};
static const char *const SMD_EN_6[] = {"blocked", "cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_7[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_8[] = {"no", "yes", NULL};
static const char *const SMD_EN_9[] = {"50_to_75", "75_to_90", "90_to_100", "gte_100", "lt_50", "unknown", NULL};
static const char *const SMD_EN_10[] = {"128k_to_256k", "256k_to_1m", "32k_to_128k", "gte_1m", "lt_32k", "unknown", NULL};
static const char *const SMD_EN_11[] = {"local", "none", "other", "platform", "webhook", NULL};
static const char *const SMD_EN_12[] = {"1s_to_5s", "2m_to_10m", "30s_to_2m", "5s_to_30s", "gte_10m", "lt_1s", NULL};
static const char *const SMD_EN_13[] = {"cancelled", "failed", "missed", "skipped", "success", NULL};
static const char *const SMD_EN_14[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_15[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_16[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_17[] = {"failed", "skipped", "success", NULL};
static const char *const SMD_EN_18[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_19[] = {"manual", "scheduled", NULL};
static const char *const SMD_EN_20[] = {"1", "2", "3", "gte_4", NULL};
static const char *const SMD_EN_21[] = {"background", "foreground", NULL};
static const char *const SMD_EN_22[] = {"cancelled", "failed", "partial", "success", NULL};
static const char *const SMD_EN_23[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_24[] = {"appearance.toggleMode", "composer.attach", "composer.cancel", "composer.dictate", "composer.focus", "composer.help", "composer.history", "composer.mention", "composer.modelPicker", "composer.newline", "composer.queue", "composer.reasoningDown", "composer.reasoningUp", "composer.send", "composer.sendQueued", "composer.slash", "composer.steer", "composer.voice", "conversation.scrollPageDown", "conversation.scrollPageUp", "hud.snapToPointer", "keybinds.openPanel", "message.copy", "message.retry", "nav.agents", "nav.artifacts", "nav.capabilities", "nav.commandCenter", "nav.commandPalette", "nav.cron", "nav.messaging", "nav.profiles", "nav.settings", "other", "profile.create", "profile.default", "profile.next", "profile.prev", "profile.toggleAll", "session.archive", "session.focusSearch", "session.new", "session.newTab", "session.newWindow", "session.next", "session.prev", "session.togglePin", "view.closeTab", "view.closeTerminal", "view.cycleSidebarGrouping", "view.findInPage", "view.findNext", "view.findPrevious", "view.flipPanes", "view.newTerminal", "view.nextTerminal", "view.prevTerminal", "view.reopenTab", "view.selectionToComposer", "view.showBrowser", "view.showFiles", "view.showTerminal", "view.terminalCopy", "view.terminalPaste", "view.toggleHud", "view.toggleProfileRail", "view.toggleReview", "view.toggleRightSidebar", "view.toggleSidebar", "view.toggleSimpleMode", "view.toggleStatusbar", "view.toggleTabStrip", "workspace.newWorktree", "workspace.openFolder", NULL};
static const char *const SMD_EN_25[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_26[] = {"click", "menu", "palette", "shortcut", NULL};
static const char *const SMD_EN_27[] = {"away_from_default", "none", "to_default", NULL};
static const char *const SMD_EN_28[] = {"cancelled", "feature_disabled", "quick_close", "rage_click", "setting_off_default", "undo", NULL};
static const char *const SMD_EN_29[] = {"agents", "appearance.toggleMode", "artifacts", "backdrop", "bot_activity_toasts", "bot_mode", "browser_pane", "capabilities", "closed_tab", "command_center", "command_palette", "composer.attach", "composer.cancel", "composer.dictate", "composer.focus", "composer.help", "composer.history", "composer.mention", "composer.modelPicker", "composer.newline", "composer.queue", "composer.reasoningDown", "composer.reasoningUp", "composer.send", "composer.sendQueued", "composer.slash", "composer.steer", "composer.voice", "composer_popout_gestures", "conversation.scrollPageDown", "conversation.scrollPageUp", "cron", "extension_page", "file_pane", "find_in_page", "free_tier_sign_in", "hud.snapToPointer", "intro_splash", "kanban", "keybind_capture", "keybinds.openPanel", "message.copy", "message.retry", "messaging", "model_picker", "native_notifications", "nav.agents", "nav.artifacts", "nav.capabilities", "nav.commandCenter", "nav.commandPalette", "nav.cron", "nav.messaging", "nav.profiles", "nav.settings", "notification_kind", "other", "profile.create", "profile.default", "profile.next", "profile.prev", "profile.toggleAll", "profiles", "project_create", "projects", "provider_oauth", "reactions", "restored_draft", "review_pane", "session.archive", "session.focusSearch", "session.new", "session.newTab", "session.newWindow", "session.next", "session.prev", "session.togglePin", "session_import", "session_picker", "session_search", "session_switcher", "setting", "settings_about", "settings_billing", "settings_config_advanced", "settings_config_appearance", "settings_config_browser", "settings_config_chat", "settings_config_memory", "settings_config_model", "settings_config_safety", "settings_config_voice", "settings_config_workspace", "settings_gateway", "settings_keybinds", "settings_keys", "settings_notifications", "settings_other", "settings_providers", "settings_sessions", "settings_vault", "skins", "starmap", "terminal_pane", "thread_timeline", "tips", "tours", "vibe_hearts", "view.closeTab", "view.closeTerminal", "view.cycleSidebarGrouping", "view.findInPage", "view.findNext", "view.findPrevious", "view.flipPanes", "view.newTerminal", "view.nextTerminal", "view.prevTerminal", "view.reopenTab", "view.selectionToComposer", "view.showBrowser", "view.showFiles", "view.showTerminal", "view.terminalCopy", "view.terminalPaste", "view.toggleHud", "view.toggleProfileRail", "view.toggleReview", "view.toggleRightSidebar", "view.toggleSidebar", "view.toggleSimpleMode", "view.toggleStatusbar", "view.toggleTabStrip", "voice_conversation", "voice_dictation", "webhooks", "workspace.newWorktree", "workspace.openFolder", NULL};
static const char *const SMD_EN_30[] = {"agents", "artifacts", "bot_mode", "browser_pane", "capabilities", "command_center", "command_palette", "cron", "extension_page", "file_pane", "find_in_page", "kanban", "messaging", "model_picker", "other", "profiles", "projects", "review_pane", "session_import", "session_picker", "session_search", "session_switcher", "settings_about", "settings_billing", "settings_config_advanced", "settings_config_appearance", "settings_config_browser", "settings_config_chat", "settings_config_memory", "settings_config_model", "settings_config_safety", "settings_config_voice", "settings_config_workspace", "settings_gateway", "settings_keybinds", "settings_keys", "settings_notifications", "settings_other", "settings_providers", "settings_sessions", "settings_vault", "skins", "starmap", "terminal_pane", "voice_conversation", "voice_dictation", "webhooks", NULL};
static const char *const SMD_EN_31[] = {"100ms_to_250ms", "1s_to_5s", "250ms_to_1s", "api_key_missing", "api_key_rejected", "artifacts_partial_load", "backend_exit", "backend_skew", "background_queue_stuck", "billing_banner", "billing_block", "build_discontinued", "client_behind", "composer_queue_stuck", "crash", "credits", "disk_full", "free_tier_notice", "gateway_auth_failed", "gateway_error", "gte_5s", "gui_skew", "install_method", "killed", "mcp_health", "method_not_allowed", "microphone_permission", "model_warning", "network", "onboarding_handoff", "oom", "other", "pool_slot_timeout", "restart_required", "restored_draft", "rpc_out_of_sync", "runtime_not_ready", "session_compress", "storage_failure", "terminal_backend", "timeout", "tip", "unclassified", "update_available", "voice_live_unavailable", "voice_stop_hint", NULL};
static const char *const SMD_EN_32[] = {"backend_disconnect", "error_toast", "notice_dismissed", "renderer_crash", "slow_frame", NULL};
static const char *const SMD_EN_33[] = {"0", "2h_to_6h", "30m_to_2h", "5m_to_30m", "gte_6h", "lt_5m", NULL};
static const char *const SMD_EN_34[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_35[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_36[] = {"bots", "sessions", NULL};
static const char *const SMD_EN_37[] = {"abandoned", "completed", "reached", NULL};
static const char *const SMD_EN_38[] = {"choose_later", "consent", "first_message", "free_tier_ready", "guide", "guide_skip", "model_pick", "provider_api_key", "provider_local", "provider_oauth", "provider_setup", "sign_in", NULL};
static const char *const SMD_EN_39[] = {"0", "2h_to_6h", "30m_to_2h", "5m_to_30m", "gte_6h", "lt_5m", NULL};
static const char *const SMD_EN_40[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_41[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_42[] = {"0", "1", "2", "3", "gte_4", NULL};
static const char *const SMD_EN_43[] = {"0", "2h_to_6h", "30m_to_2h", "5m_to_30m", "gte_6h", "lt_5m", NULL};
static const char *const SMD_EN_44[] = {"acp", "cli", "cron", "desktop", "gateway", "tui", NULL};
static const char *const SMD_EN_45[] = {"browser-use", "browserbase", "camofox", "cdp", "daytona", "docker", "extension", "firecrawl", "lightpanda", "local", "managed_modal", "modal", "other", "remote", "singularity", "ssh", "vercel_sandbox", NULL};
static const char *const SMD_EN_46[] = {"alias", "blocked", "connection_required", "connector_backend", "contract_violation", "exception", "interrupted", "invalid_arguments", "none", "not_allowed", "timeout", "tool_error", "unknown", NULL};
static const char *const SMD_EN_47[] = {"browser", "code", "terminal", NULL};
static const char *const SMD_EN_48[] = {"failed", "success", NULL};
static const char *const SMD_EN_49[] = {"already_installed", "ambiguous", "auth_rejected", "auth_required", "bootstrap_failed", "clone_failed", "config_invalid", "config_rejected", "connect_failed", "deps_declined", "deps_failed", "exception", "fetch_failed", "filesystem_error", "git_missing", "incompatible", "invalid_bundle", "invalid_name", "invalid_source", "manifest_invalid", "missing_credentials", "network", "non_interactive", "none", "not_found", "other", "permission", "rate_limited", "removed_from_catalog", "scan_blocked", "server_start_failed", "stale_index", NULL};
static const char *const SMD_EN_50[] = {"mcp_server", "plugin", "skill", NULL};
static const char *const SMD_EN_51[] = {"failed", "success", NULL};
static const char *const SMD_EN_52[] = {"browse-sh", "clawhub", "github", "hermes-index", "lobehub", "none", "official", "other", "skills-sh", "unresolved", "url", "well-known", NULL};
static const char *const SMD_EN_53[] = {"bundled", "catalog", "hub", "local", "other", "url", NULL};
static const char *const SMD_EN_54[] = {"auth", "auth_permanent", "billing", "content_policy_blocked", "context_overflow", "data_policy_denied", "format_error", "guardrail_blocked", "image_corrupt", "image_too_large", "incomplete_response", "invalid_encrypted_content", "llama_cpp_grammar_pattern", "long_context_tier", "model_entitlement", "model_not_found", "multimodal_tool_content_unsupported", "none", "oauth_long_context_beta_forbidden", "overloaded", "payload_too_large", "provider_policy_blocked", "rate_limit", "reasoning_mandatory", "role_alternation", "server_error", "ssl_cert_verification", "thinking_signature", "timeout", "unknown", "upstream_blocked", "upstream_rate_limit", NULL};
static const char *const SMD_EN_55[] = {"1d_to_7d", "30d_to_90d", "7d_to_30d", "gte_90d", "same_day", "unknown", NULL};
static const char *const SMD_EN_56[] = {"bot_mode", "browser", "cron", "curator", "delegation", "desktop", "gateway_platform", "kanban", "mcp", "memory", "plugins", "projects", "skills_created", "tui", "voice", NULL};
static const char *const SMD_EN_57[] = {"disabled", "re_enabled", NULL};
static const char *const SMD_EN_58[] = {"compression", "curator", "memory", "platform", "plugin", "setting", "skill", "toolset", NULL};
static const char *const SMD_EN_59[] = {"cli_config", "cli_slash", "cli_tools", "dashboard", "desktop", "tui", NULL};
static const char *const SMD_EN_60[] = {"block_anchor", "context_aware", "escape_normalized", "exact", "indentation_flexible", "line_trimmed", "none", "trimmed_boundary", "unicode_normalized", "whitespace_normalized", NULL};
static const char *const SMD_EN_61[] = {"replace", "v4a", "whole_file", NULL};
static const char *const SMD_EN_62[] = {"already_applied", "ambiguous", "applied", "failed", "no_match", NULL};
static const char *const SMD_EN_63[] = {"patch", "write_file", NULL};
static const char *const SMD_EN_64[] = {"dropped", "interrupted", "merged", "other", "queued", "steered", NULL};
static const char *const SMD_EN_65[] = {"2m_to_10m", "30s_to_2m", "5s_to_30s", "gte_10m", "lt_5s", "never", NULL};
static const char *const SMD_EN_66[] = {"15s_to_60s", "2s_to_5s", "5s_to_15s", "gte_60s", "lt_2s", NULL};
static const char *const SMD_EN_67[] = {"1d_to_7d", "1h_to_1d", "30d_to_90d", "7d_to_30d", "gte_90d", "lt_1h", "unknown", NULL};
static const char *const SMD_EN_68[] = {"first_delegation", "first_gateway_message", "first_long_session", "first_mcp_tool_success", "first_scheduled_task", "first_skill_created", "first_skill_reused", "first_task_started", "first_task_success", "first_tool_success", "setup_completed", NULL};
static const char *const SMD_EN_69[] = {"2m_to_5m", "30s_to_2m", "5m_to_15m", "gte_15m", "lt_30s", NULL};
static const char *const SMD_EN_70[] = {"complete", "config", "gateway", "none", "other", "prerequisites", "products", "python_deps", "repository", "setup", "venv", NULL};
static const char *const SMD_EN_71[] = {"commit_not_on_branch", "curl_missing", "deps_install_failed", "dir_not_checkout", "download_digest_mismatch", "download_failed", "filesystem_error", "gateway_failed", "git_checkout_failed", "git_clone_failed", "git_extract_failed", "git_fetch_failed", "git_missing", "git_reset_failed", "interrupted", "libstdcxx_missing", "local_changes_blocked", "none", "other", "products_build_failed", "python_install_failed", "setup_failed", "unsupported_platform", "uv_unusable", NULL};
static const char *const SMD_EN_72[] = {"install_ps1", "install_sh", "other", NULL};
static const char *const SMD_EN_73[] = {"failed", "success", NULL};
static const char *const SMD_EN_74[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", "unknown", NULL};
static const char *const SMD_EN_75[] = {"off", "on", NULL};
static const char *const SMD_EN_76[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_77[] = {"amd", "apple_silicon", "intel", "none", "nvidia", "unknown", NULL};
static const char *const SMD_EN_78[] = {"1d_to_7d", "1h_to_1d", "30d_to_90d", "7d_to_30d", "gte_90d", "lt_1h", "unknown", NULL};
static const char *const SMD_EN_79[] = {"no", "yes", NULL};
static const char *const SMD_EN_80[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_81[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_82[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_83[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_84[] = {"16g_to_32g", "32g_to_64g", "64g_to_128g", "8g_to_16g", "gte_128g", "lt_8g", "unknown", NULL};
static const char *const SMD_EN_85[] = {"dev", "main", "stable", "unknown", NULL};
static const char *const SMD_EN_86[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_87[] = {"daytona", "docker", "local", "managed_modal", "modal", "other", "singularity", "ssh", "vercel_sandbox", NULL};
static const char *const SMD_EN_88[] = {"30d_to_90d", "7d_to_30d", "gte_90d", "lt_7d", "unknown", NULL};
static const char *const SMD_EN_89[] = {"exact_failure", "idempotent_no_progress", "identical_call_streak", "identical_cycle", "iteration_budget", "same_tool_failure", "subagent_cap", "web_search_cap", NULL};
static const char *const SMD_EN_90[] = {"iteration_cap", "loop_detected", "repeated_tool_call", NULL};
static const char *const SMD_EN_91[] = {"ambiguous", "disabled", "drift", "exception", "gate_refused", "invalid_args", "missing_content", "missing_old_text", "no_match", "none", "other", "over_budget", "provider_error", "read_failed", "retry_cap", "scan_blocked", "staged", "stale_entry", "unknown", "would_empty", NULL};
static const char *const SMD_EN_92[] = {"add", "other", "read", "remove", "replace", "search", NULL};
static const char *const SMD_EN_93[] = {"background_review", "foreground", NULL};
static const char *const SMD_EN_94[] = {"failed", "rejected", "success", NULL};
static const char *const SMD_EN_95[] = {"primary", NULL};
static const char *const SMD_EN_96[] = {"local", "remote", "unknown", NULL};
static const char *const SMD_EN_97[] = {"claude", "deepseek", "gemini", "gemma", "glm", "gpt", "grok", "kimi", "llama", "mimo", "minimax", "mistral", "nemotron", "nova", "o1", "o3", "o4", "qwen", "step", "trinity", "unknown", NULL};
static const char *const SMD_EN_98[] = {"cancelled", "failed", "success", NULL};
static const char *const SMD_EN_99[] = {"aggregator", "custom", "direct", "local", "unknown", NULL};
static const char *const SMD_EN_100[] = {"interrupt", "quick_abandon", "retry", "switch_away", "undo", NULL};
static const char *const SMD_EN_101[] = {"empty", "none", "reasoning_only", "refusal", "truncated_length", NULL};
static const char *const SMD_EN_102[] = {"auxiliary", "primary", NULL};
static const char *const SMD_EN_103[] = {"auth", "auth_permanent", "billing", "content_policy_blocked", "context_overflow", "data_policy_denied", "format_error", "guardrail_blocked", "image_corrupt", "image_too_large", "incomplete_response", "invalid_encrypted_content", "llama_cpp_grammar_pattern", "long_context_tier", "model_entitlement", "model_not_found", "multimodal_tool_content_unsupported", "none", "oauth_long_context_beta_forbidden", "overloaded", "payload_too_large", "provider_policy_blocked", "rate_limit", "reasoning_mandatory", "role_alternation", "server_error", "ssl_cert_verification", "thinking_signature", "timeout", "unknown", "upstream_blocked", "upstream_rate_limit", NULL};
static const char *const SMD_EN_104[] = {"cancelled", "failed", "success", NULL};
static const char *const SMD_EN_105[] = {"1s_to_2s", "2s_to_5s", "500ms_to_1s", "5s_to_15s", "gte_15s", "lt_500ms", "not_streamed", "unknown", NULL};
static const char *const SMD_EN_106[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_107[] = {"1", "11_to_30", "2_to_3", "4_to_10", "gte_31", NULL};
static const char *const SMD_EN_108[] = {"auxiliary", "primary", NULL};
static const char *const SMD_EN_109[] = {"cache_read", "cache_write", "input", "output", "reasoning", NULL};
static const char *const SMD_EN_110[] = {"auxiliary", "primary", NULL};
static const char *const SMD_EN_111[] = {"empty_arguments", "invalid_json", "none", "repaired", "schema_mismatch", "unknown_tool", NULL};
static const char *const SMD_EN_112[] = {"auth", "forbidden", "network", "none", "other", "rate_limited", "too_long", NULL};
static const char *const SMD_EN_113[] = {"failed", "sent", NULL};
static const char *const SMD_EN_114[] = {"auth", "config", "network", "none", "other", "rate_limited", NULL};
static const char *const SMD_EN_115[] = {"connect_failed", "connect_ok", "disconnect", "reconnect", NULL};
static const char *const SMD_EN_116[] = {"auto_disabled", "compaction_failed", "discarded", "internal_error", "pass_cancelled", "pass_completed", "pass_failed", "pass_started", "skipped_busy", "used", NULL};
static const char *const SMD_EN_117[] = {"attempt_superseded", "aux_model_fallback", "commit_fence_cancelled", "empty_post_handoff_window", "empty_transcript", "exception", "explicit_interrupt", "feasibility_skip", "insufficient_messages", "lock_contended", "no_compressible_window", "no_progress", "none", "other", "pool_saturated", "rollback", "session_split_failed", "snapshot_stale", "stall_deterministic_fallback", "stall_interrupted", "summary_auth_failure", "summary_empty_content_failure", "summary_generation_aborted", "summary_generation_failed", "summary_network_failure", "summary_overload_degraded", "summary_overload_failure", "summary_truncated_failure", "unknown", "would_grow", NULL};
static const char *const SMD_EN_118[] = {"none", "tool_batch", "turn_end", NULL};
static const char *const SMD_EN_119[] = {"agent_released", "aux_fallback_main", "compaction_aborted", "fresh_required", "head_moved", "host_cancelled", "none", "other_session", "pass_failed_while_waiting", "provider_context", "rewound", "running_not_fit", "session_boundary", "shape_changed", "superseded", "text_changed", "transcript_shorter", "wait_timeout", NULL};
static const char *const SMD_EN_120[] = {"1s_to_5s", "30s_to_120s", "5s_to_30s", "gte_120s", "lt_1s", "none", NULL};
static const char *const SMD_EN_121[] = {"import_error", "memory_error", "none", "os_error", "other", "runtime_error", NULL};
static const char *const SMD_EN_122[] = {"clean", "crash", "killed", "watchdog", NULL};
static const char *const SMD_EN_123[] = {"cli", "cron", "gateway", "other", "serve", "tui", NULL};
static const char *const SMD_EN_124[] = {"abandoned", "completed", "failed", "started", NULL};
static const char *const SMD_EN_125[] = {"auth", "cancelled", "network", "no_models", "none", "other", NULL};
static const char *const SMD_EN_126[] = {"cli_model", "cli_setup", "dashboard", "desktop", "tui", NULL};
static const char *const SMD_EN_127[] = {"1m_to_5m", "2h_to_8h", "30m_to_2h", "5m_to_30m", "gte_8h", "lt_1m", NULL};
static const char *const SMD_EN_128[] = {"api", "background", "batch", "delegated", "gateway_message", "interactive", "one_shot", "other", "python", "scheduled_task", "unknown", NULL};
static const char *const SMD_EN_129[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_130[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_131[] = {"cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_132[] = {"0", "1", "101_to_250", "11_to_25", "2", "251_to_1000", "26_to_100", "3_to_5", "6_to_10", "gte_1001", "gte_251", NULL};
static const char *const SMD_EN_133[] = {"0", "1", "101_to_250", "11_to_25", "2", "251_to_1000", "26_to_100", "3_to_5", "6_to_10", "gte_1001", "gte_251", NULL};
static const char *const SMD_EN_134[] = {"0", "1", "101_to_250", "11_to_25", "2", "251_to_1000", "26_to_100", "3_to_5", "6_to_10", "gte_1001", "gte_251", NULL};
static const char *const SMD_EN_135[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_136[] = {"cli", "desktop", "other", NULL};
static const char *const SMD_EN_137[] = {"archived", "created", "edited", "installed", "patched", "restored", "stale", NULL};
static const char *const SMD_EN_138[] = {"agent_created", "external", "installed", "local", "unknown", NULL};
static const char *const SMD_EN_139[] = {"no_new_patch", "not_applicable", "reused_after_patch", NULL};
static const char *const SMD_EN_140[] = {"agent_created", "external", "installed", "local", "unknown", NULL};
static const char *const SMD_EN_141[] = {"first_use", "reused", NULL};
static const char *const SMD_EN_142[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_143[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_144[] = {"1s_to_2s", "2s_to_5s", "500ms_to_1s", "5s_to_10s", "gte_10s", "lt_500ms", NULL};
static const char *const SMD_EN_145[] = {"cli", "desktop_attach", "gateway_boot", "serve_boot", "tui", NULL};
static const char *const SMD_EN_146[] = {"0", "1", "11_to_25", "2", "26_to_50", "3_to_5", "51_to_100", "6_to_10", "gte_101", NULL};
static const char *const SMD_EN_147[] = {"completed", "failed", "interrupted", NULL};
static const char *const SMD_EN_148[] = {"10k_to_50k", "200k_to_1m", "2k_to_10k", "50k_to_200k", "gte_1m", "lt_2k", "unknown", NULL};
static const char *const SMD_EN_149[] = {"0", "1", "11_to_25", "2", "26_to_50", "3_to_5", "51_to_100", "6_to_10", "gte_101", NULL};
static const char *const SMD_EN_150[] = {"1s_to_5s", "2m_to_10m", "30s_to_2m", "5s_to_30s", "gte_10m", "lt_1s", NULL};
static const char *const SMD_EN_151[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_152[] = {"cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_153[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_154[] = {"1s_to_5s", "2m_to_10m", "30s_to_2m", "5s_to_30s", "gte_10m", "lt_1s", NULL};
static const char *const SMD_EN_155[] = {"approval_denied", "completed", "failed", "guardrail_blocked", "iteration_limit", "system_aborted", "timed_out", "unknown", "user_cancelled", NULL};
static const char *const SMD_EN_156[] = {"api", "background", "batch", "delegated", "gateway_message", "interactive", "one_shot", "other", "python", "scheduled_task", "unknown", NULL};
static const char *const SMD_EN_157[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_158[] = {"auth", "auth_permanent", "billing", "content_policy_blocked", "context_compression", "context_overflow", "data_policy_denied", "empty_response", "exception", "format_error", "guardrail_blocked", "image_corrupt", "image_too_large", "incomplete_response", "invalid_encrypted_content", "llama_cpp_grammar_pattern", "local_error", "long_context_tier", "model_entitlement", "model_not_found", "multimodal_tool_content_unsupported", "none", "oauth_long_context_beta_forbidden", "other", "overloaded", "payload_too_large", "persistence", "provider_policy_blocked", "rate_limit", "reasoning_mandatory", "repeated_errors", "restart_limit", "role_alternation", "server_error", "session_busy", "shutdown", "ssl_cert_verification", "thinking_signature", "timeout", "unknown", "upstream_blocked", "upstream_rate_limit", NULL};
static const char *const SMD_EN_159[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_160[] = {"cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_161[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_162[] = {"none", "system_aborted", "timed_out", "unknown", "user_cancelled", NULL};
static const char *const SMD_EN_163[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_164[] = {"api", "background", "batch", "delegated", "gateway_message", "interactive", "one_shot", "other", "python", "scheduled_task", "unknown", NULL};
static const char *const SMD_EN_165[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_166[] = {"daytona", "docker", "local", "managed_modal", "modal", "other", "singularity", "ssh", "vercel_sandbox", NULL};
static const char *const SMD_EN_167[] = {"build", "container", "file_ops", "git", "network", "node", "other", "package_manager", "python", "shell", "shell_builtin", "test_runner", NULL};
static const char *const SMD_EN_168[] = {"killed", "nonzero", "ok", "timeout", NULL};
static const char *const SMD_EN_169[] = {"alias", "blocked", "connection_required", "connector_backend", "contract_violation", "exception", "interrupted", "invalid_arguments", "none", "not_allowed", "timeout", "tool_error", "unknown", NULL};
static const char *const SMD_EN_170[] = {"blocked", "cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_171[] = {"annotate_preview", "browser_back", "browser_cdp", "browser_click", "browser_console", "browser_dialog", "browser_exec", "browser_get_images", "browser_navigate", "browser_press", "browser_scroll", "browser_snapshot", "browser_type", "browser_vault", "browser_vault_enter_code", "browser_vault_fill", "browser_vault_list", "browser_vault_save_login", "browser_vault_unlock", "browser_vision", "clarify", "close_terminal", "computer_use", "connector", "cronjob_manage", "delegate_task", "desktop_preview", "desktop_project", "discord", "discord_admin", "drive_preview", "execute_code", "feishu_doc_read", "feishu_drive_add_comment", "feishu_drive_list_comment_replies", "feishu_drive_list_comments", "feishu_drive_reply_comment", "focus_pane", "gui_tour", "image_generate", "kanban_attach", "kanban_attach_url", "kanban_attachments", "kanban_block", "kanban_comment", "kanban_complete", "kanban_create", "kanban_heartbeat", "kanban_link", "kanban_list", "kanban_request_changes", "kanban_request_review", "kanban_show", "kanban_unblock", "manage_catalog", "manage_connections", "mcp", "memory", "patch", "plugin", "process_manage", "react_to_message", "read_file", "read_terminal", "read_window_below", "search_files", "session_search", "set_theme", "setup_beat", "setup_choose", "show_tip", "skill_manage", "skill_view", "skills_list", "spotify_albums", "spotify_devices", "spotify_library", "spotify_playback", "spotify_playlists", "spotify_queue", "spotify_search", "start_chat", "terminal", "text_to_speech", "todo_list", "tool_call", "tool_describe", "tool_search", "unknown", "video_analyze", "video_generate", "vision_analyze", "web_extract", "web_search", "write_file", "x_search", "xai_video_edit", "xai_video_extend", "yb_query_group_info", "yb_query_group_members", "yb_search_sticker", "yb_send_dm", "yb_send_sticker", NULL};
static const char *const SMD_EN_172[] = {"tool_call", "unattributed", NULL};
static const char *const SMD_EN_173[] = {"approved", "cancelled", "denied", "timed_out", "unknown", NULL};
static const char *const SMD_EN_174[] = {"all", "none", "some", NULL};
static const char *const SMD_EN_175[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_176[] = {"approved", "cancelled", "denied", "not_required", "timed_out", "unknown", NULL};
static const char *const SMD_EN_177[] = {"100ms_to_250ms", "10s_to_30s", "1s_to_2s", "250ms_to_500ms", "2s_to_5s", "500ms_to_1s", "5s_to_10s", "gte_30s", "lt_100ms", "unknown", NULL};
static const char *const SMD_EN_178[] = {"blocked", "cancelled", "failed", "success", "timed_out", "unknown", NULL};
static const char *const SMD_EN_179[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", "unknown", NULL};
static const char *const SMD_EN_180[] = {"browser", "code_execution", "communication", "computer_use", "connector", "delegation", "file", "home_automation", "mcp", "media", "memory", "other", "planning", "project", "scheduler", "skill", "terminal", "unknown", "web", NULL};
static const char *const SMD_EN_181[] = {"100ms_to_250ms", "10s_to_30s", "1s_to_2s", "250ms_to_500ms", "2s_to_5s", "500ms_to_1s", "5s_to_10s", "gte_30s", "lt_100ms", "unknown", NULL};
static const char *const SMD_EN_182[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", "unknown", NULL};
static const char *const SMD_EN_183[] = {"browser", "code_execution", "communication", "computer_use", "connector", "delegation", "file", "home_automation", "mcp", "media", "memory", "other", "planning", "project", "scheduler", "skill", "terminal", "unknown", "web", NULL};
static const char *const SMD_EN_184[] = {"bot_room", "browser", "catalog", "clarify", "code_execution", "coding", "computer_use", "connections", "context_engine", "cronjob", "custom", "debugging", "delegation", "desktop_ui", "discord", "discord_admin", "feishu_doc", "feishu_drive", "file", "hermes-acp", "hermes-api-server", "hermes-bluebubbles", "hermes-cli", "hermes-cron", "hermes-dingtalk", "hermes-discord", "hermes-email", "hermes-feishu", "hermes-gateway", "hermes-matrix", "hermes-mattermost", "hermes-qqbot", "hermes-signal", "hermes-slack", "hermes-sms", "hermes-telegram", "hermes-webhook", "hermes-wecom", "hermes-wecom-callback", "hermes-weixin", "hermes-whatsapp", "hermes-yuanbao", "image_gen", "kanban", "memory", "onboarding", "project", "safe", "search", "session_search", "setup", "skills", "spotify", "start_chat", "terminal", "todo", "tts", "video", "video_gen", "vision", "web", "x_search", "yuanbao", NULL};
static const char *const SMD_EN_185[] = {"no", "yes", NULL};
static const char *const SMD_EN_186[] = {"100k_to_500k", "10k_to_50k", "1k_to_10k", "50k_to_100k", "gte_500k", "lt_1k", "unknown", NULL};
static const char *const SMD_EN_187[] = {"annotate_preview", "browser_back", "browser_cdp", "browser_click", "browser_console", "browser_dialog", "browser_exec", "browser_get_images", "browser_navigate", "browser_press", "browser_scroll", "browser_snapshot", "browser_type", "browser_vault", "browser_vault_enter_code", "browser_vault_fill", "browser_vault_list", "browser_vault_save_login", "browser_vault_unlock", "browser_vision", "clarify", "close_terminal", "computer_use", "connector", "cronjob_manage", "delegate_task", "desktop_preview", "desktop_project", "discord", "discord_admin", "drive_preview", "execute_code", "feishu_doc_read", "feishu_drive_add_comment", "feishu_drive_list_comment_replies", "feishu_drive_list_comments", "feishu_drive_reply_comment", "focus_pane", "gui_tour", "image_generate", "kanban_attach", "kanban_attach_url", "kanban_attachments", "kanban_block", "kanban_comment", "kanban_complete", "kanban_create", "kanban_heartbeat", "kanban_link", "kanban_list", "kanban_request_changes", "kanban_request_review", "kanban_show", "kanban_unblock", "manage_catalog", "manage_connections", "mcp", "memory", "patch", "plugin", "process_manage", "react_to_message", "read_file", "read_terminal", "read_window_below", "search_files", "session_search", "set_theme", "setup_beat", "setup_choose", "show_tip", "skill_manage", "skill_view", "skills_list", "spotify_albums", "spotify_devices", "spotify_library", "spotify_playback", "spotify_playlists", "spotify_queue", "spotify_search", "start_chat", "terminal", "text_to_speech", "todo_list", "tool_call", "tool_describe", "tool_search", "unknown", "video_analyze", "video_generate", "vision_analyze", "web_extract", "web_search", "write_file", "x_search", "xai_video_edit", "xai_video_extend", "yb_query_group_info", "yb_query_group_members", "yb_search_sticker", "yb_send_dm", "yb_send_sticker", NULL};
static const char *const SMD_EN_188[] = {"no", "yes", NULL};
static const char *const SMD_EN_189[] = {"0", "1", "101_to_250", "11_to_25", "2", "26_to_100", "3_to_5", "6_to_10", "gte_251", NULL};
static const char *const SMD_EN_190[] = {"acp", "api", "batch", "cli", "desktop", "gateway", "other", "python", "scheduled_task", "tui", "unknown", NULL};
static const char *const SMD_EN_191[] = {"0", "10k_to_20k", "20k_to_40k", "2k_to_5k", "5k_to_10k", "gte_40k", "lt_2k", NULL};
static const char *const SMD_EN_192[] = {"error", "gave_up", "no_tool_call", "success", NULL};
static const char *const SMD_EN_193[] = {"different", "none", "same", NULL};
static const char *const SMD_EN_194[] = {"annotate_preview", "browser_back", "browser_cdp", "browser_click", "browser_console", "browser_dialog", "browser_exec", "browser_get_images", "browser_navigate", "browser_press", "browser_scroll", "browser_snapshot", "browser_type", "browser_vault", "browser_vault_enter_code", "browser_vault_fill", "browser_vault_list", "browser_vault_save_login", "browser_vault_unlock", "browser_vision", "clarify", "close_terminal", "computer_use", "connector", "cronjob_manage", "delegate_task", "desktop_preview", "desktop_project", "discord", "discord_admin", "drive_preview", "execute_code", "feishu_doc_read", "feishu_drive_add_comment", "feishu_drive_list_comment_replies", "feishu_drive_list_comments", "feishu_drive_reply_comment", "focus_pane", "gui_tour", "image_generate", "kanban_attach", "kanban_attach_url", "kanban_attachments", "kanban_block", "kanban_comment", "kanban_complete", "kanban_create", "kanban_heartbeat", "kanban_link", "kanban_list", "kanban_request_changes", "kanban_request_review", "kanban_show", "kanban_unblock", "manage_catalog", "manage_connections", "mcp", "memory", "patch", "plugin", "process_manage", "react_to_message", "read_file", "read_terminal", "read_window_below", "search_files", "session_search", "set_theme", "setup_beat", "setup_choose", "show_tip", "skill_manage", "skill_view", "skills_list", "spotify_albums", "spotify_devices", "spotify_library", "spotify_playback", "spotify_playlists", "spotify_queue", "spotify_search", "start_chat", "terminal", "text_to_speech", "todo_list", "tool_call", "tool_describe", "tool_search", "unknown", "video_analyze", "video_generate", "vision_analyze", "web_extract", "web_search", "write_file", "x_search", "xai_video_edit", "xai_video_extend", "yb_query_group_info", "yb_query_group_members", "yb_search_sticker", "yb_send_dm", "yb_send_sticker", NULL};
static const char *const SMD_EN_195[] = {"0", "1", "2", "3_to_5", "6_to_10", "gte_11", NULL};
static const char *const SMD_EN_196[] = {"connector_unavailable", "error", "hits", "no_hits", NULL};
static const char *const SMD_EN_197[] = {"both", "connector", "local", NULL};
static const char *const SMD_EN_198[] = {"annotate_preview", "browser_back", "browser_cdp", "browser_click", "browser_console", "browser_dialog", "browser_exec", "browser_get_images", "browser_navigate", "browser_press", "browser_scroll", "browser_snapshot", "browser_type", "browser_vault", "browser_vault_enter_code", "browser_vault_fill", "browser_vault_list", "browser_vault_save_login", "browser_vault_unlock", "browser_vision", "clarify", "close_terminal", "computer_use", "cronjob_manage", "delegate_task", "desktop_preview", "desktop_project", "discord", "discord_admin", "drive_preview", "execute_code", "feishu_doc_read", "feishu_drive_add_comment", "feishu_drive_list_comment_replies", "feishu_drive_list_comments", "feishu_drive_reply_comment", "focus_pane", "gui_tour", "image_generate", "kanban_attach", "kanban_attach_url", "kanban_attachments", "kanban_block", "kanban_comment", "kanban_complete", "kanban_create", "kanban_heartbeat", "kanban_link", "kanban_list", "kanban_request_changes", "kanban_request_review", "kanban_show", "kanban_unblock", "manage_catalog", "manage_connections", "memory", "patch", "process_manage", "react_to_message", "read_file", "read_terminal", "read_window_below", "search_files", "session_search", "set_theme", "setup_beat", "setup_choose", "show_tip", "skill_manage", "skill_view", "skills_list", "spotify_albums", "spotify_devices", "spotify_library", "spotify_playback", "spotify_playlists", "spotify_queue", "spotify_search", "start_chat", "terminal", "text_to_speech", "todo_list", "video_analyze", "video_generate", "vision_analyze", "web_extract", "web_search", "write_file", "x_search", "xai_video_edit", "xai_video_extend", "yb_query_group_info", "yb_query_group_members", "yb_search_sticker", "yb_send_dm", "yb_send_sticker", NULL};
static const char *const SMD_EN_199[] = {"external", "git", "package", "unknown", "zip", NULL};
static const char *const SMD_EN_200[] = {"2m_to_5m", "30s_to_2m", "5m_to_15m", "gte_15m", "lt_30s", NULL};
static const char *const SMD_EN_201[] = {"apply", "build", "deps", "download", "none", "other", "plan", "restart", "snapshot", "verify", NULL};
static const char *const SMD_EN_202[] = {"aborted_before_apply", "branch_missing", "branch_unsupported", "build_failed", "channel_unresolved", "checkout_move_failed", "commit_point_refused", "deps_failed", "detached_head", "disk_full", "downgrade_refused", "download_failed", "exception", "fetch_failed", "fleet_stale", "fleet_unverified", "gateway_pause_failed", "git_failed", "git_in_progress", "git_index_locked", "git_timeout", "head_moved", "interrupted", "local_changes_blocked", "local_changes_parked", "lock_held", "managed_install", "merge_conflict", "none", "not_git_checkout", "old_version_handoff", "os_error", "other", "parked_branch_blocked", "permission_denied", "restart_failed", "stash_restore_rejected", "subprocess_failed", "syntax_rollback", "target_syntax_error", "target_unresolved", "unexpected_branch", "unknown", "venv_foreign_owner", "zip_failed", NULL};
static const char *const SMD_EN_203[] = {"30d_to_90d", "7d_to_30d", "gte_90d", "lt_7d", "unknown", NULL};
static const char *const SMD_EN_204[] = {"cli", "desktop", NULL};
static const char *const SMD_EN_205[] = {"failed", "noop", "partial", "refused", "success", NULL};
static const char *const SMD_EN_206[] = {"2m_to_5m", "30s_to_2m", "5m_to_15m", "gte_15m", "lt_30s", NULL};
static const char *const SMD_EN_207[] = {"failed", "skipped", "success", NULL};
static const char *const SMD_EN_208[] = {"apply", "build", "deps", "plan", "restart", "snapshot", "verify", NULL};
static const char *const SMD_EN_209[] = {"interrupt", "retry", "undo", NULL};
static const char *const SMD_EN_210[] = {"10k_to_50k", "200k_to_1m", "2k_to_10k", "50k_to_200k", "gte_1m", "lt_2k", "unknown", NULL};

static const smd_enum_entry ENUM_VALUES[] = {
    {"hermes.cache_break.count", "cause", SMD_EN_0},
    {"hermes.compression.count", "context_fill_bucket", SMD_EN_1},
    {"hermes.compression.count", "failure_class", SMD_EN_2},
    {"hermes.compression.count", "outcome", SMD_EN_3},
    {"hermes.compression.count", "trigger", SMD_EN_4},
    {"hermes.connector_chain.count", "described_first", SMD_EN_5},
    {"hermes.connector_chain.count", "outcome", SMD_EN_6},
    {"hermes.connector_chain.count", "searches_before_bucket", SMD_EN_7},
    {"hermes.context_peak.count", "limit_hit", SMD_EN_8},
    {"hermes.context_peak.count", "peak_fill_bucket", SMD_EN_9},
    {"hermes.context_peak.count", "window_bucket", SMD_EN_10},
    {"hermes.cron.run", "delivery_kind", SMD_EN_11},
    {"hermes.cron.run", "duration_bucket", SMD_EN_12},
    {"hermes.cron.run", "outcome", SMD_EN_13},
    {"hermes.curator.run.count", "archived_bucket", SMD_EN_14},
    {"hermes.curator.run.count", "created_bucket", SMD_EN_15},
    {"hermes.curator.run.count", "merged_bucket", SMD_EN_16},
    {"hermes.curator.run.count", "outcome", SMD_EN_17},
    {"hermes.curator.run.count", "patched_bucket", SMD_EN_18},
    {"hermes.curator.run.count", "trigger", SMD_EN_19},
    {"hermes.delegation.run.count", "depth", SMD_EN_20},
    {"hermes.delegation.run.count", "mode", SMD_EN_21},
    {"hermes.delegation.run.count", "outcome", SMD_EN_22},
    {"hermes.delegation.run.count", "subagent_count_bucket", SMD_EN_23},
    {"hermes.desktop.action_use", "action", SMD_EN_24},
    {"hermes.desktop.action_use", "count_bucket", SMD_EN_25},
    {"hermes.desktop.action_use", "via", SMD_EN_26},
    {"hermes.desktop.dislike", "direction", SMD_EN_27},
    {"hermes.desktop.dislike", "signal", SMD_EN_28},
    {"hermes.desktop.dislike", "target", SMD_EN_29},
    {"hermes.desktop.feature_use", "area", SMD_EN_30},
    {"hermes.desktop.friction", "detail", SMD_EN_31},
    {"hermes.desktop.friction", "kind", SMD_EN_32},
    {"hermes.desktop.mode_use", "active_minutes_bucket", SMD_EN_33},
    {"hermes.desktop.mode_use", "bot_count_bucket", SMD_EN_34},
    {"hermes.desktop.mode_use", "messages_sent_bucket", SMD_EN_35},
    {"hermes.desktop.mode_use", "mode", SMD_EN_36},
    {"hermes.desktop.onboarding", "event", SMD_EN_37},
    {"hermes.desktop.onboarding", "step", SMD_EN_38},
    {"hermes.engagement.day.count", "active_minutes_bucket", SMD_EN_39},
    {"hermes.engagement.day.count", "active_profile_count_bucket", SMD_EN_40},
    {"hermes.engagement.day.count", "gateway_peak_busy_chats_bucket", SMD_EN_41},
    {"hermes.engagement.day.count", "surfaces_used_count", SMD_EN_42},
    {"hermes.engagement.surface_day.count", "active_minutes_bucket", SMD_EN_43},
    {"hermes.engagement.surface_day.count", "surface", SMD_EN_44},
    {"hermes.execution_backend.count", "backend", SMD_EN_45},
    {"hermes.execution_backend.count", "error_class", SMD_EN_46},
    {"hermes.execution_backend.count", "kind", SMD_EN_47},
    {"hermes.execution_backend.count", "outcome", SMD_EN_48},
    {"hermes.extension.install.count", "failure_class", SMD_EN_49},
    {"hermes.extension.install.count", "kind", SMD_EN_50},
    {"hermes.extension.install.count", "outcome", SMD_EN_51},
    {"hermes.extension.install.count", "registry", SMD_EN_52},
    {"hermes.extension.install.count", "source", SMD_EN_53},
    {"hermes.fallback.count", "error_class", SMD_EN_54},
    {"hermes.feature_adoption.count", "days_since_install_bucket", SMD_EN_55},
    {"hermes.feature_adoption.count", "feature", SMD_EN_56},
    {"hermes.feature_disabled.count", "event", SMD_EN_57},
    {"hermes.feature_disabled.count", "kind", SMD_EN_58},
    {"hermes.feature_disabled.count", "surface", SMD_EN_59},
    {"hermes.file_edit.count", "match_strategy", SMD_EN_60},
    {"hermes.file_edit.count", "mode", SMD_EN_61},
    {"hermes.file_edit.count", "outcome", SMD_EN_62},
    {"hermes.file_edit.count", "tool", SMD_EN_63},
    {"hermes.gateway.busy_arrival.count", "disposition", SMD_EN_64},
    {"hermes.gateway.busy_arrival.count", "wait_bucket", SMD_EN_65},
    {"hermes.gateway.reply_latency", "first_response_bucket", SMD_EN_66},
    {"hermes.install.milestone", "install_age_bucket", SMD_EN_67},
    {"hermes.install.milestone", "milestone", SMD_EN_68},
    {"hermes.install.run", "duration_bucket", SMD_EN_69},
    {"hermes.install.run", "failed_stage", SMD_EN_70},
    {"hermes.install.run", "failure_class", SMD_EN_71},
    {"hermes.install.run", "installer", SMD_EN_72},
    {"hermes.install.run", "outcome", SMD_EN_73},
    {"hermes.install.snapshot", "behind_bucket", SMD_EN_74},
    {"hermes.install.snapshot", "compression_prepare_ahead", SMD_EN_75},
    {"hermes.install.snapshot", "cron_job_count_bucket", SMD_EN_76},
    {"hermes.install.snapshot", "gpu_class", SMD_EN_77},
    {"hermes.install.snapshot", "install_age_bucket", SMD_EN_78},
    {"hermes.install.snapshot", "local_model_provider_used", SMD_EN_79},
    {"hermes.install.snapshot", "mcp_server_count_bucket", SMD_EN_80},
    {"hermes.install.snapshot", "messaging_platform_count_bucket", SMD_EN_81},
    {"hermes.install.snapshot", "plugin_count_bucket", SMD_EN_82},
    {"hermes.install.snapshot", "profile_count_bucket", SMD_EN_83},
    {"hermes.install.snapshot", "ram_bucket", SMD_EN_84},
    {"hermes.install.snapshot", "release_channel", SMD_EN_85},
    {"hermes.install.snapshot", "skill_count_bucket", SMD_EN_86},
    {"hermes.install.snapshot", "terminal_backend", SMD_EN_87},
    {"hermes.install.snapshot", "version_age_bucket", SMD_EN_88},
    {"hermes.loop_guard.count", "detector", SMD_EN_89},
    {"hermes.loop_guard.count", "signal", SMD_EN_90},
    {"hermes.memory.op.count", "failure_class", SMD_EN_91},
    {"hermes.memory.op.count", "op", SMD_EN_92},
    {"hermes.memory.op.count", "origin", SMD_EN_93},
    {"hermes.memory.op.count", "outcome", SMD_EN_94},
    {"hermes.model_call.count", "call_role", SMD_EN_95},
    {"hermes.model_call.count", "locality", SMD_EN_96},
    {"hermes.model_call.count", "model_family", SMD_EN_97},
    {"hermes.model_call.count", "outcome", SMD_EN_98},
    {"hermes.model_call.count", "provider_family", SMD_EN_99},
    {"hermes.model_friction.count", "signal", SMD_EN_100},
    {"hermes.model_reply_issue.count", "issue", SMD_EN_101},
    {"hermes.model_route.count", "call_role", SMD_EN_102},
    {"hermes.model_route.count", "error_class", SMD_EN_103},
    {"hermes.model_route.count", "outcome", SMD_EN_104},
    {"hermes.model_route.count", "ttft_bucket", SMD_EN_105},
    {"hermes.model_switch.count", "execution_surface", SMD_EN_106},
    {"hermes.model_switch_after.count", "turns_before_switch_bucket", SMD_EN_107},
    {"hermes.model_tokens.sum", "call_role", SMD_EN_108},
    {"hermes.model_tokens.sum", "token_type", SMD_EN_109},
    {"hermes.model_tool_quality.count", "call_role", SMD_EN_110},
    {"hermes.model_tool_quality.count", "issue", SMD_EN_111},
    {"hermes.platform.delivery", "failure_class", SMD_EN_112},
    {"hermes.platform.delivery", "outcome", SMD_EN_113},
    {"hermes.platform.health", "error_class", SMD_EN_114},
    {"hermes.platform.health", "event", SMD_EN_115},
    {"hermes.prepared_compaction.count", "event", SMD_EN_116},
    {"hermes.prepared_compaction.count", "failure_class", SMD_EN_117},
    {"hermes.prepared_compaction.count", "origin", SMD_EN_118},
    {"hermes.prepared_compaction.count", "reason", SMD_EN_119},
    {"hermes.prepared_compaction.count", "wait_bucket", SMD_EN_120},
    {"hermes.process.exit", "crash_class", SMD_EN_121},
    {"hermes.process.exit", "exit_kind", SMD_EN_122},
    {"hermes.process.exit", "process_kind", SMD_EN_123},
    {"hermes.provider_setup.count", "event", SMD_EN_124},
    {"hermes.provider_setup.count", "failure_class", SMD_EN_125},
    {"hermes.provider_setup.count", "surface", SMD_EN_126},
    {"hermes.session.count", "active_duration_bucket", SMD_EN_127},
    {"hermes.session.count", "entrypoint", SMD_EN_128},
    {"hermes.session.count", "execution_surface", SMD_EN_129},
    {"hermes.session.count", "failed_turn_count_bucket", SMD_EN_130},
    {"hermes.session.count", "last_outcome", SMD_EN_131},
    {"hermes.session.count", "message_count_bucket", SMD_EN_132},
    {"hermes.session.count", "model_call_count_bucket", SMD_EN_133},
    {"hermes.session.count", "tool_call_count_bucket", SMD_EN_134},
    {"hermes.session.count", "turn_count_bucket", SMD_EN_135},
    {"hermes.setup.completed", "surface", SMD_EN_136},
    {"hermes.skill.lifecycle.count", "action", SMD_EN_137},
    {"hermes.skill.lifecycle.count", "provenance", SMD_EN_138},
    {"hermes.skill.load.count", "post_patch_state", SMD_EN_139},
    {"hermes.skill.load.count", "provenance", SMD_EN_140},
    {"hermes.skill.load.count", "reuse_state", SMD_EN_141},
    {"hermes.skill.load.count", "use_count_bucket", SMD_EN_142},
    {"hermes.slash_command.count", "execution_surface", SMD_EN_143},
    {"hermes.startup.latency", "latency_bucket", SMD_EN_144},
    {"hermes.startup.latency", "surface", SMD_EN_145},
    {"hermes.task_cost.count", "api_calls_bucket", SMD_EN_146},
    {"hermes.task_cost.count", "outcome", SMD_EN_147},
    {"hermes.task_cost.count", "tokens_bucket", SMD_EN_148},
    {"hermes.task_cost.count", "tool_calls_bucket", SMD_EN_149},
    {"hermes.task_run.duration", "duration_bucket", SMD_EN_150},
    {"hermes.task_run.duration", "execution_surface", SMD_EN_151},
    {"hermes.task_run.duration", "outcome", SMD_EN_152},
    {"hermes.task_run.duration", "retry_count_bucket", SMD_EN_153},
    {"hermes.task_run.finished", "duration_bucket", SMD_EN_154},
    {"hermes.task_run.finished", "end_reason", SMD_EN_155},
    {"hermes.task_run.finished", "entrypoint", SMD_EN_156},
    {"hermes.task_run.finished", "execution_surface", SMD_EN_157},
    {"hermes.task_run.finished", "failure_class", SMD_EN_158},
    {"hermes.task_run.finished", "model_call_count_bucket", SMD_EN_159},
    {"hermes.task_run.finished", "outcome", SMD_EN_160},
    {"hermes.task_run.finished", "retry_count_bucket", SMD_EN_161},
    {"hermes.task_run.finished", "termination", SMD_EN_162},
    {"hermes.task_run.finished", "tool_call_count_bucket", SMD_EN_163},
    {"hermes.task_run.started", "entrypoint", SMD_EN_164},
    {"hermes.task_run.started", "execution_surface", SMD_EN_165},
    {"hermes.terminal.outcome.count", "backend", SMD_EN_166},
    {"hermes.terminal.outcome.count", "command_kind", SMD_EN_167},
    {"hermes.terminal.outcome.count", "outcome", SMD_EN_168},
    {"hermes.tool.usage.count", "error_class", SMD_EN_169},
    {"hermes.tool.usage.count", "outcome", SMD_EN_170},
    {"hermes.tool.usage.count", "tool_name", SMD_EN_171},
    {"hermes.tool_approval.count", "attribution", SMD_EN_172},
    {"hermes.tool_approval.count", "outcome", SMD_EN_173},
    {"hermes.tool_batch.count", "connector_share", SMD_EN_174},
    {"hermes.tool_batch.count", "size_bucket", SMD_EN_175},
    {"hermes.tool_call.count", "approval_outcome", SMD_EN_176},
    {"hermes.tool_call.count", "latency_bucket", SMD_EN_177},
    {"hermes.tool_call.count", "outcome", SMD_EN_178},
    {"hermes.tool_call.count", "retry_count_bucket", SMD_EN_179},
    {"hermes.tool_call.count", "tool_category", SMD_EN_180},
    {"hermes.tool_call.latency", "latency_bucket", SMD_EN_181},
    {"hermes.tool_call.latency", "retry_count_bucket", SMD_EN_182},
    {"hermes.tool_call.latency", "tool_category", SMD_EN_183},
    {"hermes.tool_enabled_unused.count", "toolset", SMD_EN_184},
    {"hermes.tool_enabled_unused.count", "used", SMD_EN_185},
    {"hermes.tool_output_truncation.count", "original_size_bucket", SMD_EN_186},
    {"hermes.tool_output_truncation.count", "tool", SMD_EN_187},
    {"hermes.tool_output_truncation.count", "truncated", SMD_EN_188},
    {"hermes.tool_overhead.count", "enabled_tool_count_bucket", SMD_EN_189},
    {"hermes.tool_overhead.count", "execution_surface", SMD_EN_190},
    {"hermes.tool_overhead.count", "tool_schema_tokens_bucket", SMD_EN_191},
    {"hermes.tool_recovery.count", "next_outcome", SMD_EN_192},
    {"hermes.tool_recovery.count", "next_tool", SMD_EN_193},
    {"hermes.tool_recovery.count", "tool", SMD_EN_194},
    {"hermes.tool_search.count", "hits_bucket", SMD_EN_195},
    {"hermes.tool_search.count", "outcome", SMD_EN_196},
    {"hermes.tool_search.count", "scope", SMD_EN_197},
    {"hermes.tool_unavailable.count", "tool_name", SMD_EN_198},
    {"hermes.update.run", "apply_mode", SMD_EN_199},
    {"hermes.update.run", "duration_bucket", SMD_EN_200},
    {"hermes.update.run", "failed_stage", SMD_EN_201},
    {"hermes.update.run", "failure_class", SMD_EN_202},
    {"hermes.update.run", "from_version_age_bucket", SMD_EN_203},
    {"hermes.update.run", "kind", SMD_EN_204},
    {"hermes.update.run", "outcome", SMD_EN_205},
    {"hermes.update.stage", "duration_bucket", SMD_EN_206},
    {"hermes.update.stage", "outcome", SMD_EN_207},
    {"hermes.update.stage", "stage", SMD_EN_208},
    {"hermes.wasted_tokens.count", "reason", SMD_EN_209},
    {"hermes.wasted_tokens.count", "tokens_bucket", SMD_EN_210},
    {NULL, NULL, NULL}
};

static const smd_field_pair CATALOG_FIELDS[] = {
    {"hermes.extension.install.count", "name"},
    {"hermes.gateway.busy_arrival.count", "platform"},
    {"hermes.gateway.reply_latency", "platform"},
    {"hermes.install.snapshot", "display_language"},
    {"hermes.install.snapshot", "memory_provider"},
    {"hermes.memory.op.count", "provider"},
    {"hermes.model_tokens.sum", "aux_task"},
    {"hermes.platform.delivery", "platform"},
    {"hermes.platform.health", "platform"},
    {"hermes.session.count", "platform"},
    {"hermes.skill.load.count", "skill_name"},
    {"hermes.slash_command.count", "command"},
    {"hermes.task_run.finished", "platform"},
    {"hermes.task_run.started", "platform"},
    {NULL, NULL}
};

static const smd_ident_entry IDENT_FIELDS[] = {
    {"hermes.cache_break.count", "model", 256},
    {"hermes.cache_break.count", "provider", 64},
    {"hermes.context_peak.count", "model", 256},
    {"hermes.context_peak.count", "provider", 64},
    {"hermes.desktop.dislike", "setting", 96},
    {"hermes.engagement.day.count", "primary_model", 256},
    {"hermes.engagement.day.count", "primary_provider", 64},
    {"hermes.fallback.count", "from_provider", 64},
    {"hermes.fallback.count", "to_provider", 64},
    {"hermes.feature_disabled.count", "name", 64},
    {"hermes.install.snapshot", "main_provider", 64},
    {"hermes.loop_guard.count", "model", 256},
    {"hermes.loop_guard.count", "provider", 64},
    {"hermes.model_friction.count", "model", 256},
    {"hermes.model_friction.count", "provider", 64},
    {"hermes.model_reply_issue.count", "model", 256},
    {"hermes.model_reply_issue.count", "provider", 64},
    {"hermes.model_route.count", "model", 256},
    {"hermes.model_route.count", "provider", 64},
    {"hermes.model_switch.count", "from_provider", 64},
    {"hermes.model_switch.count", "to_provider", 64},
    {"hermes.model_switch_after.count", "model", 256},
    {"hermes.model_switch_after.count", "provider", 64},
    {"hermes.model_tokens.sum", "model", 256},
    {"hermes.model_tokens.sum", "provider", 64},
    {"hermes.model_tool_quality.count", "model", 256},
    {"hermes.model_tool_quality.count", "provider", 64},
    {"hermes.provider_setup.count", "provider", 64},
    {"hermes.setup.completed", "provider", 64},
    {"hermes.task_cost.count", "model", 256},
    {"hermes.task_cost.count", "provider", 64},
    {"hermes.tool_batch.count", "model", 256},
    {"hermes.tool_batch.count", "provider", 64},
    {"hermes.tool_recovery.count", "model", 256},
    {"hermes.tool_recovery.count", "provider", 64},
    {"hermes.tool_unavailable.count", "model", 256},
    {"hermes.tool_unavailable.count", "provider", 64},
    {"hermes.wasted_tokens.count", "model", 256},
    {"hermes.wasted_tokens.count", "provider", 64},
    {NULL, NULL, 0}
};

static const char *const CONTRACT_METRICS[] = {
    "hermes.cache_break.count",
    "hermes.client.active",
    "hermes.compression.count",
    "hermes.connector_chain.count",
    "hermes.context_peak.count",
    "hermes.cron.run",
    "hermes.curator.run.count",
    "hermes.delegation.run.count",
    "hermes.desktop.action_use",
    "hermes.desktop.dislike",
    "hermes.desktop.feature_use",
    "hermes.desktop.friction",
    "hermes.desktop.mode_use",
    "hermes.desktop.onboarding",
    "hermes.engagement.day.count",
    "hermes.engagement.surface_day.count",
    "hermes.execution_backend.count",
    "hermes.extension.install.count",
    "hermes.fallback.count",
    "hermes.feature_adoption.count",
    "hermes.feature_disabled.count",
    "hermes.file_edit.count",
    "hermes.gateway.busy_arrival.count",
    "hermes.gateway.reply_latency",
    "hermes.install.milestone",
    "hermes.install.run",
    "hermes.install.snapshot",
    "hermes.loop_guard.count",
    "hermes.memory.op.count",
    "hermes.model_call.count",
    "hermes.model_friction.count",
    "hermes.model_reply_issue.count",
    "hermes.model_route.count",
    "hermes.model_switch.count",
    "hermes.model_switch_after.count",
    "hermes.model_tokens.sum",
    "hermes.model_tool_quality.count",
    "hermes.platform.delivery",
    "hermes.platform.health",
    "hermes.prepared_compaction.count",
    "hermes.process.exit",
    "hermes.provider_setup.count",
    "hermes.session.count",
    "hermes.setup.completed",
    "hermes.skill.lifecycle.count",
    "hermes.skill.load.count",
    "hermes.slash_command.count",
    "hermes.startup.latency",
    "hermes.task_cost.count",
    "hermes.task_run.duration",
    "hermes.task_run.finished",
    "hermes.task_run.started",
    "hermes.terminal.outcome.count",
    "hermes.tool.usage.count",
    "hermes.tool_approval.count",
    "hermes.tool_batch.count",
    "hermes.tool_call.count",
    "hermes.tool_call.latency",
    "hermes.tool_enabled_unused.count",
    "hermes.tool_output_truncation.count",
    "hermes.tool_overhead.count",
    "hermes.tool_recovery.count",
    "hermes.tool_search.count",
    "hermes.tool_unavailable.count",
    "hermes.update.run",
    "hermes.update.stage",
    "hermes.wasted_tokens.count",
    NULL
};

static const char *smd_metric_key(const json_t *metric_name) {
    return (metric_name && metric_name->type == JSON_STRING) ? metric_name->str_val : NULL;
}

/* Exact set equality between the input object's key set and a table row. */
static int smd_fields_equal(const json_t *dims, const char *const *fields) {
    size_t want = 0, i, j;
    if (!fields) return 0;
    if (!dims || dims->type != JSON_OBJECT) return 0;
    while (fields[want]) want++;
    if (want != dims->c.count) return 0;
    for (i = 0; i < dims->c.count; i++) {
        const char *key = dims->c.keys[i];
        int found = 0;
        for (j = 0; fields[j]; j++) {
            if (key && !strcmp(key, fields[j])) { found = 1; break; }
        }
        if (!found) return 0;
    }
    return 1;
}

static const char *const *smd_metric_fields_for(const char *metric) {
    size_t i;
    if (!metric) return NULL;
    for (i = 0; METRIC_FIELDS[i].metric; i++) {
        if (!strcmp(METRIC_FIELDS[i].metric, metric)) return METRIC_FIELDS[i].fields;
    }
    return NULL;
}

static int smd_legacy_match(const char *metric, const json_t *dims) {
    size_t i;
    if (!metric) return 0;
    for (i = 0; LEGACY_SETS[i].metric; i++) {
        if (strcmp(LEGACY_SETS[i].metric, metric)) continue;
        if (smd_fields_equal(dims, LEGACY_SETS[i].fields)) return 1;
    }
    return 0;
}

/* The `_COUNTER_DIMENSION_VALUES[metric][field]` closed set, looking through the
 * caller's catalog snapshot for the fields the contract backs with a catalog. */
static const char *const *smd_allowed_static(const char *metric, const char *field,
                                            size_t *count) {
    size_t i;
    *count = 0;
    for (i = 0; ENUM_VALUES[i].metric; i++) {
        if (!strcmp(ENUM_VALUES[i].metric, metric) && !strcmp(ENUM_VALUES[i].field, field)) {
            size_t n = 0;
            while (ENUM_VALUES[i].values[n]) n++;
            *count = n;
            return ENUM_VALUES[i].values;
        }
    }
    return NULL;
}

/* `_COUNTER_DIMENSION_VALUES[metric_name]` -- a missing key is a KeyError. */
static int smd_metric_known(const char *metric) {
    size_t i;
    if (!metric) return 0;
    for (i = 0; CONTRACT_METRICS[i]; i++) {
        if (!strcmp(CONTRACT_METRICS[i], metric)) return 1;
    }
    return 0;
}

static int smd_field_in_contract(const char *metric, const char *field) {
    size_t i;
    for (i = 0; ENUM_VALUES[i].metric; i++) {
        if (!strcmp(ENUM_VALUES[i].metric, metric) && !strcmp(ENUM_VALUES[i].field, field)) return 1;
    }
    for (i = 0; CATALOG_FIELDS[i].metric; i++) {
        if (!strcmp(CATALOG_FIELDS[i].metric, metric) && !strcmp(CATALOG_FIELDS[i].field, field)) return 1;
    }
    return 0;
}

static int smd_ident_max_length(const char *metric, const char *field, int *found) {
    size_t i;
    *found = 0;
    for (i = 0; IDENT_FIELDS[i].metric; i++) {
        if (!strcmp(IDENT_FIELDS[i].metric, metric) && !strcmp(IDENT_FIELDS[i].field, field)) {
            *found = 1;
            return IDENT_FIELDS[i].max_length;
        }
    }
    return 0;
}

/* `_metric_identifier(value, max_length=...)`, caller frees. */
char *w21_sharedmetr_b6c0a5_metric_identifier(const char *value, int max_length) {
    static const char *const START = "abcdefghijklmnopqrstuvwxyz0123456789";
    static const char *const CHARS = "abcdefghijklmnopqrstuvwxyz0123456789._:/@+-";
    const char *p, *end;
    char *out;
    size_t n, i;

    if (!value) return NULL;  /* not a str -> this port's caller passes NULL */
    p = value;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f') p++;
    end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r' ||
                       end[-1] == '\v' || end[-1] == '\f')) end--;
    n = (size_t)(end - p);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = p[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';

    if (n == 0 || (long)n > (long)max_length || !strchr(START, out[0])) {
        free(out);
        return NULL;  /* "unknown" */
    }
    for (i = 0; i < n; i++) {
        if (!out[i] || !strchr(CHARS, out[i])) { free(out); return NULL; }
    }
    return out;
}

json_t *w21_sharedmetr_b6c0a5_shape_known(const json_t *metric_name, const json_t *dimensions) {
    const char *metric = smd_metric_key(metric_name);
    const char *const *fields;
    if (!metric) return json_bool(0);
    fields = smd_metric_fields_for(metric);
    if (fields && smd_fields_equal(dimensions, fields)) return json_bool(1);
    return json_bool(smd_legacy_match(metric, dimensions));
}

static json_t *smd_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    return out;
}

static json_t *smd_key_error(const char *key) {
    char msg[512];
    json_t *out = json_object();
    snprintf(msg, sizeof(msg), "'%s'", key ? key : "");
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("KeyError"));
    json_set(out, "message", json_string(msg));
    return out;
}

/* `value in allowed` over a static set or the caller's catalog list. */
static int smd_in_allowed(const json_t *catalog, const char *metric, const char *field,
                          const char *value) {
    size_t n = 0, i;
    const char *const *set = smd_allowed_static(metric, field, &n);
    if (set) {
        for (i = 0; i < n; i++) {
            if (set[i] && !strcmp(set[i], value)) return 1;
        }
        return 0;
    }
    if (catalog && catalog->type == JSON_OBJECT) {
        json_t *per_metric = json_obj_get(catalog, metric);
        json_t *list = (per_metric && per_metric->type == JSON_OBJECT) ? json_obj_get(per_metric, field) : NULL;
        if (list && list->type == JSON_ARRAY) {
            for (i = 0; i < list->c.count; i++) {
                json_t *v = list->c.items[i];
                if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, value)) return 1;
            }
        }
    }
    return 0;
}

/* `next((b for b in ("custom","other","unknown") if b in allowed), None)` */
static const char *smd_fallback(const json_t *catalog, const char *metric, const char *field) {
    static const char *const BUCKETS[] = {"custom", "other", "unknown", NULL};
    int i;
    for (i = 0; BUCKETS[i]; i++) {
        if (smd_in_allowed(catalog, metric, field, BUCKETS[i])) return BUCKETS[i];
    }
    return NULL;
}

json_t *w21_sharedmetr_b6c0a5_conform(const json_t *metric_name, const json_t *dimensions,
                                      const json_t *catalog) {
    const char *metric = smd_metric_key(metric_name);
    json_t *conformed;
    size_t i;

    if (!metric || !smd_metric_known(metric)) return smd_key_error(metric);
    if (!dimensions || dimensions->type != JSON_OBJECT) return smd_ok(json_null());

    conformed = json_object();
    for (i = 0; i < dimensions->c.count; i++) {
        const char *field = dimensions->c.keys[i];
        json_t *value = dimensions->c.items[i];
        int ident_found = 0;
        int max_length;

        /* `if not isinstance(value, str): return None` */
        if (!value || value->type != JSON_STRING || !value->str_val) {
            json_free(conformed);
            return smd_ok(json_null());
        }
        max_length = smd_ident_max_length(metric, field, &ident_found);
        if (ident_found) {
            char *ident = w21_sharedmetr_b6c0a5_metric_identifier(value->str_val, max_length);
            int same = ident && !strcmp(ident, value->str_val);
            free(ident);
            if (!same) {
                json_free(conformed);
                return smd_ok(json_null());
            }
            json_set(conformed, field, json_copy(value));
            continue;
        }
        if (!smd_field_in_contract(metric, field)) {
            json_free(conformed);
            return smd_key_error(field);
        }
        if (smd_in_allowed(catalog, metric, field, value->str_val)) {
            json_set(conformed, field, json_copy(value));
            continue;
        }
        {
            const char *bucket = smd_fallback(catalog, metric, field);
            if (!bucket) {
                json_free(conformed);
                return smd_ok(json_null());
            }
            json_set(conformed, field, json_string(bucket));
        }
    }
    return smd_ok(conformed);
}

json_t *w21_sharedmetr_b6c0a5_contract_shape(void) {
    json_t *out = json_object();
    size_t i, metrics = 0, slots = 0, legacy = 0, enums = 0, values = 0, catalog = 0, idents = 0;
    for (i = 0; METRIC_FIELDS[i].metric; i++) {
        size_t j = 0;
        metrics++;
        while (METRIC_FIELDS[i].fields[j]) j++;
        slots += j;
    }
    for (i = 0; LEGACY_SETS[i].metric; i++) legacy++;
    for (i = 0; ENUM_VALUES[i].metric; i++) {
        size_t j = 0;
        enums++;
        while (ENUM_VALUES[i].values[j]) j++;
        values += j;
    }
    for (i = 0; CATALOG_FIELDS[i].metric; i++) catalog++;
    for (i = 0; IDENT_FIELDS[i].metric; i++) idents++;
    json_set(out, "metrics", json_number((double)metrics));
    json_set(out, "metric_field_slots", json_number((double)slots));
    json_set(out, "legacy_sets", json_number((double)legacy));
    json_set(out, "static_enum_fields", json_number((double)enums));
    json_set(out, "catalog_fields", json_number((double)catalog));
    json_set(out, "ident_fields", json_number((double)idents));
    json_set(out, "enum_values", json_number((double)values));
    return out;
}
