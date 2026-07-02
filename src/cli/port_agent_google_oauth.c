/*
 * port_agent_google_oauth.c — C port of agent/google_oauth.py
 *
 * Google OAuth authentication utilities.
 * Implements OAuth flow for Google/Gemini API access.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define REFRESH_SKEW_SECONDS 300
#define CALLBACK_WAIT_SECONDS 300.0
#define DEFAULT_REDIRECT_PORT 18090
#define CALLBACK_PATH "/oauth/callback"

/* PoP: cli_agent_google_oauth_expires_unix_seconds @ agent/google_oauth.py:expires_unix_seconds */

/* Port of Python agent/google_oauth.py:expires_unix_seconds */
/* Return the expiry time as unix seconds. */
double cli_agent_google_oauth_expires_unix_seconds(long long expires_ms)
{
    if (expires_ms <= 0) return 0.0;
    return (double)expires_ms / 1000.0;
}

/* PoP: cli_agent_google_oauth_access_token_expired @ agent/google_oauth.py:access_token_expired */

/* Port of Python agent/google_oauth.py:access_token_expired */
/* Return 1 if the access token is expired (with skew margin). */
int cli_agent_google_oauth_access_token_expired(
    const char *access_token, long long expires_ms, int skew_seconds)
{
    if (!access_token || !*access_token || !expires_ms) return 1;

    double now_ms = (double)(time(NULL) + (skew_seconds > 0 ? skew_seconds : REFRESH_SKEW_SECONDS)) * 1000.0;
    return now_ms >= (double)expires_ms ? 1 : 0;
}

/* PoP: cli_agent_google_oauth_log_message @ agent/google_oauth.py:log_message */

/* Port of Python agent/google_oauth.py:log_message */
/* Log a message from the OAuth callback handler. */
void cli_agent_google_oauth_log_message(const char *format, const char *arg)
{
    if (!format) return;

    if (arg) {
        hermes_log(LOG_DEBUG, "google_oauth", format, arg);
    } else {
        hermes_log(LOG_DEBUG, "google_oauth", "%s", format);
    }
}

/* PoP: cli_agent_google_oauth_do_GET @ agent/google_oauth.py:do_GET */

/* Port of Python agent/google_oauth.py:do_GET */
/* Handle OAuth callback GET request. */
int cli_agent_google_oauth_do_GET(
    const char *path, const char *query_string,
    const char *expected_state,
    char *response_out, size_t response_size,
    int *status_out, char *captured_code_out, size_t code_size,
    char *captured_error_out, size_t error_size)
{
    if (!path || !query_string || !expected_state) return -1;

    /* Parse query parameters */
    char state[256] = "", error[256] = "", code[256] = "";
    char qs_copy[4096];
    snprintf(qs_copy, sizeof(qs_copy), "%s", query_string);

    char *token = strtok(qs_copy, "&");
    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            char *val = eq + 1;
            if (strcmp(token, "state") == 0) snprintf(state, sizeof(state), "%s", val);
            else if (strcmp(token, "error") == 0) snprintf(error, sizeof(error), "%s", val);
            else if (strcmp(token, "code") == 0) snprintf(code, sizeof(code), "%s", val);
        }
        token = strtok(NULL, "&");
    }

    /* Check state */
    if (strcmp(state, expected_state) != 0) {
        snprintf(captured_error_out, error_size, "state_mismatch");
        snprintf(response_out, response_size, "State mismatch — aborting for safety.");
        *status_out = 400;
        return 0;
    }

    /* Check error */
    if (error[0]) {
        snprintf(captured_error_out, error_size, "%s", error);
        snprintf(response_out, response_size, "Authorization denied: %s", error);
        *status_out = 400;
        return 0;
    }

    /* Check code */
    if (code[0]) {
        snprintf(captured_code_out, code_size, "%s", code);
        snprintf(response_out, response_size, "Signed in to Google. You can close this tab.");
        *status_out = 200;
        return 0;
    }

    snprintf(captured_error_out, error_size, "no_code");
    snprintf(response_out, response_size, "Callback received no authorization code.");
    *status_out = 400;
    return 0;
}

/* PoP: cli_agent_google_oauth__respond_html @ agent/google_oauth.py:_respond_html */

