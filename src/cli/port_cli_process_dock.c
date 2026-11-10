/*
 * port_cli_process_dock.c — pure helpers from hermes_cli/cli_process_dock.py.
 *
 * _STATUS_BY_REASON = {"killed":"killed","lost":"lost","failed_start":"failed"}
 * _GLYPH = {"running":"⚙","done":"✔","failed":"✘","killed":"✘","lost":"?"}
 */

#include "port_cli_process_dock.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *cpa_last_output_line(const char *buffer)
{
    /* Last line (by \n) that has non-space content, its tokens collapsed to
     * single spaces. */
    const char *b = buffer ? buffer : "";
    size_t len = strlen(b), i = 0;
    char *winner = NULL;
    while (i <= len) {
        size_t ls = i;
        while (i < len && b[i] != '\n') i++;
        size_t le = i; /* line = b[ls..le), \n excluded */
        if (le > ls && b[le - 1] == '\r') le--;
        {
            char *tmp = (char *)malloc(le - ls + 1);
            size_t tl = 0;
            int in = 0;
            if (tmp) {
                for (size_t k = ls; k < le; k++) {
                    if (isspace((unsigned char)b[k])) {
                        if (in && tl) tmp[tl++] = ' ';
                        in = 0;
                    } else {
                        tmp[tl++] = b[k];
                        in = 1;
                    }
                }
                while (tl > 0 && tmp[tl - 1] == ' ') tl--;
                tmp[tl] = '\0';
                if (tl > 0) {
                    free(winner);
                    winner = tmp;
                } else {
                    free(tmp);
                }
            }
        }
        if (i < len) i++; else break;
    }
    if (!winner) {
        winner = (char *)malloc(1);
        if (winner) winner[0] = '\0';
    }
    return winner;
}

const char *cpa_process_status(int exited, const char *completion_reason,
                               int has_exit_code)
{
    static const char *const KILLED = "killed";
    static const char *const LOST = "lost";
    static const char *const FAILED = "failed";
    static const char *const DONE = "done";
    static const char *const RUNNING = "running";
    const char *reason = completion_reason ? completion_reason : "";
    if (!exited)
        return RUNNING;
    if (strcmp(reason, "killed") == 0) return KILLED;
    if (strcmp(reason, "lost") == 0) return LOST;
    if (strcmp(reason, "failed_start") == 0) return FAILED;
    return has_exit_code ? FAILED : DONE;
}

const char *cpa_process_glyph(const char *status)
{
    const char *s = status ? status : "";
    if (strcmp(s, "running") == 0) return "\xE2\x9A\x99"; /* ⚙ */
    if (strcmp(s, "done") == 0) return "\xE2\x9C\x94";     /* ✔ */
    if (strcmp(s, "failed") == 0) return "\xE2\x9C\x98";   /* ✘ */
    if (strcmp(s, "killed") == 0) return "\xE2\x9C\x98";
    if (strcmp(s, "lost") == 0) return "?";
    return "?";
}
