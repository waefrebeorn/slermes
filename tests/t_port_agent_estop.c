/* t_port_agent_estop.c — oracle harness for the agent/estop.py emergency-stop port.
 *
 * Exercises the faithful state machine of agent_estop (agent/estop.py):
 *   - sentinel absent  -> is_engaged()==0, engage() creates it, disengage()==1
 *   - sentinel present -> is_engaged()==1, engage() updates body, disengage()==0
 *   - /pause off (via NULL arg) engages; explicit "off" disengages
 *
 * Each case prints one JSON object per line. The Python oracle reproduces the
 * SAME transitions from agent/estop.py and diffs the results 1:1.
 *
 * HERMES_HOME is redirected to the fixtures case dir (passed as argv[1]) so the
 * ESTOP sentinel file lands there and is cleaned up per-case. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_estop.h"

static char *json_quote(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 2 + 1); /* worst case */
    if (!out) return strdup("\"\"");
    char *q = out;
    *q++ = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c < 32) { q += sprintf(q, "\\u%04x", c); }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return out;
}

/* emit a single JSON record for one transition step */
static void emit(const char *step, const char *arg, int engaged, int rc, const char *sentinel)
{
    char *js = json_quote(sentinel);
    char *ja = json_quote(arg);
    printf("{\"step\":%s,\"arg\":%s,\"engaged\":%d,\"rc\":%d,\"sentinel\":%s}\n",
           json_quote(step), ja, engaged, rc, js);
    free(js); free(ja);
}

int main(int argc, char **argv)
{
    /* The runner exposes an isolated $TMPH via SLERMES_HOME / HERMES_HOME;
     * honor it (do NOT override) so the C harness and Python oracle agree on
     * the sentinel location. Clean any stale sentinel first for deterministic
     * CASE-1 entry. */
    {
        char *sp0 = estop_sentinel_path();
        if (sp0) { remove(sp0); free(sp0); }
    }

    /* CASE 1: initially not engaged, engage with a reason, then disengage */
    int engaged = estop_is_engaged();            /* expect 0 */
    char *sp = estop_sentinel_path();
    emit("initial_not_engaged", "", engaged, engaged, sp);
    free(sp);

    char *engaged_path = estop_engage("test reason");  /* engage */
    int e2 = estop_is_engaged();               /* expect 1 */
    emit("after_engage", "test reason", e2, 1, engaged_path);
    free(engaged_path);

    int dis = estop_disengage();               /* expect 1 (was paused) */
    int e3 = estop_is_engaged();               /* expect 0 */
    char *sp2 = estop_sentinel_path();
    emit("after_disengage", "off", e3, dis, sp2);
    free(sp2);

    /* CASE 2: disengage when not engaged (expect 0, no-op) */
    int dis2 = estop_disengage();              /* already gone -> 0 */
    emit("disengage_not_engaged", "off", estop_is_engaged(), dis2, estop_sentinel_path());

    /* CASE 3: re-engage without reason, then re-engage (idempotent update) */
    char *p3 = estop_engage(NULL);
    emit("engage_no_reason", "", estop_is_engaged(), 1, p3);
    free(p3);

    char *p4 = estop_engage("second reason");
    emit("engage_again_updates", "second reason", estop_is_engaged(), 1, p4);
    free(p4);

    estop_disengage();  /* cleanup */

    return 0;
}
