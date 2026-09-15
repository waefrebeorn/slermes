/* gate: session_search_order_for_recall
 * Traced copy of src/tools/session_search_tool.c::order_for_recall
 * (partition of a JSON results array: demoted "cron" rows move after all
 * interactive rows, original BM25 order preserved within each class).
 * LIVE twin: tools/session_search_tool.py::_order_for_recall.
 *
 * Input argv: one JSON array literal per input; each element is an object
 * with an optional "source" string (e.g. {"source":"cron"} / {"source":"chat"}).
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: results is a non-empty JSON array (n >= 2 to act)
 *   LOOP over rows:
 *     B2: row source is demoted ("cron")
 *   VSTR("<demoted_count>:<total>") class census
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

/* faithful traced copy of the port body (returns a fresh array) */
static json_t *order_port(const json_t *results) {
    if (!results || results->type != JSON_ARRAY) return NULL;
    size_t n = json_len(results);
    if (n < 2) return NULL;
    json_t *front = json_array();
    json_t *back = json_array();
    for (size_t i = 0; i < n; i++) {
        const json_t *row = json_get(results, i);
        const char *src = json_is_object(row)
            ? json_get_str(row, "source", "") : "";
        bool demoted = src && strcmp(src, "cron") == 0;
        json_append(demoted ? back : front, json_copy(row));
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(front); i++) json_append(out, json_copy(json_get(front, i)));
    for (size_t i = 0; i < json_len(back); i++) json_append(out, json_copy(json_get(back, i)));
    json_free(front);
    json_free(back);
    return out;
}

static void run_one(const char *arg) {
    WCUP_BEGIN("_order_for_recall");
    json_t *rows = json_parse(arg ? arg : "", NULL);
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    int demoted = 0;

    WCUP_BRANCH(n >= 2);                                        /* B1 */
    if (n >= 2) {
        for (size_t i = 0; i < n; i++) {
            WCUP_LOOP_TICK();                                   /* LOOP */
            const json_t *row = json_get(rows, i);
            const char *src = json_is_object(row)
                ? json_get_str(row, "source", "") : "";
            int d = src && strcmp(src, "cron") == 0;
            WCUP_BRANCH(d);                                     /* B2 */
            demoted += d;
        }
    }
    json_t *ordered = order_port(rows);
    if (ordered) json_free(ordered);
    if (rows) json_free(rows);

    char vs[32];
    snprintf(vs, sizeof vs, "%d:%zu", demoted, n);
    WCUP_VALSTR(vs);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_order_for_recall");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s order_for_recall <json_array...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
