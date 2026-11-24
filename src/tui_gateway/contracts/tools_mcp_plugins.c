/*
 * tools_mcp_plugins.c — C home for upstream Hermes tui_gateway/contracts/tools_mcp_plugins.py.
 * Gate: w32_toolsmcppl_39bf88.  Verified parity over the gate fixture corpus
 * (tests/fixtures/w32_toolsmcppl_39bf88.jsonl) against live pydantic via
 * tests/oracle_w32_toolsmcppl_39bf88.py.
 *
 * The upstream module is declarations only: every `method(...)` call pins a
 * JSON-RPC method name to a Params model (extra="forbid", populate_by_name)
 * and a Result model (extra="forbid", or "allow" for OpenModel rows).  This
 * file holds the field tables transcribed from those declarations and the
 * pure decisions they imply: contract lookup, schema descriptors, unknown /
 * missing / mistyped-key error shaping, default normalisation and the
 * profile-scoped predicate.
 *
 * SEAMS (identical to the header, restated where they bite):
 *   - pydantic lax coercion is not emulated; JSON-native types only.
 *   - nested model bodies are checked as objects, not field-by-field.
 *   - mcp.catalog / mcp.servers.list / mcp.servers.status own their Params in
 *     common.py and are therefore outside this table.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w32_toolsmcppl_39bf88.h"

/* ── field tables transcribed from tools_mcp_plugins.py ─────────────────── */

typedef struct {
    const char *name;
    const char *tag;        /* str int float bool list dict model enum any */
    const char *ref;        /* enum / model type name, else NULL            */
    int required;
    int nullable;           /* the annotation was `X | None`                */
    int has_default;
    const char *def_json;   /* raw JSON text of the declared default        */
} field_t;

typedef struct {
    const char *name;
    const field_t *f;
    int n;
    int open;               /* model_config extra == "allow"                */
} model_t;

typedef struct {
    const char *method;
    const char *params;
    const char *result;
} method_t;

typedef struct {
    const char *name;
    const char *const *values;
    int n;
} enum_t;

