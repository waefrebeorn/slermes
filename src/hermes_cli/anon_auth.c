/*
 * anon_auth.c — C home for upstream Hermes hermes_cli/anon_auth.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_anon_auth_pending @ hermes_cli/anon_auth.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const hermes_cli_anon_auth_CONST_ANON_AUTH_METHOD = "anonymous";
static const char *const hermes_cli_anon_auth_CONST_ANON_CLIENT_ID = "nas-anonymous";
static const char *const hermes_cli_anon_auth_CONST_ANON_ACCOUNT_TIER = "anonymous";
static const char *const hermes_cli_anon_auth_CONST_GUEST_MODEL = "nous/welcome";
static const char *const hermes_cli_anon_auth_CONST_ANON_SECRET_HEADER = "x-anonymous-api-secret";
static const char *const hermes_cli_anon_auth_CONST_ANON_SECRET_ENV = "HERMES_ANON_API_SECRET";
static const char *const hermes_cli_anon_auth_CONST_PREVIEW_FULL_CONNECTORS_ENV = "HERMES_PREVIEW_FULL_CONNECTORS";
static const char *const hermes_cli_anon_auth_CONST_FREE_TIER_LABEL = "Nous · free tier";
static const char *const hermes_cli_anon_auth_CONST_UPGRADE_HINT = "Run `hermes auth upgrade` to sign in with a Nous account, or /login inside a chat.";
static const char *const hermes_cli_anon_auth_CONST_EXTRA_WELCOME_HOSTS_ENV = "HERMES_EXTRA_WELCOME_HOSTS";
static const char *const hermes_cli_anon_auth_CONST_MODEL_SWITCH_HEADER = "x-nous-model-switch";
static const char *const hermes_cli_anon_auth_CONST_GUEST_NOTICE_FLAG = "guest_notice_shown";
static const char *const hermes_cli_anon_auth_CONST_UPGRADED_AUTH_METHOD = "oauth_device_code";

void hermes_cli_anon_auth_port_pending(void) { (void)0; }
