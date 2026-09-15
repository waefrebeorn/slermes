/* gate: agent_verification_evidence_equivalent_needles — traced copy of
 * src/agent/verification_evidence.c::verify_equivalent_needles, which is ALSO
 * linked via extra_src and called per input as an internal consistency check.
 * LIVE twin: agent/verification_evidence.py::_equivalent_needles.
 *
 * Input encoding: whitespace-separated needle tokens ("npm run build").
 * Output value event: candidates serialized "a b c|d e|f".
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   push needle
 *   B1: len>=3 && needle[1]=="run"
 *     LOOP pm in {npm,pnpm,yarn,bun}: B2 match -> push [pm, script]
 *   B3: len==1 && '/' in needle[0] -> push bash+sh forms
 *   B4: needle==["pytest"] -> LOOP 5 python-runner forms pushed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char ***verify_equivalent_needles(char **needle, int m, int *out_count,
                                  int **out_lens);

/* stubs so the REAL port links without dragging libdb/sqlite3 */
int db_open(const char *path) { (void)path; return 0; }
void db_close(void) {}
char *db_load(const char *key) { (void)key; return NULL; }
int db_save(const char *key, const char *value) { (void)key; (void)value; return -1; }

static void trace_one(char **needle, int m) {
    WCUP_BEGIN("equivalent_needles");
    char ser[2048]; int so = 0;
    /* push needle */
    for (int i = 0; i < m; i++)
        so += snprintf(ser + so, sizeof ser - so, "%s%s", i ? " " : "", needle[i]);
    if (m >= 3 && strcmp(needle[1], "run") == 0) {        /* B1 */
        WCUP_BRANCH(1);
        const char *pms[] = {"npm", "pnpm", "yarn", "bun"};
        for (int i = 0; i < 4; i++) {
            WCUP_LOOP_TICK();
            if (strcmp(needle[0], pms[i]) == 0) {         /* B2 */
                WCUP_BRANCH(1);
                so += snprintf(ser + so, sizeof ser - so, "|%s %s",
                               needle[0], needle[2]);
                break;
            }
            WCUP_BRANCH(0);
        }
    } else {
        WCUP_BRANCH(0);                                   /* B1 false */
    }
    if (m == 1 && strchr(needle[0], '/')) {               /* B3 */
        WCUP_BRANCH(1);
        so += snprintf(ser + so, sizeof ser - so, "|bash %s|sh %s",
                       needle[0], needle[0]);
    } else {
        WCUP_BRANCH(0);
    }
    if (m == 1 && strcmp(needle[0], "pytest") == 0) {     /* B4 */
        WCUP_BRANCH(1);
        static const char *const forms[][3] = {
            {"python", "-m", "pytest"}, {"python3", "-m", "pytest"},
            {"uv", "run", "pytest"}, {"poetry", "run", "pytest"},
            {"pipenv", "run", "pytest"}};
        for (int i = 0; i < 5; i++) {
            WCUP_LOOP_TICK();
            so += snprintf(ser + so, sizeof ser - so, "|%s %s %s",
                           forms[i][0], forms[i][1], forms[i][2]);
        }
    } else {
        WCUP_BRANCH(0);
    }
    WCUP_VALSTR(ser);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("equivalent_needles");

    /* consistency: REAL linked port must produce the same candidate set */
    int rc = 0, *rl = NULL;
    char ***real = verify_equivalent_needles(needle, m, &rc, &rl);
    int expected = 1;
    if (m >= 3 && !strcmp(needle[1], "run")) {
        const char *pms[] = {"npm", "pnpm", "yarn", "bun"};
        for (int i = 0; i < 4; i++) if (!strcmp(needle[0], pms[i])) expected++;
    }
    if (m == 1 && strchr(needle[0], '/')) expected += 2;
    if (m == 1 && !strcmp(needle[0], "pytest")) expected += 5;
    if (rc != expected) {
        fprintf(stderr, "real port count %d != traced %d\n", rc, expected);
        exit(3);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s equivalent_needles <tokens...>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *toks[64]; int m = 0;
        char *save = NULL;
        for (char *t = strtok_r(argv[i], " \t", &save); t && m < 64;
             t = strtok_r(NULL, " \t", &save))
            toks[m++] = t;
        trace_one(toks, m);
    }
    return 0;
}