static const field_t F__SessionScoped[] = {

    {"session_id", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_ToolsetsListResult[] = {

    {"toolsets", "list", NULL, 1, 0, 0, "null"},

};

static const field_t F_ToolsShowResult[] = {

    {"sections", "list", NULL, 1, 0, 0, "null"},
    {"total", "int", NULL, 1, 0, 0, "null"},

};

static const field_t F_ToolsConfigureParams[] = {

    {"action", "enum", "ToolsAction", 1, 0, 0, "null"},
    {"names", "list", NULL, 1, 0, 0, "null"},
    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"profile", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_ToolsConfigureResult[] = {

    {"changed", "list", NULL, 1, 0, 0, "null"},
    {"enabled_toolsets", "list", NULL, 1, 0, 0, "null"},
    {"info", "model", "SessionLiveInfo", 0, 1, 1, "null"},
    {"missing_servers", "list", NULL, 1, 0, 0, "null"},
    {"reset", "bool", NULL, 1, 0, 0, "null"},
    {"unknown", "list", NULL, 1, 0, 0, "null"},

};

static const field_t F_ReloadEnvParams[] = {

    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"profile", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_ReloadEnvResult[] = {

    {"updated", "int", NULL, 1, 0, 0, "null"},

};

static const field_t F_ReloadMcpParams[] = {

    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"confirm", "bool", NULL, 0, 0, 1, "false"},
    {"always", "bool", NULL, 0, 0, 1, "false"},
    {"rev", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_ReloadMcpResult[] = {

    {"status", "enum", "ReloadMcpStatus", 1, 0, 0, "null"},
    {"message", "str", NULL, 0, 1, 1, "null"},
    {"loaded_rev", "str", NULL, 0, 1, 1, "null"},
    {"coalesced", "bool", NULL, 0, 1, 1, "null"},
    {"turn_isolation", "bool", NULL, 0, 1, 1, "null"},
    {"host_ack", "any", NULL, 0, 1, 1, "null"},
    {"session", "enum", "ReloadMcpSession", 0, 1, 1, "null"},
    {"unavailable", "list", NULL, 0, 1, 1, "null"},
    {"session_servers", "list", NULL, 0, 1, 1, "null"},

};

static const field_t F_SkillsManageParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"action", "enum", "SkillsAction", 0, 0, 1, "\"list\""},
    {"query", "str", NULL, 0, 1, 1, "null"},
    {"page", "int", NULL, 0, 1, 1, "null"},
    {"page_size", "int", NULL, 0, 1, 1, "null"},

};

static const field_t F_SkillsManageResult[] = {

    {"skills", "dict", NULL, 0, 1, 1, "null"},
    {"results", "list", NULL, 0, 1, 1, "null"},
    {"installed", "bool", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 0, 1, 1, "null"},
    {"items", "list", NULL, 0, 1, 1, "null"},
    {"page", "int", NULL, 0, 1, 1, "null"},
    {"total_pages", "int", NULL, 0, 1, 1, "null"},
    {"total", "int", NULL, 0, 1, 1, "null"},
    {"info", "model", "SkillInspectInfo", 0, 1, 1, "null"},

};

static const field_t F_SkillsReloadParams[] = {

    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"profile", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_SkillsReloadResult[] = {

    {"output", "str", NULL, 1, 0, 0, "null"},
    {"result", "model", "SkillsReloadDiff", 1, 0, 0, "null"},

};

static const field_t F_LearningFramesParams[] = {

    {"cols", "int", NULL, 0, 1, 1, "null"},
    {"rows", "int", NULL, 0, 1, 1, "null"},
    {"frames", "int", NULL, 0, 1, 1, "null"},

};

static const field_t F_LearningFramesResult[] = {

    {"frames", "list", NULL, 1, 0, 0, "null"},
    {"legend", "list", NULL, 1, 0, 0, "null"},
    {"categories", "list", NULL, 1, 0, 0, "null"},
    {"buckets", "list", NULL, 1, 0, 0, "null"},
    {"summary", "list", NULL, 1, 0, 0, "null"},
    {"axis", "model", "LearningAxis", 1, 0, 0, "null"},
    {"count", "int", NULL, 1, 0, 0, "null"},
    {"cols", "int", NULL, 1, 0, 0, "null"},
    {"rows", "int", NULL, 1, 0, 0, "null"},

};

static const field_t F_LearningNodeParams[] = {

    {"id", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_LearningDetailResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"message", "str", NULL, 0, 1, 1, "null"},
    {"kind", "str", NULL, 0, 1, 1, "null"},
    {"id", "str", NULL, 0, 1, 1, "null"},
    {"label", "str", NULL, 0, 1, 1, "null"},
    {"content", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_LearningMutationResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"message", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_LearningEditParams[] = {

    {"id", "str", NULL, 0, 1, 1, "null"},
    {"content", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpServersAddParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"preset", "str", NULL, 0, 1, 1, "null"},
    {"config", "dict", NULL, 0, 1, 1, "null"},
    {"bearer_token", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpServersAddResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"server", "model", "McpServerSummary", 1, 0, 0, "null"},

};

static const field_t F_McpServersSetApiKeyParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"value", "str", NULL, 1, 0, 0, "null"},
    {"env_var", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpServersSetApiKeyResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"env_var", "str", NULL, 1, 0, 0, "null"},
    {"server", "model", "McpServerSummary", 1, 0, 0, "null"},

};

static const field_t F_McpServerNameParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},

};

static const field_t F_McpServersTestResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"tools", "list", NULL, 1, 0, 0, "null"},
    {"error", "str", NULL, 0, 1, 1, "null"},
    {"prompts", "int", NULL, 0, 1, 1, "null"},
    {"resources", "int", NULL, 0, 1, 1, "null"},
    {"oauth_needed", "bool", NULL, 1, 0, 0, "null"},
    {"oauth_tokens_present", "bool", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpServersRemoveResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"removed", "bool", NULL, 1, 0, 0, "null"},

};

static const field_t F_McpOauthStartParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"client_redirect_uri", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpOauthStartResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"session_id", "str", NULL, 1, 0, 0, "null"},
    {"auth_url", "str", NULL, 1, 0, 0, "null"},
    {"flow", "str", NULL, 1, 0, 0, "null"},

};

static const field_t F_McpOauthFlowParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"session_id", "str", NULL, 1, 0, 0, "null"},

};