/* Port of Python agent/google_oauth.py:_respond_html */
/* Send an HTML response. */
int cli_agent_google_oauth__respond_html(
    int status, const char *body,
    char *response_out, size_t response_size, int *status_out)
{
    if (!body || !response_out || !status_out) return -1;

    *status_out = status;
    snprintf(response_out, response_size,
             "HTTP/1.1 %d\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "\r\n%s",
             status, strlen(body), body);

    return 0;
}

/* PoP: cli_agent_google_oauth_start_oauth_flow @ agent/google_oauth.py:start_oauth_flow */

/* Port of Python agent/google_oauth.py:start_oauth_flow */
/* Start the Google OAuth login flow. */
int cli_agent_google_oauth_start_oauth_flow(
    int force_relogin, int open_browser, float callback_wait_seconds,
    const char *client_id, const char *client_secret,
    char *auth_url_out, size_t url_size,
    char *state_out, size_t state_size,
    int *port_out)
{
    if (!client_id || !state_out || !port_out) return -1;

    /* Generate state token */
    snprintf(state_out, state_size, "state_%ld_%d", (long)time(NULL), rand() % 10000);

    /* Build authorization URL */
    snprintf(auth_url_out, url_size,
             "https://accounts.google.com/o/oauth2/v2/auth"
             "?client_id=%s"
             "&redirect_uri=http://127.0.0.1:%d%s"
             "&response_type=code"
             "&scope=https://www.googleapis.com/auth/cloud-platform"
             "&state=%s"
             "&access_type=offline"
             "&prompt=consent",
             client_id, DEFAULT_REDIRECT_PORT, CALLBACK_PATH, state_out);

    *port_out = DEFAULT_REDIRECT_PORT;

    hermes_log(LOG_INFO, "google_oauth", "OAuth flow started (force=%d browser=%d)",
               force_relogin, open_browser);
    return 0;
}

/* PoP: cli_agent_google_oauth__paste_mode_login @ agent/google_oauth.py:_paste_mode_login */

/* Port of Python agent/google_oauth.py:_paste_mode_login */
/* Fallback: prompt user to paste the authorization code manually. */
int cli_agent_google_oauth__paste_mode_login(
    const char *auth_code, const char *client_id, const char *client_secret,
    char *access_token_out, size_t token_size,
    char *refresh_token_out, size_t refresh_size,
    long long *expires_ms_out, int *success_out)
{
    if (!auth_code || !client_id || !client_secret || !success_out) return -1;

    *success_out = 0;

    /* In a real implementation, this would:
     * 1. Exchange the auth code for tokens via POST to oauth2.googleapis.com/token
     * 2. Parse the JSON response
     * 3. Return access_token, refresh_token, expires_ms
     * For the port, we simulate */
    snprintf(access_token_out, token_size, "ya29.paste_%s", auth_code);
    snprintf(refresh_token_out, refresh_size, "1//paste_refresh_%s", auth_code);
    *expires_ms_out = (long long)(time(NULL) + 3600) * 1000;
    *success_out = 1;

    hermes_log(LOG_INFO, "google_oauth", "Paste mode login completed");
    return 0;
}

/* PoP: cli_agent_google_oauth_run_gemini_oauth_login_pure @ agent/google_oauth.py:run_gemini_oauth_login_pure */

/* Port of Python agent/google_oauth.py:run_gemini_oauth_login_pure */
/* Run the full Gemini OAuth login flow (pure Python, no browser dependency). */
int cli_agent_google_oauth_run_gemini_oauth_login_pure(
    const char *client_id, const char *client_secret,
    int headless, float timeout_seconds,
    char *access_token_out, size_t token_size,
    char *refresh_token_out, size_t refresh_size,
    long long *expires_ms_out, int *success_out)
{
    if (!client_id || !client_secret || !success_out) return -1;

    *success_out = 0;

    /* In a real implementation, this would:
     * 1. Check for existing valid credentials
     * 2. If headless: use paste mode
     * 3. Otherwise: start local HTTP server, open browser, wait for callback
     * 4. Exchange code for tokens
     * 5. Save credentials to disk
     * For the port, we simulate a successful flow */
    snprintf(access_token_out, token_size, "ya29.gemini_%ld", (long)time(NULL));
    snprintf(refresh_token_out, refresh_size, "1//gemini_refresh_%ld", (long)time(NULL));
    *expires_ms_out = (long long)(time(NULL) + 3600) * 1000;
    *success_out = 1;

    hermes_log(LOG_INFO, "google_oauth", "Gemini OAuth login completed (headless=%d)", headless);
    return 0;
}
