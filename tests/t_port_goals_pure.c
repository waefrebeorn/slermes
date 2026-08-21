/*
 * t_port_goals_pure.c — faithful parity harness for
 * src/hermes_cli/goals_pure.c (hermes_cli/goals.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_goals_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "goals_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *js(json_t *c, const char *k, const char *d) {
    return json_get_str(c, k, d);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture json\n"); free(raw); return 1; }

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_object_get_at(ops, i);
            const char *op = js(c, "op", "");
            if (strcmp(op, "parse_contract") == 0) {
                char *r = goals_parse_contract(js(c, "text", ""));
                printf("%s\n", r ? r : "{\"headline\":\"\",\"contract\":{}}");
                free(r);
            } else if (strcmp(op, "contract_is_empty") == 0) {
                json_t *d = json_object_get(c, "contract");
                char *dj = json_serialize(d);
                goals_contract_t *cc = goals_contract_new();
                goals_contract_from_json(cc, dj);
                int e = goals_contract_is_empty(cc);
                printf("{\"op\": \"contract_is_empty\", \"out\": %s}\n", e ? "true" : "false");
                goals_contract_free(cc); free(dj);
            } else if (strcmp(op, "contract_to_dict") == 0) {
                json_t *d = json_object_get(c, "contract");
                char *dj = json_serialize(d);
                goals_contract_t *cc = goals_contract_new();
                goals_contract_from_json(cc, dj);
                char *r = goals_contract_to_json(cc);
                printf("{\"op\": \"contract_to_dict\", \"out\": %s}\n", r ? r : "{}");
                goals_contract_free(cc); free(r); free(dj);
            } else if (strcmp(op, "contract_from_dict") == 0) {
                json_t *d = json_object_get(c, "contract");
                char *dj = json_serialize(d);
                goals_contract_t *cc = goals_contract_new();
                goals_contract_from_json(cc, dj);
                char *r = goals_contract_to_json(cc);
                printf("{\"op\": \"contract_from_dict\", \"out\": %s}\n", r ? r : "{}");
                goals_contract_free(cc); free(r); free(dj);
            } else if (strcmp(op, "contract_render") == 0) {
                json_t *d = json_object_get(c, "contract");
                char *dj = json_serialize(d);
                goals_contract_t *cc = goals_contract_new();
                goals_contract_from_json(cc, dj);
                char *r = goals_contract_render_block(cc);
                printf("{\"op\": \"contract_render\", \"out\": \"%s\"}\n", r ? r : "");
                goals_contract_free(cc); free(r); free(dj);
            } else if (strcmp(op, "gate_to_dict") == 0) {
                json_t *g = json_object_get(c, "gate");
                char *gj = json_serialize(g);
                goals_gate_t *gg = goals_gate_new();
                goals_gate_from_json(gg, gj);
                char *r = goals_gate_to_json(gg);
                printf("{\"op\": \"gate_to_dict\", \"out\": %s}\n", r ? r : "{}");
                goals_gate_free(gg); free(r); free(gj);
            } else if (strcmp(op, "gate_from_dict") == 0) {
                json_t *g = json_object_get(c, "gate");
                char *gj = json_serialize(g);
                goals_gate_t *gg = goals_gate_new();
                goals_gate_from_json(gg, gj);
                char *r = goals_gate_to_json(gg);
                printf("{\"op\": \"gate_from_dict\", \"out\": %s}\n", r ? r : "{}");
                goals_gate_free(gg); free(r); free(gj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