static const field_t F_McpOauthPollResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"status", "enum", "McpOauthPollStatus", 1, 0, 0, "null"},
    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"error_message", "str", NULL, 0, 1, 1, "null"},
    {"auth_url", "str", NULL, 0, 1, 1, "null"},
    {"tools", "list", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpOauthCancelResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"status", "str", NULL, 0, 1, 1, "null"},
    {"error_message", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpOauthCallbackParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},
    {"session_id", "str", NULL, 1, 0, 0, "null"},
    {"code", "str", NULL, 0, 1, 1, "null"},
    {"state", "str", NULL, 0, 1, 1, "null"},
    {"error", "str", NULL, 0, 1, 1, "null"},
    {"iss", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_McpOauthCallbackResult[] = {

    {"ok", "bool", NULL, 1, 0, 0, "null"},
    {"session_id", "str", NULL, 0, 1, 1, "null"},
    {"error_message", "str", NULL, 0, 1, 1, "null"},

};

static const field_t F_PluginsListParams[] = {

    {NULL, NULL, NULL, 0, 0, 0, NULL}

};

static const field_t F_PluginsListResult[] = {

    {"plugins", "list", NULL, 1, 0, 0, "null"},
    {"toolsets", "list", NULL, 0, 1, 1, "null"},
    {"portable_mcp_servers", "list", NULL, 0, 1, 1, "null"},

};

static const field_t F_PluginsManageParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"action", "enum", "PluginsAction", 0, 0, 1, "\"list\""},
    {"key", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 0, 1, 1, "null"},
    {"enable", "bool", NULL, 0, 1, 1, "null"},
    {"identifier", "str", NULL, 0, 1, 1, "null"},
    {"repo", "str", NULL, 0, 1, 1, "null"},
    {"catalog_name", "str", NULL, 0, 1, 1, "null"},
    {"force", "bool", NULL, 0, 1, 1, "null"},
    {"ref", "str", NULL, 0, 1, 1, "null"},
    {"inspected_sha", "str", NULL, 0, 1, 1, "null"},
    {"accept_capabilities", "bool", NULL, 0, 1, 1, "null"},
    {"values", "dict", NULL, 0, 1, 1, "null"},
    {"names", "list", NULL, 0, 1, 1, "null"},

};

static const field_t F_PluginsManageResult[] = {

    {"plugins", "list", NULL, 0, 1, 1, "null"},
    {"user_count", "int", NULL, 0, 1, 1, "null"},
    {"bundled_count", "int", NULL, 0, 1, 1, "null"},
    {"ok", "bool", NULL, 0, 1, 1, "null"},
    {"unchanged", "bool", NULL, 0, 1, 1, "null"},
    {"restart_required", "bool", NULL, 0, 1, 1, "null"},
    {"gateway_reloaded", "bool", NULL, 0, 1, 1, "null"},
    {"activation", "model", "PluginActivation", 0, 1, 1, "null"},
    {"cleared_memory_provider", "bool", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 0, 1, 1, "null"},
    {"plugin", "model", "AgentPluginRow", 0, 1, 1, "null"},
    {"plugin_name", "str", NULL, 0, 1, 1, "null"},
    {"warnings", "list", NULL, 0, 1, 1, "null"},
    {"missing_env", "list", NULL, 0, 1, 1, "null"},
    {"python_dependencies", "list", NULL, 0, 1, 1, "null"},
    {"known_issues", "list", NULL, 0, 1, 1, "null"},
    {"after_install_path", "str", NULL, 0, 1, 1, "null"},
    {"enabled", "bool", NULL, 0, 1, 1, "null"},
    {"sha", "str", NULL, 0, 1, 1, "null"},
    {"consent_required", "bool", NULL, 0, 1, 1, "null"},
    {"delta", "dict", NULL, 0, 1, 1, "null"},
    {"delta_lines", "list", NULL, 0, 1, 1, "null"},
    {"scan_blocked", "bool", NULL, 0, 1, 1, "null"},
    {"scan_verdict", "str", NULL, 0, 1, 1, "null"},
    {"scan_findings", "list", NULL, 0, 1, 1, "null"},
    {"error", "str", NULL, 0, 1, 1, "null"},
    {"written", "list", NULL, 0, 1, 1, "null"},
    {"onboarding", "list", NULL, 0, 1, 1, "null"},
    {"presence", "list", NULL, 0, 1, 1, "null"},
    {"agent", "bool", NULL, 0, 1, 1, "null"},
    {"desktop", "bool", NULL, 0, 1, 1, "null"},
    {"agent_name", "str", NULL, 0, 1, 1, "null"},
    {"desktop_name", "str", NULL, 0, 1, 1, "null"},
    {"insecure", "bool", NULL, 0, 1, 1, "null"},
    {"path", "str", NULL, 0, 1, 1, "null"},
    {"paired_with_agent", "bool", NULL, 0, 1, 1, "null"},
    {"unpaired_from", "str", NULL, 0, 1, 1, "null"},
    {"results", "list", NULL, 0, 1, 1, "null"},
    {"skipped", "list", NULL, 0, 1, 1, "null"},
    {"current", "int", NULL, 0, 1, 1, "null"},
    {"auto_apply", "bool", NULL, 0, 1, 1, "null"},
    {"check_hours", "float", NULL, 0, 1, 1, "null"},

};

