/* gate: shell_hooks_evaluate_result — traced copy of
 * src/agent/shell_hooks_ports.c::shk_evaluate_result
 * (LIVE twin: agent/shell_hooks.py::_evaluate_result).
 *
 * Input format (one string per argv slot):
 *   <event>|<fail_closed 0|1>|<error>|<timed_out 0|1>|<returncode>|<stdout>|<stderr>
 * ('|' separators; empty fields = absent/empty)
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1 blocking_event   B2 fail_closed && blocking
 *   B3 spawn error      B4 timed_out      B5 stderr non-empty
 *   B6 rc==2 && blocking     B7 stdout-parsed directive is block
 *   B8 rc != 0
 *   B9 normal-path parsed directive exists
 *   B10 fail_closed && stdout non-empty    B11 valid JSON object
 *   VSTR(outcome) in {none, block, other}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

#define SHK_BLOCK_EXIT_CODE 2

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* Mirrors shk_parse_response classification: NULL | "block" | "other". */
static const char *parse_directive(const char *stdout_json)
{
    if (!stdout_json || !*stdout_json) return NULL;
    const char *s = stdout_json;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (*s == '\0') return NULL;
    json_t *data = json_parse(s, NULL);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return NULL;
    }
    const char *action = json_get_str(data, "action", NULL);
    const char *decision = json_get_str(data, "decision", NULL);
    const char *result = NULL;
    if ((action && strcmp(action, "block") == 0) ||
        (decision && strcmp(decision, "block") == 0)) {
        result = "block";
    } else {
        const char *ctx = json_get_str(data, "context", NULL);
        if (ctx && *ctx) result = "other";
    }
    json_free(data);
    return result;
}

static void split_input(char *in, char **ev, int *fc, char **err, int *to,
                        int *rc, char **out, char **er)
{
    *ev = in;
    char *p = in;
    char *f[6];
    int n = 0;
    f[n++] = p;
    while (*p && n < 7) {
        if (*p == '|') { *p = '\0'; f[n++] = p + 1; }
        p++;
    }
    while (n < 7) f[n++] = p + strlen(p);
    *ev = f[0]; *fc = atoi(f[1]); *err = f[2]; *to = atoi(f[3]);
    *rc = atoi(f[4]); *out = f[5]; *er = f[6];
}

static void evaluate(const char *event_in, int fail_closed_in,
                     const char *error_in, int timed_out_in,
                     int returncode_in, const char *stdout_in,
                     const char *stderr_in)
{
    WCUP_BEGIN("evaluate_result");
    const char *outcome;

    bool blocking_event = event_in && strcmp(event_in, "pre_tool_call") == 0;
    WCUP_BRANCH(blocking_event);                          /* B1 */
    bool fc = fail_closed_in && blocking_event;
    WCUP_BRANCH(fc);                                      /* B2 */

    if (error_in && *error_in) {
        WCUP_BRANCH(1);                                   /* B3 */
        outcome = fc ? "block" : "none";
        goto emit;
    }
    WCUP_BRANCH(0);

    if (timed_out_in) {
        WCUP_BRANCH(1);                                   /* B4 */
        outcome = fc ? "block" : "none";
        goto emit;
    }
    WCUP_BRANCH(0);

    {
        char stderr_lim[512] = {0};
        if (stderr_in && *stderr_in)
            snprintf(stderr_lim, sizeof stderr_lim, "%.*s", 400, stderr_in);
        WCUP_BRANCH(stderr_lim[0] != 0);                  /* B5 */

        if (returncode_in == SHK_BLOCK_EXIT_CODE && blocking_event) {
            WCUP_BRANCH(1);                               /* B6 */
            const char *parsed = parse_directive(stdout_in);
            WCUP_BRANCH(parsed != NULL);                  /* B7 */
            outcome = (parsed && strcmp(parsed, "block") == 0) ? "block" : "block";
            goto emit;
        }
        WCUP_BRANCH(0);

        WCUP_BRANCH(returncode_in != 0);                  /* B8 */

        const char *parsed = parse_directive(stdout_in);
        WCUP_BRANCH(parsed != NULL);                      /* B9 */

        if (!parsed && fc && stdout_in && *stdout_in) {
            WCUP_BRANCH(1);                               /* B10 */
            /* valid-json-object probe: parse again */
            const char *s = stdout_in;
            while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
            json_t *d = (*s) ? json_parse(s, NULL) : NULL;
            bool valid = (d && d->type == JSON_OBJECT);
            if (d) json_free(d);
            WCUP_BRANCH(valid);                           /* B11 */
            if (!valid) { outcome = "block"; goto emit; }
            outcome = "none";
            goto emit;
        }
        WCUP_BRANCH(0);
        outcome = parsed ? (strcmp(parsed, "block") == 0 ? "block" : "other")
                         : "none";
    }

emit:
    WCUP_VALSTR(outcome ? outcome : "none");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
    }
    WCUP_END("evaluate_result");
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *buf = xstrdup(argv[i]);
        char *ev, *out, *er, *err;
        int fc, to, rc;
        split_input(buf, &ev, &fc, &err, &to, &rc, &out, &er);
        evaluate(ev, fc, err, to, rc, out, er);
        free(buf);
    }
    return 0;
}
