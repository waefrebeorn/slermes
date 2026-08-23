/* gate: shell_heredoc_scan_heredoc_command_unit — traced copy of
 * src/tools/shell_heredoc.c::scan_heredoc_command_unit (start fixed at 0,
 * the common call shape) against LIVE tools/shell_heredoc.py::
 * _scan_heredoc_command_unit.
 *
 * Branch ladder per loop iteration (twin mirrors EXACTLY):
 *   B1: comment mode
 *     B2: newline ends unit
 *   B3: inside quote
 *   B4: backslash escape
 *   B5: quote opens
 *   B6: '#' starts comment
 *   B7: newline ends unit
 *   B8: '<<<' here-string skipped
 *   B9: '<<' opener present
 *     B10: opener unparseable (unknown operator)
 *   V1: canonical "end|specs|unknown|has_list" result string
 * plus one L tick per loop iteration.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* faithful copies of shell_heredoc.c statics */
static bool wc_parse_heredoc_operator(const char *command, size_t index,
                                      size_t *out_end, char **out_delimiter,
                                      bool *out_strip_tabs, bool *out_quoted) {
    size_t n = strlen(command);
    if (index + 2 > n) return false;
    if (strncmp(command + index, "<<", 2) != 0) return false;
    if (index + 3 <= n && strncmp(command + index, "<<<", 3) == 0) return false;

    size_t cursor = index + 2;
    bool strip_tabs = false;
    if (cursor < n && command[cursor] == '-') { strip_tabs = true; cursor++; }
    while (cursor < n && (command[cursor] == ' ' || command[cursor] == '\t')) cursor++;
    if (cursor >= n || command[cursor] == '\r' || command[cursor] == '\n') return false;

    char *delimiter = malloc(n + 1);
    if (!delimiter) return false;
    size_t dlen = 0;
    bool quoted = false;

    while (cursor < n) {
        char ch = command[cursor];
        if (isspace((unsigned char)ch) || ch == ';' || ch == '|' ||
            ch == '<' || ch == '>' || ch == '(' || ch == ')') {
            break;
        }
        if (ch == '\\') {
            if (cursor + 1 >= n || command[cursor + 1] == '\r' || command[cursor + 1] == '\n') {
                free(delimiter); return false;
            }
            quoted = true;
            delimiter[dlen++] = command[cursor + 1];
            cursor += 2;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            quoted = true;
            char quote = ch;
            cursor++;
            while (cursor < n && command[cursor] != quote) {
                if (quote == '"' && command[cursor] == '\\') {
                    if (cursor + 1 >= n) { free(delimiter); return false; }
                    char following = command[cursor + 1];
                    if (following == '$' || following == '`' || following == '"' ||
                        following == '\\' || following == '\n') {
                        delimiter[dlen++] = following;
                        cursor += 2;
                        continue;
                    }
                    delimiter[dlen++] = '\\';
                    cursor++;
                    continue;
                }
                if (command[cursor] == '\r' || command[cursor] == '\n') {
                    free(delimiter); return false;
                }
                delimiter[dlen++] = command[cursor];
                cursor++;
            }
            if (cursor >= n) { free(delimiter); return false; } /* unterminated quote */
            cursor++; /* closing quote */
            continue;
        }
        delimiter[dlen++] = ch;
        cursor++;
    }
    delimiter[dlen] = '\0';
    if (dlen == 0 && !quoted) { free(delimiter); return false; }
    *out_end = cursor;
    *out_delimiter = delimiter;
    *out_strip_tabs = strip_tabs;
    *out_quoted = quoted;
    return true;
}

static void wc_scan(const char *command) {
    WCUP_BEGIN("_scan_heredoc_command_unit");
    size_t n = strlen(command);
    size_t cursor = 0;
    int quote = 0;
    bool comment = false;
    char specs[2048] = {0}; size_t so = 0;
    bool unknown = false, has_list = false;

    while (cursor < n) {
        WCUP_LOOP_TICK();
        char ch = command[cursor];
        if (comment) {
            WCUP_BRANCH(1);                                    /* B1 */
            if (ch == '\n') { WCUP_BRANCH(1); goto done; }     /* B2 */
            WCUP_BRANCH(0);
            cursor++; continue;
        }
        WCUP_BRANCH(0);
        if (quote != 0) {
            WCUP_BRANCH(1);                                    /* B3 */
            if (quote != '\'' && ch == '\\' && cursor + 1 < n) { cursor += 2; continue; }
            if (ch == quote) quote = 0;
            cursor++; continue;
        }
        WCUP_BRANCH(0);
        if (ch == '\\' && cursor + 1 < n) { WCUP_BRANCH(1); cursor += 2; continue; } /* B4 */
        WCUP_BRANCH(0);
        if (ch == '\'' || ch == '"' || ch == '`') { WCUP_BRANCH(1); quote = ch; cursor++; continue; } /* B5 */
        WCUP_BRANCH(0);
        if (ch == '#') {
            char prev = (cursor > 0) ? command[cursor - 1] : '\0';
            if (cursor == 0 || isspace((unsigned char)prev) || prev == ';' ||
                prev == '|' || prev == '(' || prev == ')') {
                WCUP_BRANCH(1);                                /* B6 */
                comment = true; cursor++; continue;
            }
            WCUP_BRANCH(0);
        } else {
            WCUP_BRANCH(0);
        }
        if (ch == '\n') { WCUP_BRANCH(1); goto done; }         /* B7 */
        WCUP_BRANCH(0);
        if (cursor + 3 <= n && strncmp(command + cursor, "<<<", 3) == 0) {
            WCUP_BRANCH(1); cursor += 3; continue;             /* B8 */
        }
        WCUP_BRANCH(0);
        if (cursor + 2 <= n && strncmp(command + cursor, "<<", 2) == 0) {
            WCUP_BRANCH(1);                                    /* B9 */
            size_t end; char *delim; bool st, q;
            if (!wc_parse_heredoc_operator(command, cursor, &end, &delim, &st, &q)) {
                WCUP_BRANCH(1);                                /* B10 */
                unknown = true;
                cursor += 2;
                continue;
            }
            WCUP_BRANCH(0);
            so += (size_t)snprintf(specs + so, sizeof specs - so, "%s,%d,%d;",
                                   delim, st ? 1 : 0, q ? 1 : 0);
            free(delim);
            cursor = end;
            continue;
        }
        WCUP_BRANCH(0);
        if (ch == ';' || ch == '|' || ch == '&') has_list = true;
        cursor++;
    }
done:
    {
        char res[2560];
        snprintf(res, sizeof res, "%zu|%s|%d|%d",
                 (comment || cursor < n) ? cursor : n, specs,
                 unknown ? 1 : 0, has_list ? 1 : 0);
        WCUP_VALSTR(res);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_scan_heredoc_command_unit");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _scan_heredoc_command_unit <cmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_scan(argv[i]);
    return 0;
}
