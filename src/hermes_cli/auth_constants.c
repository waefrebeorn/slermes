/*
 * auth_constants.c — C home for upstream Hermes hermes_cli/auth_constants.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_auth_constants_pending @ hermes_cli/auth_constants.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static long long hermes_cli_auth_constants_CONST_AUTH_STORE_VERSION = 1;
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_NOUS_PORTAL_URL = "https://portal.nousresearch.com";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_NOUS_INFERENCE_URL = "https://inference-api.nousresearch.com/v1";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_NOUS_WELCOME_URL = "https://welcome-api.nousresearch.com/v1";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_NOUS_CLIENT_ID = "hermes-cli";
static const char *const hermes_cli_auth_constants_CONST_WELCOME_CLAIM_FLAG = "welcome_claim";
static const char *const hermes_cli_auth_constants_CONST_NOUS_INFERENCE_INVOKE_SCOPE = "inference:invoke";
static const char *const hermes_cli_auth_constants_CONST_NOUS_BILLING_MANAGE_SCOPE = "billing:manage";
static const char *const hermes_cli_auth_constants_CONST_NOUS_DEVICE_CODE_SOURCE = "device_code";
static const char *const hermes_cli_auth_constants_CONST_NOUS_AUTH_PATH_INVOKE_JWT = "invoke_jwt";
static const char *const hermes_cli_auth_constants_CONST_DEVICE_CODE_GRANT_TYPE = "urn:ietf:params:oauth:grant-type:device_code";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_CODEX_BASE_URL = "https://chatgpt.com/backend-api/codex";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_XAI_OAUTH_BASE_URL = "https://api.x.ai/v1";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_CLIENT_ID = "78257093-7e40-4613-99e0-527b14b39113";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_SCOPE = "group_id profile model.completion";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_GRANT_TYPE = "urn:ietf:params:oauth:grant-type:user_code";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_GLOBAL_BASE = "https://api.minimax.io";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_CN_BASE = "https://api.minimaxi.com";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_GLOBAL_INFERENCE = "https://api.minimax.io/anthropic";
static const char *const hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_CN_INFERENCE = "https://api.minimaxi.com/anthropic";
static long long hermes_cli_auth_constants_CONST_MINIMAX_OAUTH_REFRESH_SKEW_SECONDS = 60;
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_QWEN_BASE_URL = "https://portal.qwen.ai/v1";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_OLLAMA_CLOUD_BASE_URL = "https://ollama.com/v1";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_ACTUAL_BASE_URL = "https://api.actual.inc/v1";
static const char *const hermes_cli_auth_constants_CONST_DEFAULT_ACTUAL_LOCAL_BASE_URL = "http://127.0.0.1:8080/v1";
static const char *const hermes_cli_auth_constants_CONST_STEPFUN_STEP_PLAN_INTL_BASE_URL = "https://api.stepfun.ai/step_plan/v1";
static const char *const hermes_cli_auth_constants_CONST_STEPFUN_STEP_PLAN_CN_BASE_URL = "https://api.stepfun.com/step_plan/v1";
static const char *const hermes_cli_auth_constants_CONST_CODEX_OAUTH_CLIENT_ID = "app_EMoamEEZ73f0CkXaXp7hrann";
static const char *const hermes_cli_auth_constants_CONST_CODEX_OAUTH_TOKEN_URL = "https://auth.openai.com/oauth/token";
static long long hermes_cli_auth_constants_CONST_CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS = 120;
static const char *const hermes_cli_auth_constants_CONST_XAI_OAUTH_ISSUER = "https://auth.x.ai";
static const char *const hermes_cli_auth_constants_CONST_XAI_OAUTH_CLIENT_ID = "b1a00492-073a-47ea-816f-4c329264a828";
static const char *const hermes_cli_auth_constants_CONST_XAI_OAUTH_SCOPE = "openid profile email offline_access grok-cli:access api:access";
static long long hermes_cli_auth_constants_CONST_XAI_ACCESS_TOKEN_REFRESH_SKEW_SECONDS = 3600;
static const char *const hermes_cli_auth_constants_CONST_QWEN_OAUTH_CLIENT_ID = "f0304373b74a44d2b584a3fb70ca9e56";
static const char *const hermes_cli_auth_constants_CONST_QWEN_OAUTH_TOKEN_URL = "https://chat.qwen.ai/api/v1/oauth2/token";
static long long hermes_cli_auth_constants_CONST_QWEN_ACCESS_TOKEN_REFRESH_SKEW_SECONDS = 120;
static const char *const hermes_cli_auth_constants_CONST_OPENROUTER_AUTH_URL = "https://openrouter.ai/auth";
static const char *const hermes_cli_auth_constants_CONST_OPENROUTER_AUTH_KEYS_URL = "https://openrouter.ai/api/v1/auth/keys";
static const char *const hermes_cli_auth_constants_CONST_OPENROUTER_OAUTH_DOCS_URL = "https://openrouter.ai/docs/guides/overview/auth/oauth";
static const char *const hermes_cli_auth_constants_CONST_OAUTH_OVER_SSH_DOCS_URL = "https://hermes-agent.nousresearch.com/docs/guides/oauth-over-ssh";
static const char *const hermes_cli_auth_constants_CONST_LMSTUDIO_NOAUTH_PLACEHOLDER = "dummy-lm-api-key";
static const char *const hermes_cli_auth_constants_CONST_ACTUAL_LOCAL_NOAUTH_PLACEHOLDER = "dummy-actual-local-api-key";
static const char *const hermes_cli_auth_constants_CONST_CODEX_RATE_LIMITED_CODE = "codex_rate_limited";

void hermes_cli_auth_constants_port_pending(void) { (void)0; }