static const field_t F_PluginCatalogLookupParams[] = {

    {"profile", "str", NULL, 0, 1, 1, "null"},
    {"name", "str", NULL, 1, 0, 0, "null"},

};

static const field_t F_PluginCatalogLookupResult[] = {

    {"status", "enum", "PluginCatalogLookupStatus", 1, 0, 0, "null"},
    {"entry", "model", "PluginCatalogLookupEntry", 0, 1, 1, "null"},

};

static const method_t METHODS[] = {
    {"tools.list", "_SessionScoped", "ToolsetsListResult"},
    {"toolsets.list", "_SessionScoped", "ToolsetsListResult"},
    {"tools.show", "_SessionScoped", "ToolsShowResult"},
    {"tools.configure", "ToolsConfigureParams", "ToolsConfigureResult"},
    {"reload.env", "ReloadEnvParams", "ReloadEnvResult"},
    {"reload.mcp", "ReloadMcpParams", "ReloadMcpResult"},
    {"skills.manage", "SkillsManageParams", "SkillsManageResult"},
    {"skills.reload", "SkillsReloadParams", "SkillsReloadResult"},
    {"learning.frames", "LearningFramesParams", "LearningFramesResult"},
    {"learning.detail", "LearningNodeParams", "LearningDetailResult"},
    {"learning.delete", "LearningNodeParams", "LearningMutationResult"},
    {"learning.edit", "LearningEditParams", "LearningMutationResult"},
    {"mcp.servers.add", "McpServersAddParams", "McpServersAddResult"},
    {"mcp.servers.set_api_key", "McpServersSetApiKeyParams", "McpServersSetApiKeyResult"},
    {"mcp.servers.test", "McpServerNameParams", "McpServersTestResult"},
    {"mcp.servers.remove", "McpServerNameParams", "McpServersRemoveResult"},
    {"mcp.servers.oauth.start", "McpOauthStartParams", "McpOauthStartResult"},
    {"mcp.servers.oauth.poll", "McpOauthFlowParams", "McpOauthPollResult"},
    {"mcp.servers.oauth.cancel", "McpOauthFlowParams", "McpOauthCancelResult"},
    {"mcp.servers.oauth.callback", "McpOauthCallbackParams", "McpOauthCallbackResult"},
    {"plugins.list", "PluginsListParams", "PluginsListResult"},
    {"plugins.manage", "PluginsManageParams", "PluginsManageResult"},
    {"plugins.catalog.lookup", "PluginCatalogLookupParams", "PluginCatalogLookupResult"},
};

