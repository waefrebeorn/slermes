/*
 * anon_sign_in.c — C home for upstream Hermes hermes_cli/anon_sign_in.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_anon_sign_in_pending @ hermes_cli/anon_sign_in.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_START = "Sign in with a Nous account to unlock more models and tools.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_ALREADY_SIGNED_IN = "Already signed in.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_DO_NOT_SHARE = "Do not share this code.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_TIMED_OUT = "That sign-in link has expired. Start again whenever you're ready.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_NOT_COMPLETED = "Sign-in didn't finish. Try again whenever you're ready.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_NO_DEFAULT_TERMINAL = "No default model is set yet; run `hermes model` to pick one.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_NO_DEFAULT_CHAT = "No default model is set yet; run /model to pick one.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_WAITING = "Waiting for sign-in...";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_WAITING_UP_TO = "Waiting for sign-in, up to {minutes}.";
static const char *const hermes_cli_anon_sign_in_CONST_UPGRADE_CANCELLED = "\\nSign-in cancelled.";
static const char *const hermes_cli_anon_sign_in_CONST_LOGIN_COMMAND = "/login";
static const char *const hermes_cli_anon_sign_in_CONST_LOGIN_STARTING = "Starting sign-in...";
static const char *const hermes_cli_anon_sign_in_CONST_LOGIN_DM_ONLY = "Sign in from a direct message with Hermes.";
static const char *const hermes_cli_anon_sign_in_CONST_LOGIN_BUSY_ELSEWHERE = "Another sign-in is already running on this Hermes. Try again in a few minutes.";
static const char *const hermes_cli_anon_sign_in_CONST_LOGIN_NOT_ALLOWED = "Only an operator of this Hermes can sign it in.";

void hermes_cli_anon_sign_in_port_pending(void) { (void)0; }
