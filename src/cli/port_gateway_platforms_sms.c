/*
 * port_gateway_platforms_sms.c — C port of gateway/platforms/sms.py
 *
 * SMS (Twilio) platform adapter.
 * Connects to Twilio REST API for outbound SMS and webhook server for inbound.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "libbase64/base64.h"
#include "libcrypto/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define TWILIO_API_BASE "https://api.twilio.com/2010-04-01/Accounts"
#define MAX_SMS_LENGTH 1600

/* PoP: cli_gateway_platforms_sms_check_sms_requirements @ gateway/platforms/sms.py:check_sms_requirements */

/* Port of Python gateway/platforms/sms.py:check_sms_requirements */
/* Check if SMS adapter dependencies are available. */
int cli_gateway_platforms_sms_check_sms_requirements(void)
{
    /* Check for required env vars */
    const char *account_sid = getenv("TWILIO_ACCOUNT_SID");
    const char *auth_token = getenv("TWILIO_AUTH_TOKEN");

    if (!account_sid || !*account_sid) {
        hermes_log(LOG_DEBUG, "sms", "TWILIO_ACCOUNT_SID not set");
        return 0;
    }
    if (!auth_token || !*auth_token) {
        hermes_log(LOG_DEBUG, "sms", "TWILIO_AUTH_TOKEN not set");
        return 0;
    }

    hermes_log(LOG_DEBUG, "sms", "SMS requirements met (SID=%.*s...)",
               account_sid ? 8 : 0, account_sid ? account_sid : "");
    return 1;
}

/* PoP: cli_gateway_platforms_sms__basic_auth_header @ gateway/platforms/sms.py:_basic_auth_header */

/* Port of Python gateway/platforms/sms.py:_basic_auth_header */
/* Build HTTP Basic auth header value for Twilio. */
int cli_gateway_platforms_sms__basic_auth_header(
    const char *account_sid, const char *auth_token,
    char *header_out, size_t header_size)
{
    if (!account_sid || !auth_token || !header_out || header_size == 0) return -1;

    /* Build base64-encoded credentials: base64("sid:token") */
    char creds[512];
    snprintf(creds, sizeof(creds), "%s:%s", account_sid, auth_token);

    char *b64 = base64_encode((const unsigned char *)creds, strlen(creds));
    if (!b64) return -1;
    snprintf(header_out, header_size, "Basic %s", b64);
    free(b64);
    hermes_log(LOG_DEBUG, "sms", "Built auth header for SID=%.*s",
               account_sid ? 8 : 0, account_sid ? account_sid : "");
    return 0;
}

/* PoP: cli_gateway_platforms_sms__validate_twilio_signature @ gateway/platforms/sms.py:_validate_twilio_signature */

/* Port of Python gateway/platforms/sms.py:_validate_twilio_signature */
/* Validate X-Twilio-Signature header (HMAC-SHA1, base64). */
int cli_gateway_platforms_sms__validate_twilio_signature(
    const char *url, const char **param_keys, const char **param_values,
    int param_count, const char *signature,
    const char *auth_token)
{
    if (!url || !signature || !auth_token) return 0;

    /* Try primary URL */
    int result = cli_gateway_platforms_sms__check_signature(
        url, param_keys, param_values, param_count, signature, auth_token);
    if (result) return 1;

    /* Try port variant URL */
    char variant_url[2048];
    /* Build variant: toggle default port */
    if (strstr(url, "https://") == url) {
        /* For https, try without :443 or with :443 */
        char *port_pos = strstr(url + 8, ":443");
        if (port_pos) {
            /* Has explicit :443 — try without */
            size_t prefix = (size_t)(port_pos - url);
            snprintf(variant_url, sizeof(variant_url), "%.*s%s", (int)prefix, url, port_pos + 4);
        } else {
            /* No port — try adding :443 */
            char *path_pos = strchr(url + 8, '/');
            if (path_pos) {
                size_t prefix = (size_t)(path_pos - url);
                snprintf(variant_url, sizeof(variant_url), "%.*s:443%s",
                         (int)prefix, url, path_pos);
            } else {
                snprintf(variant_url, sizeof(variant_url), "%s:443", url);
            }
        }
    } else if (strstr(url, "http://") == url) {
        char *port_pos = strstr(url + 7, ":80");
        if (port_pos) {
            size_t prefix = (size_t)(port_pos - url);
            snprintf(variant_url, sizeof(variant_url), "%.*s%s", (int)prefix, url, port_pos + 3);
        } else {
            char *path_pos = strchr(url + 7, '/');
            if (path_pos) {
                size_t prefix = (size_t)(path_pos - url);
                snprintf(variant_url, sizeof(variant_url), "%.*s:80%s",
                         (int)prefix, url, path_pos);
            } else {
                snprintf(variant_url, sizeof(variant_url), "%s:80", url);
            }
        }
    } else {
        variant_url[0] = '\0';
    }

    if (variant_url[0]) {
        result = cli_gateway_platforms_sms__check_signature(
            variant_url, param_keys, param_values, param_count, signature, auth_token);
        if (result) return 1;
    }

    hermes_log(LOG_WARNING, "sms", "Twilio signature validation failed");
    return 0;
}

