/*
 * port_hermes_cli_dashboard_auth_prefix.c — C port of hermes_cli/dashboard_auth/prefix.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_dashboard_auth_prefix__warn_if_malformed @ hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed */
/* Warns (once per distinct value) when a non-empty public-url value was rejected. */
void cli_hermes_cli_dashboard_auth_prefix__warn_if_malformed(
    const char *source, const char *raw)
{
    if (!raw || !raw[0]) {
        return;
    }
    /* Check if the value looks like it's missing a scheme. */
    if (strncmp(raw, "http://", 7) != 0 && strncmp(raw, "https://", 8) != 0) {
        hermes_log(LOG_WARNING, "dashboard_auth",
                   "%s is set to '%s' but was ignored because it is not a valid "
                   "absolute URL — it must include an http:// or https:// scheme. "
                   "Falling back to reconstructing the OAuth redirect URI from "
                   "request headers.",
                   source, raw);
    }
}

/* PoP: cli_hermes_cli_dashboard_auth_prefix_normalise_prefix @ hermes_cli/dashboard_auth/prefix.py:normalise_prefix */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:normalise_prefix */
/* Normalises an X-Forwarded-Prefix header value. */
/* Returns a string like "/hermes" (no trailing slash) or "" when invalid. */
int cli_hermes_cli_dashboard_auth_prefix_normalise_prefix(
    const char *raw, char *output, size_t output_size)
{
    if (!raw || !output || output_size == 0) {
        return -1;
    }
    output[0] = '\0';
    /* Skip leading whitespace. */
    while (*raw == ' ' || *raw == '\t') raw++;
    if (!*raw) {
        return 0;
    }
    /* Must start with '/'. */
    if (*raw != '/') {
        if (output_size < 2) return -1;
        output[0] = '/';
        strncpy(output + 1, raw, output_size - 2);
        output[output_size - 1] = '\0';
    } else {
        strncpy(output, raw, output_size - 1);
        output[output_size - 1] = '\0';
    }
    /* Strip trailing slashes. */
    size_t len = strlen(output);
    while (len > 0 && output[len - 1] == '/') {
        output[--len] = '\0';
    }
    /* Reject if contains "..", "//", or reject characters. */
    if (strstr(output, "..") != NULL || strstr(output, "//") != NULL) {
        output[0] = '\0';
        return 0;
    }
    /* Reject characters: quotes, angle brackets, whitespace, newlines, tabs. */
    for (const char *p = output; *p; p++) {
        if (*p == '"' || *p == '\'' || *p == '<' || *p == '>' ||
            *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            output[0] = '\0';
            return 0;
        }
    }
    /* Reject if too long. */
    if (len > 64) {
        output[0] = '\0';
        return 0;
    }
    return 0;
}

/* PoP: cli_hermes_cli_dashboard_auth_prefix_prefix_from_request @ hermes_cli/dashboard_auth/prefix.py:prefix_from_request */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:prefix_from_request */
/* Convenience wrapper that reads the header off a request and normalises it. */
/* In the Python version this reads request.headers.get("x-forwarded-prefix"). */
/* The C port takes the header value directly as a parameter. */
int cli_hermes_cli_dashboard_auth_prefix_prefix_from_request(
    const char *x_forwarded_prefix_header, char *output, size_t output_size)
{
    /* Handle NULL header — no prefix set. */
    if (!x_forwarded_prefix_header) {
        if (output && output_size > 0) {
            output[0] = '\0';
        }
        return 0;
    }
    /* Delegate to the normalise_prefix function for validation. */
    return cli_hermes_cli_dashboard_auth_prefix_normalise_prefix(
        x_forwarded_prefix_header, output, output_size);
}

/* PoP: cli_hermes_cli_dashboard_auth_prefix__normalise_public_url @ hermes_cli/dashboard_auth/prefix.py:_normalise_public_url */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:_normalise_public_url */
/* Normalises a dashboard.public_url value. */
/* Returns the cleaned URL or "" when malformed. */
int cli_hermes_cli_dashboard_auth_prefix__normalise_public_url(
    const char *raw, char *output, size_t output_size)
{
    if (!raw || !output || output_size == 0) {
        return -1;
    }
    output[0] = '\0';
    /* Skip leading whitespace. */
    while (*raw == ' ' || *raw == '\t') raw++;
    if (!*raw) {
        return 0;
    }
    /* Reject control/quote/whitespace characters. */
    for (const char *p = raw; *p; p++) {
        if (*p == '"' || *p == '\'' || *p == '<' || *p == '>' ||
            *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            return 0;
        }
    }
    /* Check for http:// or https:// scheme. */
    if (strncmp(raw, "http://", 7) != 0 && strncmp(raw, "https://", 8) != 0) {
        return 0;
    }
    /* Find the netloc (after scheme://). */
    const char *after_scheme = strstr(raw, "://");
    if (!after_scheme) {
        return 0;
    }
    after_scheme += 3;  /* skip "://" */
    if (!*after_scheme) {
        return 0;  /* no netloc */
    }
    /* Copy URL, stripping trailing slash. */
    strncpy(output, raw, output_size - 1);
    output[output_size - 1] = '\0';
    size_t len = strlen(output);
    while (len > 0 && output[len - 1] == '/') {
        output[--len] = '\0';
    }
    return 0;
}

/* PoP: cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section @ hermes_cli/dashboard_auth/prefix.py:_load_dashboard_section */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:_load_dashboard_section */
/* Returns the dashboard block from config.yaml if it exists. */
int cli_hermes_cli_dashboard_auth_prefix__load_dashboard_section(
    char *public_url_out, size_t url_size)
{
    if (!public_url_out || url_size == 0) {
        return -1;
    }
    public_url_out[0] = '\0';
    /* CLI port: config loading is handled by the hermes_cli config module. */
    /* Return empty to signal "no config section found". */
    return 0;
}

/* PoP: cli_hermes_cli_dashboard_auth_prefix_resolve_public_url @ hermes_cli/dashboard_auth/prefix.py:resolve_public_url */

/* Port of Python hermes_cli/dashboard_auth/prefix.py:resolve_public_url */
/* Resolves the operator-declared dashboard public URL. */
int cli_hermes_cli_dashboard_auth_prefix_resolve_public_url(
    char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    output[0] = '\0';
    /* 1. Check HERMES_DASHBOARD_PUBLIC_URL env var. */
    const char *env_raw = getenv("HERMES_DASHBOARD_PUBLIC_URL");
    if (env_raw && env_raw[0]) {
        char normalised[512];
        if (cli_hermes_cli_dashboard_auth_prefix__normalise_public_url(
                env_raw, normalised, sizeof(normalised)) == 0 && normalised[0]) {
            strncpy(output, normalised, output_size - 1);
            output[output_size - 1] = '\0';
            return 0;
        }
        cli_hermes_cli_dashboard_auth_prefix__warn_if_malformed(
            "HERMES_DASHBOARD_PUBLIC_URL env var", env_raw);
    }
    /* 2. Check dashboard.public_url in config.yaml (CLI port: not available). */
    /* 3. Return empty string — signals "no override, reconstruct from request". */
    return 0;
}
