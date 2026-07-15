/* Oracle harness for gateway/restart_loop_guard.py port.
 * Reads fixture path from argv[1]. Fixture JSON:
 *   {"home": "/tmp/xxx", "now": 1000.0, "cases":[{"op":..., "args":[...]}]}
 * Performs each op against `home` (HERMES_HOME) and prints compact JSON array.
 * Diff against sta_oracle_restart_loop_guard.py (same temp home).
 */
#include "gateway/restart_loop_guard.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

static char *json_str_dup(json_t *node) {
    if (!node || node->type != JSON_STRING) return NULL;
    return strdup(node->str_val ? node->str_val : "");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    char home[PATH_MAX]; home[0] = '\0';
    json_t *h = json_object_get(root, "home");
    if (h && h->type == JSON_STRING) snprintf(home, sizeof(home), "%s", h->str_val ? h->str_val : "");

    double now = 1000.0;
    json_t *nw = json_object_get(root, "now");
    if (nw && nw->type == JSON_NUMBER) now = nw->num_val;

    printf("[");
    int first = 1;
    json_t *cases = json_object_get(root, "cases");
    if (cases && cases->type == JSON_ARRAY) {
        for (size_t i = 0; i < cases->c.count; i++) {
            json_t *c = cases->c.items[i];
            if (!c || c->type != JSON_OBJECT) continue;
            char *op = json_str_dup(json_object_get(c, "op"));
            json_t *a = json_object_get(c, "args");
            if (!op) continue;

            if (strcmp(op, "record") == 0) {
                int ws = 60;
                if (a && a->type == JSON_ARRAY && a->c.count > 0 && a->c.items[0]->type == JSON_NUMBER)
                    ws = (int)a->c.items[0]->num_val;
                restart_loop_record_boot(ws, now, home);
                /* Python's record_restart_interrupted_boot returns the pruned+appended list;
                 * emit it to match the oracle output. */
                double boots[64];
                int bn = restart_loop_load_boots(boots, 64, home);
                if (!first) printf(",");
                printf("[");
                for (int k = 0; k < bn; k++) { if (k) printf(","); printf("%.1f", boots[k]); }
                printf("]");
                first = 0;
            } else if (strcmp(op, "is_tripped") == 0) {
                int mr = 3, ws = 60;
                if (a && a->type == JSON_ARRAY) {
                    if (a->c.count > 0 && a->c.items[0]->type == JSON_NUMBER) mr = (int)a->c.items[0]->num_val;
                    if (a->c.count > 1 && a->c.items[1]->type == JSON_NUMBER) ws = (int)a->c.items[1]->num_val;
                }
                bool r = restart_loop_is_tripped(mr, ws, now, home);
                if (!first) printf(",");
                printf("%s", r ? "true" : "false");
                first = 0;
            } else if (strcmp(op, "check_and_record") == 0) {
                int mr = 3, ws = 60;
                if (a && a->type == JSON_ARRAY) {
                    if (a->c.count > 0 && a->c.items[0]->type == JSON_NUMBER) mr = (int)a->c.items[0]->num_val;
                    if (a->c.count > 1 && a->c.items[1]->type == JSON_NUMBER) ws = (int)a->c.items[1]->num_val;
                }
                bool r = restart_loop_check_and_record(mr, ws, now, home);
                if (!first) printf(",");
                printf("%s", r ? "true" : "false");
                first = 0;
            } else if (strcmp(op, "clear") == 0) {
                restart_loop_clear(home);
                if (!first) printf(",");
                printf("\"ok\"");
                first = 0;
            } else if (strcmp(op, "load") == 0) {
                double boots[64];
                int n = restart_loop_load_boots(boots, 64, home);
                if (!first) printf(",");
                printf("[");
                for (int k = 0; k < n; k++) { if (k) printf(","); printf("%.1f", boots[k]); }
                printf("]");
                first = 0;
            }
            free(op);
        }
    }
    printf("]");
    json_free(root);
    free(src);
    return 0;
}
