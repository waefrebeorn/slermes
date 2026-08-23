/* gate: goals_parse_contract — traced copy of
 * src/hermes_cli/goals_data.c::parse_contract
 * (PoP: hermes_cli/goals.py:parse_contract).
 * Input: raw goal text (real newlines on argv). Output V event:
 *   headline \x1f outcome \x1f verification \x1f constraints \x1f boundaries \x1f stop_when
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "watercup.h"

#define N_FIELDS 5
static const char *FIELDS[N_FIELDS] =
    {"outcome", "verification", "constraints", "boundaries", "stop_when"};

static int field_index(const char *f) {
    for (int i = 0; i < N_FIELDS; i++) if (strcmp(FIELDS[i], f) == 0) return i;
    return -1;
}

static const char *alias_to_field(const char *buf) {
#define ALIAS2(a, f) if (strcmp(buf, a) == 0) return f;
    ALIAS2("outcome", "outcome")
    ALIAS2("goal", "outcome")
    ALIAS2("done", "outcome")
    ALIAS2("done when", "outcome")
    ALIAS2("verification", "verification")
    ALIAS2("verify", "verification")
    ALIAS2("verified by", "verification")
    ALIAS2("evidence", "verification")
    ALIAS2("proof", "verification")
    ALIAS2("constraints", "constraints")
    ALIAS2("constraint", "constraints")
    ALIAS2("preserve", "constraints")
    ALIAS2("must not", "constraints")
    ALIAS2("do not change", "constraints")
    ALIAS2("boundaries", "boundaries")
    ALIAS2("boundary", "boundaries")
    ALIAS2("scope", "boundaries")
    ALIAS2("allowed", "boundaries")
    ALIAS2("files", "boundaries")
    ALIAS2("stop when", "stop_when")
    ALIAS2("stop_when", "stop_when")
    ALIAS2("blocked", "stop_when")
    ALIAS2("stop if blocked", "stop_when")
    ALIAS2("give up when", "stop_when")
#undef ALIAS2
    return NULL;
}

static char *lower_dup(const char *s, size_t n) {
    char *b = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        b[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
    b[n] = '\0';
    return b;
}

static void run_one(const char *text) {
    char out[8192]; size_t ooff = 0; out[0] = '\0';
    char headline[4096]; size_t hlen = 0; headline[0] = '\0';
    char *field_lines[N_FIELDS][64]; size_t field_n[N_FIELDS];
    for (int i = 0; i < N_FIELDS; i++) field_n[i] = 0;

    WCUP_BEGIN("parse_contract");
    if (!text || !text[0]) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        const char *p = text;
        while (*p) {
            WCUP_LOOP_TICK();
            const char *ls = p;
            while (*p && *p != '\n') p++;
            const char *le = p;
            while (le > ls && (le[-1] == ' ' || le[-1] == '\t' || le[-1] == '\r')) le--;
            while (ls < le && (*ls == ' ' || *ls == '\t')) ls++;
            size_t tlen = (size_t)(le - ls);
            int blank = tlen == 0;
            int matched = 0;
            if (!blank && memchr(ls, ':', tlen)) {
                WCUP_BRANCH(1);
                const char *colon = memchr(ls, ':', tlen);
                size_t plen = (size_t)(colon - ls);
                while (plen > 0 && (ls[plen-1] == ' ' || ls[plen-1] == '\t')) plen--;
                const char *val = colon + 1;
                while (val < le && (*val == ' ' || *val == '\t')) val++;
                const char *ve = le;
                while (ve > val && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
                char *prefix = lower_dup(ls, plen);
                /* trim leading spaces of prefix */
                const char *alias = alias_to_field(prefix);
                free(prefix);
                if (alias && ve > val) {
                    WCUP_BRANCH(1);
                    int idx = field_index(alias);
                    char *v = strndup(val, (size_t)(ve - val));
                    field_lines[idx][field_n[idx]++] = v;
                    matched = 1;
                } else WCUP_BRANCH(0);
            } else WCUP_BRANCH(0);
            if (!matched && !blank) {
                WCUP_BRANCH(1);
                if (hlen) { WCUP_BRANCH(1); headline[hlen++] = ' '; }
                else WCUP_BRANCH(0);
                memcpy(headline + hlen, ls, tlen); hlen += tlen; headline[hlen] = '\0';
            } else WCUP_BRANCH(0);
            if (*p == '\n') p++;
        }
    }
    if (!ooff && hlen) { strcpy(out, headline); ooff = hlen; }
    for (int i = 0; i < N_FIELDS; i++) {
        out[ooff++] = '\x1f';
        for (size_t k = 0; k < field_n[i]; k++) {
            if (k) { out[ooff++] = ' '; }
            strcpy(out + ooff, field_lines[i][k]); ooff += strlen(field_lines[i][k]);
        }
        out[ooff] = '\0';
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_contract");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_contract <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
