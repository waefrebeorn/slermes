/* gate: agent_context_compressor_find_one_exchange — traced copy of
 * src/agent/context_compressor.c::cc_find_one_exchange (+ its helpers
 * cc_is_assistant_with_output / cc_msg_is_summary ->
 * cc_is_context_summary_message, transcribed rung-for-rung).
 * LIVE twin: agent/context_compressor.py::_find_one_exchange
 * (ContextCompressor method; summary detection via the LIVE
 * _is_context_summary_message classmethod).
 *
 * Input encoding (argv-safe): "<start>,<tail_start>;ROLE|C|T|S;..." where
 * ROLE in {user,assistant,tool,system}, C = content non-empty,
 * T = tool_calls non-empty, S = summary marker flag.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: start >= n or >= tail_start -> None
 *   LOOP walk past non-assistant/summary msgs:
 *     B2: assistant-with-output && !summary -> break
 *   B3: walk exhausted (>= tail_start or >= n) -> None
 *   LOOP consume turn (assistant/tool until user or summary):
 *     B4: role not assistant/tool -> break
 *     B5: summary marker -> break
 *   B6: idx <= exchange_start -> None (empty exchange)
 *   B7: idx >= n -> None (ran off end mid-turn)
 *   B8: boundary role is assistant/tool -> None (splice-adjacency guard)
 *   else return (exchange_start, idx)
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "watercup.h"

typedef struct {
    const char *role;
    bool content_nonempty;
    bool tool_calls_nonempty;
    bool summary;
} wmsg_t;

static bool msg_is_summary(const wmsg_t *m) {
    return m->summary;                    /* cc_is_context_summary_message */
}

static bool is_assistant_with_output(const wmsg_t *m) {
    if (strcmp(m->role, "assistant") != 0) return false;
    if (msg_is_summary(m)) return false;
    return m->content_nonempty || m->tool_calls_nonempty;
}

static void wc_find_one_exchange(const wmsg_t *msgs, int n, long start,
                                 long tail_start) {
    WCUP_BEGIN("find_one_exchange");
    long found_start = -1, found_end = -1;
    long idx = start;
    if (idx >= n || idx >= tail_start) {          /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    /* Walk past user/system messages and existing summary markers. */
    while (idx < tail_start && idx < n) {
        WCUP_LOOP_TICK();
        const wmsg_t *m = &msgs[idx];
        if (is_assistant_with_output(m)) {         /* B2 */
            WCUP_BRANCH(1);
            break;
        }
        WCUP_BRANCH(0);
        idx++;
    }
    if (idx >= tail_start || idx >= n) {           /* B3 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    long exchange_start = idx;

    /* Consume the full turn. */
    idx++;
    while (idx < tail_start && idx < n) {
        WCUP_LOOP_TICK();
        const wmsg_t *m = &msgs[idx];
        if (strcmp(m->role, "assistant") != 0 &&
            strcmp(m->role, "tool") != 0) {        /* B4 */
            WCUP_BRANCH(1);
            break;
        }
        WCUP_BRANCH(0);
        if (msg_is_summary(m)) {                   /* B5 */
            WCUP_BRANCH(1);
            break;
        }
        WCUP_BRANCH(0);
        idx++;
    }
    if (idx <= exchange_start) {                   /* B6 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    if (idx >= n) {                                /* B7 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        const wmsg_t *boundary = &msgs[idx];
        if (strcmp(boundary->role, "assistant") == 0 ||
            strcmp(boundary->role, "tool") == 0) { /* B8 */
            WCUP_BRANCH(1);
            goto out;
        }
        WCUP_BRANCH(0);
    }
    found_start = exchange_start;
    found_end = idx;

out:
    {
        char cup[32], res[64];
        snprintf(res, sizeof res, "%ld,%ld", found_start, found_end);
        WCUP_VALSTR(res);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_one_exchange");
    }
}

static void run_input(const char *arg) {
    /* parse "<start>,<tail_start>;tok;tok;..." */
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", arg);
    char *semi = strchr(buf, ';');
    if (!semi) { printf("bad-input\n"); return; }
    *semi = '\0';
    long start = atol(buf);
    long tail_start = 0;
    char *comma = strchr(semi - strlen(semi), ',');
    (void)comma;
    {
        char *c2 = strchr(buf, ',');
        if (!c2) { printf("bad-input\n"); return; }
        *c2 = '\0';
        start = atol(buf);
        tail_start = atol(c2 + 1);
    }

    wmsg_t msgs[128];
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(semi + 1, ";", &save); tok && n < 128;
         tok = strtok_r(NULL, ";", &save)) {
        char *p1 = strchr(tok, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        char *p3 = p2 ? strchr(p2 + 1, '|') : NULL;
        if (!p3) { printf("bad-token\n"); return; }
        *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
        msgs[n].role = tok;
        msgs[n].content_nonempty = p1[1] == '1';
        msgs[n].tool_calls_nonempty = p2[1] == '1';
        msgs[n].summary = p3[1] == '1';
        n++;
    }
    wc_find_one_exchange(msgs, n, start, tail_start);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s find_one_exchange <start,tail;msgs>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) run_input(argv[i]);
    return 0;
}
