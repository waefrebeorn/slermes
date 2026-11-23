/*
 * goal_command.c — C port of upstream Hermes hermes_cli/goal_command.py.
 *
 * PoP: w9_goalcommand_7b6c68_is_goal_control @ hermes_cli/goal_command.py:is_goal_control
 *
 * Faithful port of is_goal_control: strip + lowercase the argument, then the
 * command is a goal control when the whole normalized string is an exact
 * handler (the empty string is one) or its first whitespace token is one of
 * the non-destructive control verbs / wait / gate.
 */
#include "noc_w9_goalcommand_7b6c68.h"
#include <ctype.h>
#include <string.h>

static int w9_goalcommand_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int w9_goalcommand_7b6c68_is_goal_control(const char *arg) {
    char norm[8192];
    size_t n = 0, last = 0;
    const char *p = arg ? arg : "";

    while (*p && w9_goalcommand_is_ws((unsigned char)*p)) p++;
    for (; *p && n + 1 < sizeof(norm); p++) {
        unsigned char c = (unsigned char)*p;
        norm[n++] = (char)tolower(c);
        if (!w9_goalcommand_is_ws(c)) last = n;
    }
    norm[n] = '\0';
    norm[last] = '\0'; /* rstrip */

    /* _EXACT_HANDLERS keys (the empty string is deliberately a key). */
    static const char *exact[] = {
        "", "status", "show", "pause", "resume", "continue", "unpause",
        "clear", "stop", "done", "unwait", NULL
    };
    for (int i = 0; exact[i]; i++)
        if (!strcmp(norm, exact[i])) return 1;

    /* first = normalized.split(None, 1)[0] if normalized else "" */
    char first[8192];
    size_t fn = 0;
    for (size_t i = 0; i < n && fn + 1 < sizeof(first); i++) {
        if (w9_goalcommand_is_ws((unsigned char)norm[i])) break;
        first[fn++] = norm[i];
    }
    first[fn] = '\0';

    /* _CONTROL_FIRST_WORDS (bare control verbs + wait + gate). */
    static const char *control[] = {
        "resume", "continue", "unpause", "pause", "status", "show", "unwait",
        "wait", "gate", NULL
    };
    for (int i = 0; control[i]; i++)
        if (!strcmp(first, control[i])) return 1;
    return 0;
}
