/*
 * port_auth_error_copy.c — Port of Python: hermes_cli/auth_error_copy.py
 *
 * One table of (predicate, lead sentence) classifies a failure into a sentence
 * a first-time user can act on; the raw exception is demoted to a Details:
 * line. Python inspects the exception object; the C entry points take an
 * afc_error_t carrying the same facts (class name, message, oauth code, exit
 * code).
 */

#include "port_auth_error_copy.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── tiny growing string ─────────────────────────────────────────── */

typedef struct { char *p; size_t len, cap; } sbuf;

static void sb_init(sbuf *b)
{
    b->cap = 128;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_addn(sbuf *b, const char *s, size_t n)
{
    if (!b->p || !s) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap * 2;
        while (cap < b->len + n + 1) cap *= 2;
        char *tmp = (char *)realloc(b->p, cap);
        if (!tmp) return;
        b->p = tmp;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_add(sbuf *b, const char *s)
{
    if (s) sb_addn(b, s, strlen(s));
}

static void sb_addf(sbuf *b, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) sb_addn(b, tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
}

/* Python str.strip() */
static char *str_strip_dup(const char *s)
{
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python str.splitlines() (the ASCII split points; unicode separators are not
 * produced by these code paths). Returns a NULL-terminated array of malloc'd
 * lines and sets *count. */
static char **split_lines(const char *s, int *count)
{
    int cap = 8, n = 0;
    char **out = (char **)calloc((size_t)cap, sizeof(char *));
    if (!out) { *count = 0; return NULL; }
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        size_t len = (size_t)(p - start);
        if (n + 2 > cap) {
            cap *= 2;
            char **tmp = (char **)realloc(out, (size_t)cap * sizeof(char *));
            if (!tmp) break;
            out = tmp;
        }
        char *line = (char *)malloc(len + 1);
        if (!line) break;
        memcpy(line, start, len);
        line[len] = '\0';
        out[n++] = line;
        if (*p == '\r') { p++; if (*p == '\n') p++; }
        else if (*p == '\n') p++;
    }
    out[n] = NULL;
    *count = n;
    return out;
}

static void free_lines(char **lines, int count)
{
    if (!lines) return;
    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

static int name_in(const char *name, const char *const *set, size_t n)
{
    if (!name) return 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(name, set[i]) == 0) return 1;
    return 0;
}

/* Python: _NETWORK_ERROR_TYPES (httpx class names). */
static const char *const NETWORK_TYPES[] = {
    "ConnectError", "ConnectTimeout", "ReadTimeout", "PoolTimeout",
    "WriteTimeout", "TimeoutException", "RemoteProtocolError", "ReadError",
    "ProxyError", "UnsupportedProtocol", "NetworkError",
};

/* Python: isinstance(exc, (ConnectionError, TimeoutError)) — the stdlib base
 * plus its concrete subclasses, which isinstance() covers. */
static const char *const STDLIB_NETWORK_TYPES[] = {
    "ConnectionError", "ConnectionRefusedError", "ConnectionResetError",
    "ConnectionAbortedError", "BrokenPipeError", "TimeoutError",
};

/* PoP: afc_is_network_error @ hermes_cli/auth_error_copy.py:is_network_error */
int afc_is_network_error(const afc_error_t *err)
{
    if (!err) return 0;
    if (err->kind == AFC_SIGNIN_COPY || err->kind == AFC_DEVICE_EXPIRED)
        return 0;
    if (name_in(err->type_name, NETWORK_TYPES,
                sizeof(NETWORK_TYPES) / sizeof(NETWORK_TYPES[0])))
        return 1;
    return name_in(err->type_name, STDLIB_NETWORK_TYPES,
                   sizeof(STDLIB_NETWORK_TYPES) / sizeof(STDLIB_NETWORK_TYPES[0]));
}

/* PoP: afc_is_device_code_expired @ hermes_cli/auth_error_copy.py:is_device_code_expired */
int afc_is_device_code_expired(const afc_error_t *err)
{
    if (!err) return 0;
    if (err->exc_code &&
        (strcmp(err->exc_code, "device_code_timeout") == 0 ||
         strcmp(err->exc_code, "timeout") == 0))
        return 1;
    return err->oauth_code && strcmp(err->oauth_code, "expired_token") == 0;
}

/* PoP: afc_is_cancelled @ hermes_cli/auth_error_copy.py:is_cancelled */
int afc_is_cancelled(const afc_error_t *err)
{
    if (!err) return 0;
    if (err->kind == AFC_CANCELLED) return 1;   /* KeyboardInterrupt / EOFError */
    if (err->type_name && strcmp(err->type_name, "SystemExit") == 0)
        return err->exit_code == -1 || err->exit_code == 0 ||
               err->exit_code == 130;
    return 0;
}

/* PoP: afc_details_line @ hermes_cli/auth_error_copy.py:_details_line */
char *afc_details_line(const afc_error_t *err)
{
    if (!err) return NULL;
    /* str(SystemExit) is "" for None and the decimal code otherwise — not the
     * class name. Everything else uses its own message. */
    char codebuf[24];
    const char *raw_message = err->message;
    if (err->type_name && strcmp(err->type_name, "SystemExit") == 0) {
        if (err->exit_code == -1) {
            raw_message = "";
        } else {
            snprintf(codebuf, sizeof(codebuf), "%d", err->exit_code);
            raw_message = codebuf;
        }
    }
    char *text = str_strip_dup(raw_message ? raw_message : "");
    if (!text) return NULL;
    const char *shown = (*text) ? text : (err->type_name ? err->type_name : "");
    size_t cap = strlen(shown) + 16;
    char *out = (char *)malloc(cap);
    if (out) snprintf(out, cap, "  Details: %s", shown);
    free(text);
    return out;
}

/* Python: DEVICE_FLOW_ERROR_COPY[code].format(retry=retry_command) */
static const char *device_flow_copy(const char *code)
{
    static const struct { const char *code, *copy; } table[] = {
        { "expired_token",
          "The sign-in code expired before it was approved in the browser. "
          "Run `%s` to get a new code." },
        { "access_denied",
          "Sign-in was declined in the browser. Run `%s` to try again, or "
          "`hermes model` to pick a different provider." },
        { "invalid_grant",
          "The sign-in code was not accepted by the server. Run `%s` to get "
          "a new code." },
        { "invalid_client",
          "The server did not recognize this copy of Hermes. Run "
          "`hermes update`, then `%s` again." },
    };
    if (!code) return NULL;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(code, table[i].code) == 0) return table[i].copy;
    return NULL;
}

/* PoP: afc_device_flow_error @ hermes_cli/auth_error_copy.py:device_flow_error */
char *afc_device_flow_error(const char *code, const char *description,
                            const char *retry_command)
{
    const char *retry = (retry_command && *retry_command) ? retry_command
                                                          : "hermes portal";
    const char *desc = (description && *description) ? description
                                                     : "the server rejected the request";
    sbuf lead;
    sb_init(&lead);
    if (!lead.p) return NULL;

    const char *tmpl = device_flow_copy(code);
    if (tmpl) {
        sb_addf(&lead, tmpl, retry);
    } else {
        sb_addf(&lead, "Sign-in did not complete: %s. Run `%s` to try again.",
                desc, retry);
    }

    sbuf details;
    sb_init(&details);
    if (!details.p) { free(lead.p); return NULL; }
    if (code && *code) sb_addf(&details, "%s: %s", code, description ? description : "");
    else if (description) sb_add(&details, description);

    sbuf out;
    sb_init(&out);
    if (!out.p) { free(lead.p); free(details.p); return NULL; }
    sb_add(&out, lead.p);
    if (details.len) {
        sb_add(&out, "\n  Details: ");
        sb_add(&out, details.p);
    }
    free(lead.p);
    free(details.p);
    return out.p;
}

static void arr_push(json_t *arr, const char *line)
{
    if (arr && line) json_append(arr, json_string(line));
}

/* PoP: afc_sign_in_failure_lines @ hermes_cli/auth_error_copy.py:sign_in_failure_lines */
json_t *afc_sign_in_failure_lines(const afc_error_t *err,
                                  const char *service_host,
                                  const char *retry_command)
{
    if (!err) return NULL;
    const char *host = (service_host && *service_host) ? service_host
                                                        : "portal.nousresearch.com";
    const char *retry = (retry_command && *retry_command) ? retry_command
                                                          : "hermes portal";
    json_t *arr = json_array();
    if (!arr) return NULL;

    if (err->kind == AFC_SIGNIN_COPY) {
        int n = 0;
        char **lines = split_lines(err->message, &n);
        for (int i = 0; i < n; i++) arr_push(arr, lines[i]);
        free_lines(lines, n);
        return arr;
    }

    int cancelled = afc_is_cancelled(err);
    int network = afc_is_network_error(err);
    sbuf line;
    sb_init(&line);
    if (!line.p) { json_free(arr); return NULL; }

    if (cancelled)
        sb_addf(&line, "Sign-in was cancelled. Run `%s` when you want to try again.", retry);
    else if (network)
        sb_addf(&line,
                "Could not sign in: Hermes could not reach %s. Check your "
                "internet connection or proxy, then run `%s` again.", host, retry);
    else
        sb_addf(&line,
                "Could not sign in. Run `%s` to try again, or `hermes model` "
                "to pick a different provider.", retry);
    arr_push(arr, line.p);
    free(line.p);

    if (!cancelled) {
        char *details = afc_details_line(err);
        arr_push(arr, details);
        free(details);
    }
    return arr;
}

/* PoP: afc_provider_setup_failure_lines @ hermes_cli/auth_error_copy.py:provider_setup_failure_lines */
json_t *afc_provider_setup_failure_lines(const afc_error_t *err,
                                         const char *retry_command)
{
    if (!err) return NULL;
    const char *retry = (retry_command && *retry_command) ? retry_command
                                                          : "hermes model";
    json_t *arr = json_array();
    if (!arr) return NULL;

    sbuf nothing;
    sb_init(&nothing);
    if (!nothing.p) { json_free(arr); return NULL; }
    sb_addf(&nothing,
            "Your provider settings were not changed. Continue the wizard now "
            "and run `%s` afterwards to try again.", retry);

    if (err->kind == AFC_SIGNIN_COPY) {
        int n = 0;
        char **lines = split_lines(err->message, &n);
        sbuf first;
        sb_init(&first);
        if (!first.p) { free_lines(lines, n); free(nothing.p); json_free(arr); return NULL; }
        sb_add(&first, "Could not finish connecting a provider: ");
        if (n > 0 && lines[0][0]) {
            char c = (char)tolower((unsigned char)lines[0][0]);
            sb_addn(&first, &c, 1);
            sb_add(&first, lines[0] + 1);
        }
        arr_push(arr, first.p);
        arr_push(arr, nothing.p);
        for (int i = 1; i < n; i++) arr_push(arr, lines[i]);
        free(first.p);
        free_lines(lines, n);
        free(nothing.p);
        return arr;
    }

    const char *reason;
    if (afc_is_cancelled(err))
        reason = "sign-in was cancelled";
    else if (afc_is_network_error(err))
        reason = "no internet connection, or the provider could not be reached";
    else
        reason = "something went wrong while talking to the provider";

    sbuf head;
    sb_init(&head);
    if (!head.p) { free(nothing.p); json_free(arr); return NULL; }
    sb_addf(&head, "Could not finish connecting a provider (%s).", reason);
    arr_push(arr, head.p);
    arr_push(arr, nothing.p);
    free(head.p);
    free(nothing.p);

    if (!afc_is_cancelled(err)) {
        char *details = afc_details_line(err);
        arr_push(arr, details);
        free(details);
    }
    return arr;
}

/* PoP: afc_unknown_provider_hint @ hermes_cli/auth_error_copy.py:unknown_provider_hint */
const char *afc_unknown_provider_hint(const char *provider_name)
{
    if (!provider_name) return NULL;
    char *name = str_strip_dup(provider_name);
    if (!name) return NULL;
    for (char *p = name; *p; p++) *p = (char)tolower((unsigned char)*p);
    int removed = strcmp(name, "opencode-free") == 0 ||
                  strcmp(name, "free") == 0 ||
                  strcmp(name, "opencode_free") == 0;
    free(name);
    if (!removed) return NULL;
    return "OpenCode discontinued anonymous free-tier access outside its own "
           "client (relay 403s FreeTierError), so the keyless 'opencode-free' "
           "provider was removed. Switch to 'opencode-zen' (pay-as-you-go, "
           "OPENCODE_ZEN_API_KEY) or 'opencode-go' ($10/mo subscription, "
           "OPENCODE_GO_API_KEY) via 'hermes model'.";
}
