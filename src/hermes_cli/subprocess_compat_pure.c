#define _POSIX_C_SOURCE 200809L
/*
 * subprocess_compat_pure.c - Faithful C11 port of the POSIX path of
 * hermes_cli/_subprocess_compat.py:split_command_line.
 *
 * On POSIX the Python original is exactly `shlex.split(line)` (posix=True).
 * shlex.split tokenizes a command line the way a POSIX shell does:
 *   - whitespace separates tokens
 *   - '...' single-quotes: everything literal until the closing quote
 *   - "..." double-quotes: literal except \, $, ` which may be escaped;
 *       a backslash before one of those (or a newline) yields that char
 *   - \ outside quotes escapes the next char (incl. line continuation)
 *   - # starts a comment when it is the first char of a token (posix)
 *   - unbalanced quotes raise an error (we return NULL + set *err)
 *
 * Returns a malloc'd JSON array of token strings (caller frees). On
 * unbalanced quotes returns NULL and sets *err_nonzero.
 *
 * Port (PoP): subprocess_compat_split_command_line @
 *   hermes_cli/_subprocess_compat.py:split_command_line
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: subprocess_compat_split_command_line @ hermes_cli/_subprocess_compat.py:split_command_line */
char *subprocess_compat_split_command_line(const char *line, int *err)
{
    if (err) *err = 0;
    json_t *arr = json_new_array();

    if (!line) { char *r = json_dumps(arr, 0); json_free(arr); return r; }

    size_t n = strlen(line);
    size_t i = 0;
    /* token state */
    char *tok = NULL; size_t tokcap = 0, toklen = 0;
    int in_token = 0;
    int in_single = 0, in_double = 0;

    /* ensure we have a token buffer (allocate once) */
    #define TOK_INIT() do { if (!tok) { tokcap = 64; tok = (char *)malloc(tokcap); } } while (0)
    #define TOK_BEGIN() do { TOK_INIT(); toklen = 0; } while (0)
    #define TOK_PUSH(c) do { \
        if (toklen + 1 >= tokcap) { tokcap = tokcap ? tokcap * 2 : 64; tok = (char *)realloc(tok, tokcap); } \
        tok[toklen++] = (c); \
    } while (0)
    #define TOK_EMIT() do { \
        TOK_PUSH('\0'); \
        json_array_append(arr, json_string(tok)); \
        in_token = 0; toklen = 0; \
    } while (0)

    while (i < n) {
        char c = line[i];

        if (in_single) {
            if (c == '\'') { in_single = 0; i++; }
            else { TOK_INIT(); TOK_PUSH(c); i++; }
            continue;
        }
        if (in_double) {
            if (c == '"') { in_double = 0; i++; }
            else if (c == '\\') {
                /* inside double quotes, \ only special before $ " ` \ newline */
                if (i + 1 < n) {
                    char nx = line[i+1];
                    if (nx == '$' || nx == '"' || nx == '`' || nx == '\\' || nx == '\n') {
                        TOK_INIT(); TOK_PUSH(nx); i += 2; continue;
                    }
                    /* otherwise literal backslash; fall through to push it */
                    TOK_INIT(); TOK_PUSH('\\'); i++; continue;
                }
                TOK_INIT(); TOK_PUSH('\\'); i++;
            }
            else { TOK_INIT(); TOK_PUSH(c); i++; }
            continue;
        }

        /* outside quotes */
        if (c == '\'' ) { in_single = 1; in_token = 1; i++; continue; }
        if (c == '"' ) { in_double = 1; in_token = 1; i++; continue; }
        if (c == '\\') {
            /* escape next char; if at end, just ignore */
            if (i + 1 < n) {
                if (!in_token) TOK_BEGIN(); else TOK_INIT(); in_token = 1; TOK_PUSH(line[i+1]); i += 2;
            } else { i++; }
            continue;
        }
        if (isspace((unsigned char)c)) {
            if (in_token) TOK_EMIT();
            i++; continue;
        }
        /* regular char */
        if (!in_token) TOK_BEGIN(); else TOK_INIT(); in_token = 1; TOK_PUSH(c); i++;
    }

    /* EOF: if still in a quote, unbalanced */
    if (in_single || in_double) {
        if (err) *err = 1;
        free(tok);
        json_free(arr);
        return NULL;
    }
    if (in_token) TOK_EMIT();
    free(tok);
    char *r = json_dumps(arr, 0);
    json_free(arr);
    return r;
}
