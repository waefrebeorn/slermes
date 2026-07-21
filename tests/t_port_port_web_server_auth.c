/*
 * t_port_port_web_server_auth.c — oracle harness for the web server auth
 * helpers ported in src/cli/port_web_server_auth.c.
 *
 * Fixture line grammar (one op per line, '|' field separator; leading '#'
 * or blank line ignored):
 *   should_require_auth <host> <allow_public>
 *   is_accepted_host <host_header> <bound_host>
 *   has_valid_session_token <raw_headers> <expected_token>
 */
#include "hermes_web_dashboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* emit a JSON boolean: true/false (raw, no quotes) */
static void emit_json_bool(bool v) {
    fputs(v ? "true" : "false", stdout);
}

/* emit a JSON string value with proper escaping */
static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s ? s : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c < 0x20) fprintf(stdout, "\\u%04x", c);
        else putchar((int)c);
    }
    putchar('"');
}

/* emit a per-case JSON envelope line, matching the Python oracle's format
 * exactly so run_oracle.sh can diff the two outputs case-by-case. */
static void emit_case(const char *op, const char *a0, const char *a1,
                      bool result) {
    fputs("{\"op\":\"", stdout);
    fputs(op, stdout);
    fputs("\",\"args\":[", stdout);
    emit_json_string(a0);
    putchar(',');
    emit_json_string(a1);
    fputs("],\"result\":", stdout);
    emit_json_bool(result);
    fputs("}\n", stdout);
}

int main(int argc, char **argv) {
    /* Set a known session token so has_valid_session_token can validate. */
    strncpy(g_session_token, "test-token-123", sizeof(g_session_token) - 1);

    FILE *f = stdin;
    if (argc >= 2) {
        f = fopen(argv[1], "rb");
        if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    }

    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        /* skip comments/blank */
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
        if (!*p || *p == '#') continue;

        /* strip trailing newline */
        size_t len = strlen(p);
        while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r')) p[--len] = '\0';

        /* parse op and rest */
        /* parse op and rest. The fixture uses '|' as the per-line field
         * separator, so the first token (the op) is everything up to the
         * first '|'. Strtok with delimiter "|" naturally yields that. */
        char *op = strtok(p, "|");
        if (!op) continue;
        char *rest = strtok(NULL, "");
        if (!rest) rest = "";

        /* strip leading whitespace from op (some fixture dialects put a
         * ' ' after the separator) */
        while (*op == ' ' || *op == '\t') op++;

        if (strcmp(op, "should_require_auth") == 0) {
            char *host = strtok(rest, "|");
            char *allow_public_str = strtok(NULL, "|");
            if (!host) continue;
            /* NONE is the per-line sentinel for an empty string. The two
             * ops that take string-typed args (host / host_header) honour
             * this; the boolean allows-public column has no sentinel. */
            if (strcmp(host, "NONE") == 0) host = "";
            bool allow_public = (allow_public_str && strcmp(allow_public_str, "true") == 0);
            bool result = ws_should_require_auth(host, allow_public);
            emit_case(op, host, allow_public_str ? allow_public_str : "false", result);
        }
        else if (strcmp(op, "is_accepted_host") == 0) {
            char *host_header = strtok(rest, "|");
            char *bound_host = strtok(NULL, "|");
            if (!host_header || !bound_host) continue;
            if (strcmp(host_header, "NONE") == 0) host_header = "";
            bool result = ws_is_accepted_host(host_header, bound_host);
            emit_case(op, host_header, bound_host, result);
        }
        else if (strcmp(op, "has_valid_session_token") == 0) {
            char *raw = strtok(rest, "|");
            if (!raw) continue;
            /* Strip surrounding double quotes if present. */
            char *headers = raw;
            size_t hlen = strlen(headers);
            if (hlen >= 2 && headers[0] == '"' && headers[hlen-1] == '"') {
                headers[hlen-1] = '\0';
                headers++;
            }
            /* Unescape literal backslash-r and backslash-n sequences so the
             * fixture can carry CRLF line endings inside a single-line token
             * header block. Also honour \\ and \". */
            char *h = headers, *out = headers;
            while (*h) {
                if (h[0] == '\\' && h[1] == 'r') { *out++ = '\r'; h += 2; }
                else if (h[0] == '\\' && h[1] == 'n') { *out++ = '\n'; h += 2; }
                else if (h[0] == '\\' && h[1] == '\\') { *out++ = '\\'; h += 2; }
                else if (h[0] == '\\' && h[1] == '"') { *out++ = '"'; h += 2; }
                else *out++ = *h++;
            }
            *out = '\0';
            bool result = ws_has_valid_session_token(headers);
            emit_case(op, headers, "", result);
        }
    }
    if (f != stdin) fclose(f);
    /* Force the stdio buffers out — we exit via `return 0` and the runtime
     * may otherwise flush lazily for non-tty stdout (a pipe). */
    fflush(stdout);
    return 0;
}