static const model_t MODELS[] = {
    {"_SessionScoped", F__SessionScoped, 1, 0},
    {"ToolsetsListResult", F_ToolsetsListResult, 1, 0},
    {"ToolsShowResult", F_ToolsShowResult, 2, 0},
    {"ToolsConfigureParams", F_ToolsConfigureParams, 4, 0},
    {"ToolsConfigureResult", F_ToolsConfigureResult, 6, 0},
    {"ReloadEnvParams", F_ReloadEnvParams, 2, 0},
    {"ReloadEnvResult", F_ReloadEnvResult, 1, 0},
    {"ReloadMcpParams", F_ReloadMcpParams, 4, 0},
    {"ReloadMcpResult", F_ReloadMcpResult, 9, 0},
    {"SkillsManageParams", F_SkillsManageParams, 5, 0},
    {"SkillsManageResult", F_SkillsManageResult, 9, 0},
    {"SkillsReloadParams", F_SkillsReloadParams, 2, 0},
    {"SkillsReloadResult", F_SkillsReloadResult, 2, 0},
    {"LearningFramesParams", F_LearningFramesParams, 3, 0},
    {"LearningFramesResult", F_LearningFramesResult, 9, 0},
    {"LearningNodeParams", F_LearningNodeParams, 1, 0},
    {"LearningDetailResult", F_LearningDetailResult, 6, 0},
    {"LearningMutationResult", F_LearningMutationResult, 2, 0},
    {"LearningEditParams", F_LearningEditParams, 2, 0},
    {"McpServersAddParams", F_McpServersAddParams, 5, 0},
    {"McpServersAddResult", F_McpServersAddResult, 3, 0},
    {"McpServersSetApiKeyParams", F_McpServersSetApiKeyParams, 4, 0},
    {"McpServersSetApiKeyResult", F_McpServersSetApiKeyResult, 4, 0},
    {"McpServerNameParams", F_McpServerNameParams, 2, 0},
    {"McpServersTestResult", F_McpServersTestResult, 7, 0},
    {"McpServersRemoveResult", F_McpServersRemoveResult, 2, 0},
    {"McpOauthStartParams", F_McpOauthStartParams, 3, 0},
    {"McpOauthStartResult", F_McpOauthStartResult, 4, 0},
    {"McpOauthFlowParams", F_McpOauthFlowParams, 3, 0},
    {"McpOauthPollResult", F_McpOauthPollResult, 6, 0},
    {"McpOauthCancelResult", F_McpOauthCancelResult, 3, 0},
    {"McpOauthCallbackParams", F_McpOauthCallbackParams, 7, 0},
    {"McpOauthCallbackResult", F_McpOauthCallbackResult, 3, 0},
    {"PluginsListParams", F_PluginsListParams, 0, 0},
    {"PluginsListResult", F_PluginsListResult, 3, 0},
    {"PluginsManageParams", F_PluginsManageParams, 14, 0},
    {"PluginsManageResult", F_PluginsManageResult, 42, 0},
    {"PluginCatalogLookupParams", F_PluginCatalogLookupParams, 2, 0},
    {"PluginCatalogLookupResult", F_PluginCatalogLookupResult, 2, 0},
};

static const enum_t ENUMS[] = {
    {"McpOauthPollStatus", (const char *[]){"pending", "approved", "error"}, 3},
    {"McpRuntimeStatus", (const char *[]){"connected", "disabled", "connecting", "failed", "lazy", "configured"}, 6},
    {"McpServerSource", (const char *[]){"config", "plugin"}, 2},
    {"PluginCatalogLookupStatus", (const char *[]){"found", "invalid_name", "unknown", "unavailable"}, 4},
    {"PluginRestartTarget", (const char *[]){"gateway", "backend"}, 2},
    {"PluginServerState", (const char *[]){"connected", "app_not_running", "hermes_not_connected", "endpoint_unavailable", "no_interactive_session", "version_too_old", "missing_app", "unsupported_gpu", "unknown"}, 9},
    {"PluginSettingFieldType", (const char *[]){"string", "number", "boolean", "enum", "secret", "json"}, 6},
    {"PluginUpdateAllStatus", (const char *[]){"updated", "current", "failed", "needs_review"}, 4},
    {"PluginsAction", (const char *[]){"list", "toggle", "install", "inspect", "install_desktop", "remove_desktop", "update", "remove", "settings", "onboarding", "presence", "update_all", "auto_update"}, 13},
    {"ReloadMcpSession", (const char *[]){"applied", "unchanged", "busy"}, 3},
    {"ReloadMcpStatus", (const char *[]){"confirm_required", "reloaded"}, 2},
    {"SkillsAction", (const char *[]){"list", "search", "install", "browse", "inspect"}, 5},
    {"ToolsAction", (const char *[]){"enable", "disable"}, 2},
};

