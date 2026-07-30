/*
 * port_agent_google_oauth.c — C port of agent/google_oauth.py
 *
 * Google OAuth authentication utilities.
 * Implements OAuth flow for Google/Gemini API access.
 */

#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
/* Fallback: exchange the pasted authorization code for tokens via a real
 * POST to oauth2.googleapis.com/token (grant_type=authorization_code). */
int cli_agent_google_oauth__paste_mode_login(
    const char *auth_code, const char *client_id, const char *client_secret,
    char *access_token_out, size_t token_size,
    char *refresh_token_out, size_t refresh_size,
    long long *expires_ms_out, int *success_out)
{
    if (!auth_code || !client_id || !client_secret || !success_out) return -1;

    *success_out = 0;
    if (access_token_out && token_size) access_token_out[0] = '\0';
    if (refresh_token_out && refresh_size) refresh_token_out[0] = '\0';
    if (expires_ms_out) *expires_ms_out = 0;

    char body[2048];
    snprintf(body, sizeof(body),
             "code=%s&client_id=%s&client_secret=%s&grant_type=authorization_code",
             auth_code, client_id, client_secret);

    char headers[128];
    snprintf(headers, sizeof(headers), "Content-Type: application/x-www-form-urlencoded");

    http_t *http = http_new(30);
    if (!http) return -1;

    http_resp_t *res = http_request(http, HTTP_POST,
                                    "https://oauth2.googleapis.com/token",
                                    headers, body, strlen(body));
    int rc = -1;
    if (res && res->status >= 200 && res->status < 300 && res->body) {
        json_t *doc = json_parse(res->body, NULL);
        if (doc && doc->type == JSON_OBJECT) {
            const char *at = json_get_str(doc, "access_token", NULL);
            const char *rt = json_get_str(doc, "refresh_token", NULL);
            double expires_in = json_get_num(doc, "expires_in", 0);
            if (at) {
                if (access_token_out) snprintf(access_token_out, token_size, "%s", at);
                if (refresh_token_out && rt) snprintf(refresh_token_out, refresh_size, "%s", rt);
                if (expires_ms_out) *expires_ms_out = (long long)(time(NULL) + (long)expires_in) * 1000;
                *success_out = 1;
                rc = 0;
                hermes_log(LOG_INFO, "google_oauth", "Paste mode login exchanged code for tokens");
            } else {
                const char *err = json_get_str(doc, "error_description", json_get_str(doc, "error", "unknown"));
                hermes_log(LOG_ERROR, "google_oauth", "token exchange failed: %s", err ? err : "unknown");
            }
        }
        if (doc) json_free(doc);
    } else {
        hermes_log(LOG_ERROR, "google_oauth", "token endpoint HTTP %d", res ? res->status : -1);
    }
    if (res) http_resp_free(res);
    http_free(http);
    return rc;
}

/* PoP: cli_agent_google_oauth_run_gemini_oauth_login_pure @ agent/google_oauth.py:run_gemini_oauth_login_pure */
/* Run the full Gemini OAuth login flow (pure C, no browser dependency).
 * Headless: exchange the supplied code directly. Otherwise: start a real
 * local HTTP callback server, build the consent URL, wait for the redirect
 * carrying ?code=..., then exchange it. */
int cli_agent_google_oauth_run_gemini_oauth_login_pure(
    const char *client_id, const char *client_secret,
    int headless, float timeout_seconds,
    char *access_token_out, size_t token_size,
    char *refresh_token_out, size_t refresh_size,
    long long *expires_ms_out, int *success_out)
{
    if (!client_id || !client_secret || !success_out) return -1;

    *success_out = 0;
    if (access_token_out && token_size) access_token_out[0] = '\0';
    if (refresh_token_out && refresh_size) refresh_token_out[0] = '\0';
    if (expires_ms_out) *expires_ms_out = 0;

    /* Headless: caller must have supplied the code via auth_code path already.
     * In pure-headless mode there is no browser; report that a code is needed. */
    if (headless) {
        hermes_log(LOG_WARNING, "google_oauth",
                   "Gemini OAuth headless flow requires an authorization code; "
                   "use _paste_mode_login with the pasted code");
        return -1;
    }

    /* Interactive: spin up a real local callback server. */
    int port = DEFAULT_REDIRECT_PORT;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return -1;
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv);
        return -1;
    }
    listen(srv, 1);

    char redirect_uri[256];
    snprintf(redirect_uri, sizeof(redirect_uri), "http://localhost:%d%s", port, CALLBACK_PATH);
    hermes_log(LOG_INFO, "google_oauth",
               "Open this URL in your browser to authorize:\n"
               "https://accounts.google.com/o/oauth2/v2/auth?client_id=%s&redirect_uri=%s"
               "&response_type=code&scope=https://www.googleapis.com/auth/generative-language",
               client_id, redirect_uri);

    int cli = accept(srv, NULL, NULL);
    char code[1024] = {0};
    if (cli >= 0) {
        char buf[4096] = {0};
        ssize_t n = read(cli, buf, sizeof(buf) - 1);
        if (n > 0) {
            /* Parse ?code=... from the GET request line. */
            const char *q = strstr(buf, "GET ");
            if (q) {
                const char *c = strstr(q, "code=");
                if (c) {
                    c += 5;
                    const char *end = strpbrk(c, " &\n\r");
                    size_t len = end ? (size_t)(end - c) : strlen(c);
                    if (len < sizeof(code)) memcpy(code, c, len);
                }
            }
            /* Respond to the browser. */
            const char *resp =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                "<html><body><h2>Gemini authorized.</h2>You may close this tab.</body></html>";
            write(cli, resp, strlen(resp));
        }
        close(cli);
    }
    close(srv);

    if (!code[0]) {
        hermes_log(LOG_ERROR, "google_oauth", "No authorization code received from callback");
        return -1;
    }

    /* Exchange the code for tokens (reuse the real exchange). */
    return cli_agent_google_oauth__paste_mode_login(code, client_id, client_secret,
                                                     access_token_out, token_size,
                                                     refresh_token_out, refresh_size,
                                                     expires_ms_out, success_out);
}
