/* gate: session_lost_and_found_classify_lost_and_found_row — traced C copy of
 * src/hermes_cli/session_lost_and_found_pure.c::laf_classify_row (+ its
 * private helpers). LIVE twin:
 * hermes_cli/session_lost_and_found.py::classify_lost_and_found_row.
 *
 * Input encoding (argv-safe): "<nfield>|<cell>[,<cell>...]" where each cell is
 *   N          -> SQL NULL
 *   S<text>    -> string value
 *
 * Rung ladder (identical both sides):
 *   B1 len(cells)>=3 and cells[0] is NULL
 *   B2 role-shaped row -> messages
 *   B3 cells[0] is a session id
 *   B4 nfield == 18 (model-usage layout)
 *   B5 model string present
 *   B6 nfield in {55,54,52} or ==14 (sessions layouts)
 *   B7 looks_like_source(cells[1])
 *   B8 nfield >= 30 and looks_like_source(cells[1])
 *   V classification result
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

#define CT_NULL 0
#define CT_STR  1

typedef struct { int type; const char *str; } cell_t;

static int laf_known_source(const char *v) {
    static const char *known[] = {
        "cli", "telegram", "discord", "slack", "whatsapp", "signal", "matrix",
        "irc", "email", "x", "twitter", "api", "gateway", "web", "dashboard",
        "tool", "subagent", "cron", "recovered", "imported", "acp", NULL
    };
    for (int i = 0; known[i]; i++) if (strcmp(v, known[i]) == 0) return 1;
    return 0;
}

static int laf_source_shape(const char *v) {
    size_t n = strlen(v);
    if (n == 0 || n > 32) return 0;
    if (!(v[0] >= 'a' && v[0] <= 'z')) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = v[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static int laf_looks_like_source(const char *v) {
    if (!v || !*v) return 0;
    return laf_known_source(v) || laf_source_shape(v);
}

static int laf_is_session_id(const char *v) {
    if (!v) return 0;
    for (int i = 0; i < 8; i++) if (v[i] < '0' || v[i] > '9') return 0;
    if (v[8] != '_') return 0;
    for (int i = 9; i < 15; i++) if (v[i] < '0' || v[i] > '9') return 0;
    if (v[15] != '_') return 0;
    return 1;
}

static int laf_is_role(const char *v) {
    return strcmp(v, "user") == 0 || strcmp(v, "assistant") == 0 ||
           strcmp(v, "tool") == 0 || strcmp(v, "system") == 0;
}

static void laf_classify(int nfield, const cell_t *cells, size_t ncells) {
    WCUP_BEGIN("classify_lost_and_found_row");
    const char *result = "null";
    const cell_t *c0 = (ncells > 0) ? &cells[0] : NULL;
    const cell_t *c1 = (ncells > 1) ? &cells[1] : NULL;
    const cell_t *c2 = (ncells > 2) ? &cells[2] : NULL;

    WCUP_BRANCH(ncells >= 3 && c0 && c0->type == CT_NULL);       /* B1 */
    if (ncells >= 3 && c0 && c0->type == CT_NULL) {
        WCUP_BRANCH(c1 && c1->type == CT_STR && c1->str && *c1->str &&
                    c2 && c2->type == CT_STR && laf_is_role(c2->str)); /* B2 */
        if (c1 && c1->type == CT_STR && c1->str && *c1->str &&
            c2 && c2->type == CT_STR && laf_is_role(c2->str))
            result = "messages";
        goto out;
    }

    WCUP_BRANCH(c0 && c0->type == CT_STR &&
                laf_is_session_id(c0->str));                     /* B3 */
    if (!(c0 && c0->type == CT_STR && laf_is_session_id(c0->str)))
        goto out;

    WCUP_BRANCH(nfield == 18);                                   /* B4 */
    if (nfield == 18) {
        WCUP_BRANCH(c1 && c1->type == CT_STR && *c1->str);       /* B5 */
        if (c1 && c1->type == CT_STR && *c1->str)
            result = "session_model_usage";
        goto out;
    }

    WCUP_BRANCH(nfield == 55 || nfield == 54 || nfield == 52 ||
                nfield == 14);                                   /* B6 */
    if (nfield == 55 || nfield == 54 || nfield == 52 || nfield == 14) {
        WCUP_BRANCH(ncells > 1 && c1 && c1->type == CT_STR &&
                    laf_looks_like_source(c1->str));             /* B7 */
        if (ncells > 1 && c1 && c1->type == CT_STR &&
            laf_looks_like_source(c1->str))
            result = "sessions";
        goto out;
    }

    WCUP_BRANCH(nfield >= 30 && ncells > 1 && c1 && c1->type == CT_STR &&
                laf_looks_like_source(c1->str));                 /* B8 */
    if (nfield >= 30 && ncells > 1 && c1 && c1->type == CT_STR &&
        laf_looks_like_source(c1->str))
        result = "sessions";

out:
    WCUP_VALSTR(result);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("classify_lost_and_found_row");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s classify_lost_and_found_row <nfield|cells>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *bar = strchr(argv[i], '|');
        if (!bar) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *bar = '\0';
        int nfield = atoi(argv[i]);
        cell_t cells[16];
        size_t nc = 0;
        char *tok = strtok(bar + 1, ",");
        while (tok && nc < 16) {
            if (strcmp(tok, "N") == 0) {
                cells[nc].type = CT_NULL; cells[nc].str = ""; nc++;
            } else if (tok[0] == 'S') {
                cells[nc].type = CT_STR; cells[nc].str = tok + 1; nc++;
            } else {
                fprintf(stderr, "bad cell: %s\n", tok); return 2;
            }
            tok = strtok(NULL, ",");
        }
        laf_classify(nfield, cells, nc);
    }
    return 0;
}
