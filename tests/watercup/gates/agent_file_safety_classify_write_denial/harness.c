/* gate: agent_file_safety_classify_write_denial
 * Links the REAL C port src/agent/file_safety.c (compiled with -Dstatic= so
 * classify_write_denial + its helper predicates are linkable) against LIVE
 * agent/file_safety.py::_classify_write_denial.
 *
 * Input: one raw path token per cup (may use ~/).
 *
 * Branch ladder (twin.py mirrors EXACTLY — each predicate is computed with
 * each side's own real logic; the final verdict comes from the REAL fn):
 *   B1 approval path (~/.ssh/config)
 *   B2 exact-denied membership
 *   B3 denied-prefix membership
 *   B4 hermes .env
 *   B5 hermes mcp-tokens dir
 *   B6 hermes pairing dir
 *   B7 outside safe write roots
 * Value: final verdict string ("credential"/"safe_root"/"" for None).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
/* include watercup.h with the command-line -Dstatic= suspended so its
 * static-inline tracer helpers keep internal linkage */
#pragma push_macro("static")
#undef static
#include "watercup.h"
#pragma pop_macro("static")

/* exposed from src/agent/file_safety.c via -Dstatic= */
#include <stdbool.h>
const char *classify_write_denial(const char *resolved);
bool denied_exact_match(const char *resolved);
bool denied_prefix_match(const char *resolved);
bool outside_safe_root(const char *resolved);
#define classify_probe_exact   denied_exact_match
#define classify_probe_prefix  denied_prefix_match
#define classify_probe_safe    outside_safe_root

void resolve_like_live(const char *in, char *out, size_t cap) {
    /* os.path.realpath(os.path.expanduser(str(path))) */
    char expanded[PATH_MAX];
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        snprintf(expanded, sizeof expanded, "%s%s", home, in + 1);
    } else {
        snprintf(expanded, sizeof expanded, "%s", in);
    }
    if (!realpath(expanded, out)) {
        /* python realpath is purely lexical for missing paths */
        snprintf(out, cap, "%s", expanded);
    }
}

void run_one(const char *raw) {
    WCUP_BEGIN("classify_write_denial");
    char resolved[PATH_MAX];
    resolve_like_live(raw, resolved, sizeof resolved);

    /* B1: approval-gated path (~/.ssh/config) — LIVE returns None there. */
    char home[PATH_MAX];
    {
        const char *h = getenv("HOME");
        snprintf(home, sizeof home, "%s", h ? h : "/root");
    }
    char buf[PATH_MAX], rbuf[PATH_MAX];
    int p_approval = 0;
    snprintf(buf, sizeof buf, "%s/.ssh/config", home);
    if (realpath(buf, rbuf) && strcmp(resolved, rbuf) == 0) p_approval = 1;
    WCUP_BRANCH(p_approval);                                    /* B1 */

    /* B2: exact-denied membership (real C helper). */
    WCUP_BRANCH(classify_probe_exact(resolved));                /* B2 */
    /* B3: denied-prefix membership (real C helper). */
    WCUP_BRANCH(classify_probe_prefix(resolved));               /* B3 */

    /* B4/B5/B6: hermes home/root control paths. */
    const char *env_hh = getenv("HERMES_HOME");
    char hh[PATH_MAX], hr[PATH_MAX];
    if (env_hh && *env_hh) { snprintf(hh, sizeof hh, "%s", env_hh); }
    else { snprintf(hh, sizeof hh, "%s/.hermes", home); }
    snprintf(hr, sizeof hr, "%s", hh); /* C: root defaults to home */

    int p_env = 0, p_mcp = 0, p_pairing = 0;
    const char *bases[2] = { hh, hr };
    for (int i = 0; i < 2; i++) {
        snprintf(buf, sizeof buf, "%s/.env", bases[i]);
        if (realpath(buf, rbuf) && strcmp(resolved, rbuf) == 0) p_env = 1;
        snprintf(buf, sizeof buf, "%s/mcp-tokens", bases[i]);
        if (realpath(buf, rbuf)) {
            if (strcmp(resolved, rbuf) == 0 ||
                (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/'))
                p_mcp = 1;
        }
        snprintf(buf, sizeof buf, "%s/pairing", bases[i]);
        if (realpath(buf, rbuf)) {
            if (strcmp(resolved, rbuf) == 0 ||
                (strncmp(resolved, rbuf, strlen(rbuf)) == 0 && resolved[strlen(rbuf)] == '/'))
                p_pairing = 1;
        }
    }
    WCUP_BRANCH(p_env);                                         /* B4 */
    WCUP_BRANCH(p_mcp);                                         /* B5 */
    WCUP_BRANCH(p_pairing);                                     /* B6 */

    /* B7: outside HERMES_WRITE_SAFE_ROOT (real C helper; unset -> false). */
    WCUP_BRANCH(classify_probe_safe(resolved));                 /* B7 */

    const char *verdict = classify_write_denial(resolved);
    WCUP_VALSTR(verdict ? verdict : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("classify_write_denial");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s classify_write_denial <path>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