static int name_cmp(const void *pa, const void *pb) {
    return strcmp(*(const char *const *)pa, *(const char *const *)pb);
}

/* ── lookups ───────────────────────────────────────────────────────────── */

static size_t method_count(void) { return sizeof(METHODS) / sizeof(METHODS[0]); }
static size_t model_count(void)  { return sizeof(MODELS) / sizeof(MODELS[0]); }
static size_t enum_count(void)   { return sizeof(ENUMS) / sizeof(ENUMS[0]); }

/* PoP: w32_toolsmcppl_39bf88_find_method @ tui_gateway/contracts/tools_mcp_plugins.py:registry.METHODS */
static const method_t *find_method(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < method_count(); i++)
        if (strcmp(METHODS[i].method, name) == 0) return &METHODS[i];
    return NULL;
}

/* PoP: w32_toolsmcppl_39bf88_find_model @ tui_gateway/contracts/base.py:Params|Result:model_fields */
static const model_t *find_model(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < model_count(); i++)
        if (strcmp(MODELS[i].name, name) == 0) return &MODELS[i];
    return NULL;
}

/* PoP: w32_toolsmcppl_39bf88_find_enum @ tui_gateway/contracts/base.py:WireEnum */
static const enum_t *find_enum(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < enum_count(); i++)
        if (strcmp(ENUMS[i].name, name) == 0) return &ENUMS[i];
    return NULL;
}

static const field_t *find_field(const model_t *m, const char *name) {
    int i;
    if (!m || !name) return NULL;
    for (i = 0; i < m->n; i++)
        if (strcmp(m->f[i].name, name) == 0) return &m->f[i];
    return NULL;
}

/* ── error shaping ─────────────────────────────────────────────────────── */

typedef struct {
    char *loc;
    const char *code;
} err_t;

typedef struct {
    err_t *v;
    size_t n;
    size_t cap;
} errs_t;

/* PoP: w32_toolsmcppl_39bf88_errs_add @ tui_gateway/contracts/registry.py:ValidationError.errors */
static void errs_add(errs_t *e, const char *loc, const char *code) {
    char *dup;
    if (e->n == e->cap) {
        size_t cap = e->cap ? e->cap * 2 : 8;
        err_t *v = (err_t *)realloc(e->v, cap * sizeof(*v));
        if (!v) return;
        e->v = v;
        e->cap = cap;
    }
    dup = (char *)malloc(strlen(loc) + 1);
    if (!dup) return;
    strcpy(dup, loc);
    e->v[e->n].loc = dup;
    e->v[e->n].code = code;
    e->n++;
}

static void errs_free(errs_t *e) {
    size_t i;
    for (i = 0; i < e->n; i++) free(e->v[i].loc);
    free(e->v);
    e->v = NULL;
    e->n = 0;
    e->cap = 0;
}

static int err_cmp(const void *pa, const void *pb) {
    const err_t *a = (const err_t *)pa;
    const err_t *b = (const err_t *)pb;
    int c = strcmp(a->loc, b->loc);
    return c ? c : strcmp(a->code, b->code);
}

/* PoP: w32_toolsmcppl_39bf88_verdict @ tui_gateway/contracts/registry.py:validate_params */
static json_t *verdict(const errs_t *e) {
    json_t *out = json_object();
    json_t *arr = json_array();
    size_t i;
    err_t *sorted = NULL;
    if (!out || !arr) { json_free(out); json_free(arr); return json_null(); }
    if (e->n) {
        sorted = (err_t *)malloc(e->n * sizeof(*sorted));
        if (sorted) {
            memcpy(sorted, e->v, e->n * sizeof(*sorted));
            qsort(sorted, e->n, sizeof(*sorted), err_cmp);
            for (i = 0; i < e->n; i++) {
                json_t *row = json_object();
                json_set(row, "loc", json_string(sorted[i].loc));
                json_set(row, "type", json_string(sorted[i].code));
                json_append(arr, row);
            }
            free(sorted);
        }
    }
    json_set(out, "ok", json_bool(e->n == 0));
    json_set(out, "errors", arr);
    return out;
}

/* ── type checks ───────────────────────────────────────────────────────── */

