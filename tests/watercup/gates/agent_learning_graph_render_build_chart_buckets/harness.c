/* gate: agent_learning_graph_render_build_chart_buckets
 * Links the REAL C port src/hermes_cli/learning_graph_render.c::
 * learning_graph_render_build_chart_buckets (+ helpers.o, libjson) against
 * LIVE agent/learning_graph_render.py::_build_chart_buckets.
 *
 * Input argv token: compact JSON
 *   {"nodes":[...],"rec":{id:score,...},"timed":bool,
 *    "minTs":num|null,"maxTs":num|null,"max_rows":int}
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1 nodes empty            B2 untimed (rec["timed"] falsy)
 * Result value: one V per bucket "label|ts|skills|memories|rec|ids".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../include/hermes_json.h"
#include "watercup.h"

char *learning_graph_render_build_chart_buckets(const char *nodes_json,
                                                const char *rec_json,
                                                int max_rows);

static void summarize(json_t *buckets, char *out, size_t cap) {
    out[0] = '\0';
    if (!buckets || buckets->type != JSON_ARRAY) return;
    size_t n = json_array_size(buckets);
    size_t pos = 0;
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *b = json_array_get(buckets, i);
        if (!b || b->type != JSON_OBJECT) continue;
        const char *label = json_object_get_string(b, "label", "");
        double ts = json_object_get_number(b, "ts", 0);
        int skills = (int)json_object_get_number(b, "skills", 0);
        int mems = (int)json_object_get_number(b, "memories", 0);
        double rec = json_object_get_number(b, "rec", 0);
        const char *sep = first ? "" : ";";
        first = 0;
        int w = snprintf(out + pos, cap - pos, "%s%s|%.6f|%d|%d|%.6f",
                         sep, label, ts, skills, mems, rec);
        if (w < 0 || (size_t)w >= cap - pos) return;
        pos += (size_t)w;
        json_t *nd = json_object_get(b, "nodes");
        if (nd && nd->type == JSON_ARRAY) {
            for (size_t k = 0; k < nd->c.count; k++) {
                json_t *item = nd->c.items[k];
                const char *nid = item ? json_object_get_string(item, "id", "?") : "?";
                w = snprintf(out + pos, cap - pos, "%s", k == 0 ? "|" : ",");
                if (w < 0 || (size_t)w >= cap - pos) return;
                pos += (size_t)w;
                w = snprintf(out + pos, cap - pos, "%s", nid);
                if (w < 0 || (size_t)w >= cap - pos) return;
                pos += (size_t)w;
            }
        }
    }
}

static void run_one(const char *input) {
    WCUP_BEGIN("build_chart_buckets");
    json_t *req = json_parse(input ? input : "{}", NULL);
    json_t *nodes = req ? json_object_get(req, "nodes") : NULL;
    json_t *recj = req ? json_object_get(req, "rec") : NULL;
    int timed = req ? json_object_get_bool(req, "timed", 0) : 0;
    int max_rows = req ? (int)json_object_get_number(req, "max_rows", 1) : 1;

    size_t nnodes = (nodes && nodes->type == JSON_ARRAY) ? json_array_size(nodes) : 0;
    WCUP_BRANCH(nnodes == 0);                                   /* B1 */
    if (nnodes > 0) {
        WCUP_BRANCH(!timed);                                    /* B2 */
    }

    /* serialize the two sub-args back to compact JSON for the REAL port.
     * The port's rec object carries rec scores + timed + minTs/maxTs. */
    char *nodes_s = nodes ? json_dumps(nodes, 0) : strdup("[]");
    json_t *rec_full = json_object();
    if (recj && recj->type == JSON_OBJECT) {
        for (size_t k = 0; k < recj->c.count; k++)
            if (recj->c.keys[k] && recj->c.items[k])
                json_set(rec_full, recj->c.keys[k], json_copy(recj->c.items[k]));
    }
    json_set(rec_full, "timed", json_bool(timed != 0));
    json_t *mn = req ? json_object_get(req, "minTs") : NULL;
    json_t *mx = req ? json_object_get(req, "maxTs") : NULL;
    json_set(rec_full, "minTs", mn ? json_copy(mn) : json_null());
    json_set(rec_full, "maxTs", mx ? json_copy(mx) : json_null());
    char *rec_s = json_dumps(rec_full, 0);
    json_t *out = NULL;
    if (nnodes > 0)
        out = json_parse(learning_graph_render_build_chart_buckets(
                             nodes_s ? nodes_s : "[]",
                             rec_s ? rec_s : "{}", max_rows),
                         NULL);
    char summary[4096];
    summarize(out, summary, sizeof summary);
    WCUP_VALSTR(summary);
    free(nodes_s); free(rec_s);
    if (out) json_free(out);
    if (req) json_free(req);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s build_chart_buckets <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
