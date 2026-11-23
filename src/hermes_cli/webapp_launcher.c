/*
 * webapp_launcher.c — faithful C11 port of upstream Hermes
 * hermes_cli/webapp_launcher.py (wave14 gate w14_webapplaun_8e9a00).
 *
 * PoP: w14_webapplaun_8e9a00_older_gateway @ hermes_cli/webapp_launcher.py:older_gateway
 * PoP: w14_webapplaun_8e9a00_refusal @ hermes_cli/webapp_launcher.py:_refusal
 * PoP: w14_webapplaun_8e9a00_plain_http_note @ hermes_cli/webapp_launcher.py:_plain_http_note
 * PoP: w14_webapplaun_8e9a00_signed_out_summary @ hermes_cli/webapp_launcher.py:_signed_out_summary
 * PoP: w14_webapplaun_8e9a00_restart_applies @ hermes_cli/webapp_launcher.py:_restart_applies
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define RESTART_COMMAND "hermes gateway restart"
#define OLDER_GATEWAY \
    "The running gateway is older than the Webapp and does not know its commands. " \
    "Restart it onto this version of Hermes: " RESTART_COMMAND
#define PLAIN_HTTP_NOTE \
    "This address is plain HTTP, so the browser turns off what needs a secure connection: " \
    "the bot screen, the microphone (dictation, live voice and the remote wake word), " \
    "reading the clipboard (terminal paste, pasting an image) and system notifications. " \
    "Open the Webapp over HTTPS (webapp.public_url) or through an SSH tunnel to " \
    "127.0.0.1 to use them."
#define NOT_SERVING \
    "The gateway is not serving the Webapp, so no browser is connected to it. " \
    "Replaced its stored browser credential: every browser must sign in again once " \
    "it serves the Webapp."
#define OLD_GATEWAY_SIGN \
    "Replaced the Webapp's browser credential. Restart the gateway (" RESTART_COMMAND \
    ") to sign out browsers that signed in with a password or an account too."
#define NOT_BUILT "The Webapp is not built"

static int truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

/* Python str(value) for the scalar values used as error strings. */
static void py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) snprintf(buf, n, "None");
    else if (v->type == JSON_STRING) snprintf(buf, n, "%s", v->str_val ? v->str_val : "");
    else if (v->type == JSON_BOOL) snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else snprintf(buf, n, "%g", v->num_val);
    } else buf[0] = '\0';
}

static void error_str(const struct json_t *response, const char *dflt,
                      char *buf, size_t n) {
    const struct json_t *v = response ? json_obj_get(response, "error") : NULL;
    if (truthy(v)) py_str(v, buf, n);
    else snprintf(buf, n, "%s", dflt);
}

static int is_older(const struct json_t *response) {
    const struct json_t *ok = response ? json_obj_get(response, "ok") : NULL;
    char err[4096];
    error_str(response, "", err, sizeof err);
    return !truthy(ok) && !strncmp(err, "unknown verb:", 13);
}

struct json_t *w14_webapplaun_8e9a00_older_gateway(const struct json_t *in) {
    return json_bool(is_older(json_obj_get(in, "response")) ? 1 : 0);
}

struct json_t *w14_webapplaun_8e9a00_refusal(const struct json_t *in) {
    const struct json_t *response = json_obj_get(in, "response");
    char err[4096], *sep;
    if (response && is_older(response)) return json_string(OLDER_GATEWAY);
    error_str(response, "The gateway refused the request.", err, sizeof err);
    sep = strstr(err, ": ");
    if (sep) {
        size_t klen = (size_t)(sep - err);
        const char *msg = sep + 2;
        if (klen >= 5 && !strncmp(err + klen - 5, "Error", 5) && msg[0])
            return json_string(msg);
    }
    return json_string(err);
}

static int is_loopback_ipv4(const char *h) {
    int parts = 0, val = 0, digits = 0;
    const char *p = h;
    while (1) {
        if (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; digits++; continue; }
        if (*p == '.' && digits) {
            if (val > 255) return 0;
            parts++; val = 0; digits = 0; p++; continue;
        }
        break;
    }
    if (!digits || val > 255 || parts != 3 || *p) return 0;
    return strncmp(h, "127.", 4) == 0;
}

/* urlsplit(url); if scheme != http -> None; localhost / *.localhost / loopback
 * IPs are secure contexts; everything else is PLAIN_HTTP_NOTE. */
struct json_t *w14_webapplaun_8e9a00_plain_http_note(const struct json_t *in) {
    const char *url = json_get_str(in, "url", "");
    const char *sep = strstr(url, "://");
    const char *p, *hend;
    char host[1024];
    size_t n, i;

    if (!sep || strncasecmp(url, "http://", 7)) return json_null();
    p = sep + 3;
    /* strip userinfo */
    {
        const char *at = strchr(p, '@');
        const char *slash = strchr(p, '/');
        if (at && (!slash || at < slash)) p = at + 1;
    }
    if (*p == '[') {
        const char *close = strchr(p, ']');
        if (!close) return json_string(PLAIN_HTTP_NOTE);
        p++;
        hend = close;
    } else {
        hend = p;
        while (*hend && *hend != ':' && *hend != '/') hend++;
    }
    n = (size_t)(hend - p);
    if (n >= sizeof host) n = sizeof host - 1;
    for (i = 0; i < n; i++) host[i] = (char)tolower((unsigned char)p[i]);
    host[n] = '\0';
    while (n > 0 && host[n - 1] == '.') host[--n] = '\0';

    if (!strcmp(host, "localhost") ||
        (n > 10 && !strcmp(host + n - 10, ".localhost"))) return json_null();
    if (is_loopback_ipv4(host) || !strcmp(host, "::1") ||
        !strcmp(host, "0:0:0:0:0:0:0:1")) return json_null();
    return json_string(PLAIN_HTTP_NOTE);
}

struct json_t *w14_webapplaun_8e9a00_signed_out_summary(const struct json_t *in) {
    const struct json_t *result = json_obj_get(in, "result");
    const struct json_t *serving, *closed, *gate;
    char connections[128], buf[4096];
    long long c;

    if (!truthy(result ? json_obj_get(result, "serving") : NULL) && result &&
        json_obj_get(result, "serving"))
        return json_string(NOT_SERVING);
    closed = result ? json_obj_get(result, "connections_closed") : NULL;
    if (!closed) return json_string(OLD_GATEWAY_SIGN);

    c = (closed->type == JSON_NUMBER) ? (long long)closed->num_val : 0;
    snprintf(connections, sizeof connections, "closed %lld browser connection%s",
             c, c == 1 ? "" : "s");
    gate = result ? json_obj_get(result, "sign_in_gate") : NULL;
    if (truthy(gate)) {
        snprintf(buf, sizeof buf,
                 "Signed out every browser: %s. Every browser login behind sign-in has "
                 "ended, and the account tokens it held no longer work here: browsers "
                 "must sign in again. Native apps signed in to this gateway with their "
                 "own sign-in stay signed in.", connections);
        return json_string(buf);
    }
    snprintf(buf, sizeof buf,
             "Signed out of every Webapp tab: %s. Run hermes webapp to open it again.",
             connections);
    return json_string(buf);
}

struct json_t *w14_webapplaun_8e9a00_restart_applies(const struct json_t *in) {
    char reason[4096];
    py_str(json_obj_get(in, "reason"), reason, sizeof reason);
    return json_bool(strncmp(reason, NOT_BUILT, strlen(NOT_BUILT)) ? 1 : 0);
}