/* PoP: w32_toolsmcppl_39bf88_null_code @ tui_gateway/contracts/base.py:Params:model_validate */
static const char *null_code(const field_t *f) {
    if (!strcmp(f->tag, "str"))   return "type_str";
    if (!strcmp(f->tag, "int"))   return "type_int";
    if (!strcmp(f->tag, "float")) return "type_float";
    if (!strcmp(f->tag, "bool"))  return "type_bool";
    if (!strcmp(f->tag, "list"))  return "type_list";
    if (!strcmp(f->tag, "dict"))  return "type_dict";
    if (!strcmp(f->tag, "model")) return "type_object";
    if (!strcmp(f->tag, "enum"))  return "enum";
    return NULL;
}

/* PoP: w32_toolsmcppl_39bf88_type_problem @ tui_gateway/contracts/base.py:Params:model_validate */
static const char *type_problem(const field_t *f, const json_t *v) {
    const enum_t *en;
    int i;
    if (json_type(v) == JSON_NULL) return f->nullable ? NULL : null_code(f);
    if (!strcmp(f->tag, "str"))   return json_is_string(v) ? NULL : "type_str";
    if (!strcmp(f->tag, "int")) {
        double d;
        if (!json_is_number(v)) return "type_int";
        d = v->num_val;
        return d == (double)(long long)d ? NULL : "type_int";
    }
    if (!strcmp(f->tag, "float")) return json_is_number(v) ? NULL : "type_float";
    if (!strcmp(f->tag, "bool"))  return json_is_bool(v) ? NULL : "type_bool";
    if (!strcmp(f->tag, "list"))  return json_is_array(v) ? NULL : "type_list";
    if (!strcmp(f->tag, "dict"))  return json_is_object(v) ? NULL : "type_dict";
    if (!strcmp(f->tag, "model")) return json_is_object(v) ? NULL : "type_object";
    if (!strcmp(f->tag, "enum")) {
        if (!json_is_string(v)) return "enum";
        en = find_enum(f->ref);
        if (!en) return NULL;
        for (i = 0; i < en->n; i++)
            if (strcmp(en->values[i], v->str_val) == 0) return NULL;
        return "enum";
    }
    return NULL; /* any */
}

/* ── the check itself ──────────────────────────────────────────────────── */

/* PoP: w32_toolsmcppl_39bf88_check_model @ tui_gateway/contracts/registry.py:canonical_param_problems */
static void check_model(const model_t *m, const json_t *v, const char *root, errs_t *e) {
    int i;
    size_t k, total;
    if (!m) { errs_add(e, root, "type_object"); return; }
    if (!json_is_object(v)) { errs_add(e, root, "type_object"); return; }
    for (i = 0; i < m->n; i++) {
        const field_t *f = &m->f[i];
        json_t *got = json_obj_get(v, f->name);
        const char *code;
        if (!got) {
            if (f->required) errs_add(e, f->name, "missing");
            continue;
        }
        code = type_problem(f, got);
        if (code) errs_add(e, f->name, code);
    }
    if (m->open) return;
    total = json_object_size(v);
    for (k = 0; k < total; k++) {
        const char *key = json_object_get_key_at(v, k);
        if (!key) continue;
        if (!find_field(m, key)) errs_add(e, key, "extra");
    }
}

/* ── public surface ────────────────────────────────────────────────────── */