/* PoP: cli_gateway_platforms_sms__check_signature @ gateway/platforms/sms.py:_check_signature */

/* Port of Python gateway/platforms/sms.py:_check_signature */
/* Compute and compare a single Twilio signature. */
int cli_gateway_platforms_sms__check_signature(
    const char *url, const char **param_keys, const char **param_values,
    int param_count, const char *signature, const char *auth_token)
{
    if (!url || !signature || !auth_token) return 0;

    /* Build data_to_sign = url + sorted(key + value).
     * Twilio sorts the concatenation of each "keyvalue" pair. */
    char data[8192];
    snprintf(data, sizeof(data), "%s", url);

    /* Collect "keyvalue" pairs, sort them, append. */
    char pairs[256][512];
    int np = 0;
    for (int i = 0; i < param_count && np < 256; i++) {
        if (param_keys[i] && param_values[i]) {
            snprintf(pairs[np], sizeof(pairs[np]), "%s%s", param_keys[i], param_values[i]);
            np++;
        }
    }
    /* Simple insertion sort (ascending, byte-wise). */
    for (int i = 1; i < np; i++) {
        char key[512];
        snprintf(key, sizeof(key), "%s", pairs[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(pairs[j], key) > 0) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s", pairs[j]);
            snprintf(pairs[j + 1], sizeof(pairs[j + 1]), "%s", tmp);
            j--;
        }
        snprintf(pairs[j + 1], sizeof(pairs[j + 1]), "%s", key);
    }
    for (int i = 0; i < np; i++) {
        size_t pos = strlen(data);
        if (pos < sizeof(data) - 512)
            snprintf(data + pos, sizeof(data) - pos, "%s", pairs[i]);
    }

    /* Compute HMAC-SHA1(auth_token, data) and base64-encode it. */
    unsigned char mac[20];
    crypto_hmac_sha1((const unsigned char *)auth_token, strlen(auth_token),
                     (const unsigned char *)data, strlen(data), mac);
    char *computed = base64_encode(mac, sizeof(mac));
    if (!computed) return 0;

    int match = (strcmp(computed, signature) == 0);
    free(computed);

    hermes_log(LOG_DEBUG, "sms", "Checking signature for URL (len=%zu) match=%d",
               strlen(data), match);
    return match;
}

/* PoP: cli_gateway_platforms_sms__port_variant_url @ gateway/platforms/sms.py:_port_variant_url */

/* Port of Python gateway/platforms/sms.py:_port_variant_url */
/* Return the URL with the default port toggled, or empty string. */
int cli_gateway_platforms_sms__port_variant_url(
    const char *url, char *variant_out, size_t variant_size)
{
    if (!url || !variant_out || variant_size == 0) return -1;

    variant_out[0] = '\0';

    /* Parse scheme */
    int default_port = 0;
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return -1;

    size_t scheme_len = (size_t)(scheme_end - url);
    if (scheme_len == 5 && strncmp(url, "https", 5) == 0) {
        default_port = 443;
    } else if (scheme_len == 4 && strncmp(url, "http", 4) == 0) {
        default_port = 80;
    } else {
        return -1; /* Unknown scheme */
    }

    /* Find hostname and port */
    const char *host_start = scheme_end + 3;
    const char *path_start = strchr(host_start, '/');
    const char *port_start = strchr(host_start, ':');

    if (port_start && (!path_start || port_start < path_start)) {
        /* Has explicit port */
        int port = atoi(port_start + 1);
        if (port == default_port) {
            /* Strip default port */
            size_t host_len = (size_t)(port_start - host_start);
            size_t path_len = path_start ? strlen(path_start) : 0;
            if (host_len + path_len + 1 > variant_size) return -1;
            snprintf(variant_out, variant_size, "%.*s%s",
                     (int)host_len, host_start, path_start ? path_start : "");
            return 0;
        }
    } else if (!port_start || (path_start && port_start > path_start)) {
        /* No port — add default */
        size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
        size_t remaining = variant_size;
        int written = snprintf(variant_out, remaining, "%.*s:%d",
                               (int)host_len, host_start, default_port);
        if (written > 0 && (size_t)written < remaining && path_start) {
            strncat(variant_out, path_start, remaining - written - 1);
        }
        return 0;
    }

    /* Non-standard port — no variant */
    return -1;
}
