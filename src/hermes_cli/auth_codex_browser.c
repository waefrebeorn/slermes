/*
 * auth_codex_browser.c — C home for upstream Hermes hermes_cli/auth_codex_browser.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_auth_codex_browser_pending @ hermes_cli/auth_codex_browser.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const hermes_cli_auth_codex_browser_CONST_CODEX_OAUTH_AUTHORIZE_URL = "https://auth.openai.com/oauth/authorize";
static const char *const hermes_cli_auth_codex_browser_CONST_CODEX_OAUTH_BROWSER_SCOPE = "openid profile email offline_access";
static const char *const hermes_cli_auth_codex_browser_CONST_CODEX_BROWSER_CALLBACK_PORT = 1455;
static const char *const hermes_cli_auth_codex_browser_CONST_CODEX_BROWSER_CALLBACK_PATH = "/auth/callback";
static const char *const hermes_cli_auth_codex_browser_CONST_CODEX_BROWSER_PORT_BUSY_CODE = "codex_browser_port_busy";

void hermes_cli_auth_codex_browser_port_pending(void) { (void)0; }