/* PoP: w32_toolsmcppl_39bf88_methods @ tui_gateway/contracts/tools_mcp_plugins.py:method(<23 names>) */
json_t *w32_toolsmcppl_39bf88_methods(void) {
    json_t *out = json_object();
    size_t i;
    if (!out) return json_null();
    for (i = 0; i < method_count(); i++) {
        json_t *row = json_object();
        json_set(row, "params", json_string(METHODS[i].params));
        json_set(row, "result", json_string(METHODS[i].result));
        json_set(out, METHODS[i].method, row);
    }
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_enums @ tui_gateway/contracts/tools_mcp_plugins.py:<13 WireEnum classes> */
json_t *w32_toolsmcppl_39bf88_enums(void) {
    json_t *out = json_object();
    size_t i;
    if (!out) return json_null();
    for (i = 0; i < enum_count(); i++) {
        json_t *arr = json_array();
        int j;
        for (j = 0; j < ENUMS[i].n; j++) json_append(arr, json_string(ENUMS[i].values[j]));
        json_set(out, ENUMS[i].name, arr);
    }
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_params_spec @ tui_gateway/contracts/tools_mcp_plugins.py:<Params models> */
json_t *w32_toolsmcppl_39bf88_params_spec(const char *method) {
    const method_t *m = find_method(method);
    const model_t *pm;
    json_t *out;
    int i;
    if (!m) return json_string("UNKNOWN_METHOD");
    pm = find_model(m->params);
    out = json_object();
    if (!pm || !out) { json_free(out); return json_null(); }
    for (i = 0; i < pm->n; i++) {
        const field_t *f = &pm->f[i];
        json_t *row = json_object();
        const enum_t *en = f->ref ? find_enum(f->ref) : NULL;
        json_set(row, "required", json_bool(f->required != 0));
        json_set(row, "nullable", json_bool(f->nullable != 0));
        json_set(row, "tag", json_string(f->tag));
        json_set(row, "ref", f->ref ? json_string(f->ref) : json_null());
        if (en) {
            json_t *choices = json_array();
            int j;
            for (j = 0; j < en->n; j++) json_append(choices, json_string(en->values[j]));
            json_set(row, "choices", choices);
        } else {
            json_set(row, "choices", json_null());
        }
        json_set(row, "has_default", json_bool(f->has_default != 0));
        json_set(row, "default", json_parse(f->def_json ? f->def_json : "null", NULL));
        json_set(out, f->name, row);
    }
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_validate_params @ tui_gateway/contracts/registry.py:validate_params */
json_t *w32_toolsmcppl_39bf88_validate_params(const char *method, const json_t *params) {
    const method_t *m = find_method(method);
    errs_t e;
    json_t *out;
    if (!m) return json_string("UNKNOWN_METHOD");
    memset(&e, 0, sizeof(e));
    check_model(find_model(m->params), params, "params", &e);
    out = verdict(&e);
    errs_free(&e);
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_normalize_params @ tui_gateway/contracts/tools_mcp_plugins.py:Params.model_dump */
json_t *w32_toolsmcppl_39bf88_normalize_params(const char *method, const json_t *params) {
    const method_t *m = find_method(method);
    const model_t *pm;
    errs_t e;
    json_t *out, *norm;
    int i;
    if (!m) return json_string("UNKNOWN_METHOD");
    pm = find_model(m->params);
    memset(&e, 0, sizeof(e));
    check_model(pm, params, "params", &e);
    if (e.n) {
        out = verdict(&e);
        errs_free(&e);
        return out;
    }
    errs_free(&e);
    norm = json_object();
    out = json_object();
    if (!norm || !out) { json_free(norm); json_free(out); return json_null(); }
    for (i = 0; i < pm->n; i++) {
        const field_t *f = &pm->f[i];
        json_t *got = json_obj_get(params, f->name);
        json_set(norm, f->name, got ? json_copy(got) : json_parse(f->def_json ? f->def_json : "null", NULL));
    }
    json_set(out, "ok", json_bool(true));
    json_set(out, "params", norm);
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_validate_result @ tui_gateway/contracts/registry.py:_report */
json_t *w32_toolsmcppl_39bf88_validate_result(const char *method, const json_t *result) {
    const method_t *m = find_method(method);
    errs_t e;
    json_t *out;
    if (!m) return json_string("UNKNOWN_METHOD");
    memset(&e, 0, sizeof(e));
    check_model(find_model(m->result), result, "result", &e);
    out = verdict(&e);
    errs_free(&e);
    return out;
}

/* PoP: w32_toolsmcppl_39bf88_profile_scoped @ tui_gateway/contracts/tools_mcp_plugins.py:<module docstring> */
json_t *w32_toolsmcppl_39bf88_profile_scoped(void) {
    const char *names[64];
    size_t n = 0, i;
    json_t *arr;
    for (i = 0; i < method_count(); i++) {
        const model_t *pm = find_model(METHODS[i].params);
        if (find_field(pm, "profile") && n < sizeof(names) / sizeof(names[0])) names[n++] = METHODS[i].method;
    }
    qsort((void *)names, n, sizeof(names[0]), name_cmp);
    arr = json_array();
    if (!arr) return json_null();
    for (i = 0; i < n; i++) json_append(arr, json_string(names[i]));
    return arr;
}
