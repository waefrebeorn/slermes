/*
 * run_goals.c - Goal/heartbeat post-turn helpers (C11 port).
 *
 * PoP: w6_rungoals_ec4200_final_text @ gateway/run_goals.py:_final_text_for_post_turn_hooks
 * PoP: w6_rungoals_ec4200_goal_max_turns @ gateway/run_goals.py:_goal_max_turns_from_config
 *
 * The LIVE module is gateway/run_goals.py (GatewayGoalsMixin). Both helpers are
 * effectively pure: the gate supplies their inputs as pre-resolved scalars
 * (the "goals" config block is supplied already resolved from the runner config).
 */
#include "noc_w6_rungoals_ec4200.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int py_blank(const char *s) {
    if (!s) return 1;
    while (*s && py_space((unsigned char)*s)) s++;
    return *s == '\0';
}
static const char *py_trim(const char *s) {
    static char buf[4100];
    size_t n = 0;
    const char *a = s ? s : "";
    while (*a && py_space((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && py_space((unsigned char)b[-1])) b--;
    while (a < b && n < sizeof(buf) - 1) buf[n++] = *a++;
    buf[n] = '\0';
    return buf;
}

const char *w6_rungoals_ec4200_final_text(const char *text, int has_event, const char *streamed) {
    const char *t = text ? text : "";
    if (!py_blank(t)) return t;                 /* str.strip() truthy -> return text as-is */
    if (has_event && streamed && !py_blank(streamed)) return streamed;
    return t;
}

long w6_rungoals_ec4200_goal_max_turns(int kind, const char *val) {
    if (kind == 0) return 20;                   /* falsy raw value, or missing */
    if (kind == 1) {                            /* number: falsy iff 0; int() truncates */
        double d = val ? atof(val) : 0.0;
        if (d == 0.0) return 20;
        return (long)d;
    }
    if (kind == 2) {                            /* string: "" falsy; int(str) whole integer */
        const char *s = py_trim(val);
        if (!*s) return 20;
        char *end = NULL;
        errno = 0;
        long long x = strtoll(s, &end, 10);
        if (!end || end == s || *end != '\0' || errno == ERANGE) return 20;
        return (long)x;
    }
    if (kind == 3) return (val && !strcmp(val, "1")) ? 1 : 20;  /* bool */
    return 20;
}

