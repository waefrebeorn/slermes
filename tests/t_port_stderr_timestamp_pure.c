/*
 * t_port_stderr_timestamp_pure.c — faithful parity harness for
 * src/hermes_cli/stderr_timestamp_pure.c (hermes_cli/stderr_timestamp.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_stderr_timestamp_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "stderr_timestamp_pure.h"

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

/* build argv array from a JSON array "argv" into a NULL-terminated char** */
static char **build_argv(json_t *c, int *argc_out) {
    json_t *a = json_object_get(c, "argv");
    int n = a ? (int)json_len(a) : 0;
    char **argv = (char **)malloc(sizeof(char *) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) {
        json_t *e = json_array_get(a, i);
        const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
        argv[i] = (char *)s;
    }
    *argc_out = n;
    return argv;
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
            if (strcmp(op, "has_timestamp_prefix") == 0) {
                int r = stderr_ts_has_timestamp_prefix(js(c, "line", ""));
                printf("{\"op\": \"has_timestamp_prefix\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "command_exit_code") == 0) {
                int rc = (int)json_get_num(c, "rc", 0);
                printf("{\"op\": \"command_exit_code\", \"out\": %d}\n", stderr_ts_command_exit_code(rc));
            } else if (strcmp(op, "is_launchd_supervised") == 0) {
                const char *xpc = js(c, "xpc", "");
                int r = stderr_ts_is_launchd_supervised(xpc);
                printf("{\"op\": \"is_launchd_supervised\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "is_gateway_run_argv") == 0) {
                int na; char **av = build_argv(c, &na);
                int r = stderr_ts_is_gateway_run_argv(av, na);
                free(av);
                printf("{\"op\": \"is_gateway_run_argv\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "with_external_supervisor_flag") == 0) {
                int na; char **av = build_argv(c, &na);
                char *out[64]; int oc;
                stderr_ts_with_external_supervisor_flag(av, na, out, &oc);
                printf("{\"op\": \"with_external_supervisor_flag\", \"out\": [");
                for (int k = 0; k < oc; k++) {
                    if (k) printf(", ");
                    printf("\"%s\"", out[k]);
                }
                printf("]}\n");
                free(av);
            } else if (strcmp(op, "prepare_child_command") == 0) {
                int na; char **av = build_argv(c, &na);
                const char *xpc = js(c, "xpc", "");
                char *out[64]; int oc;
                stderr_ts_prepare_child_command(av, na, xpc, out, &oc);
                printf("{\"op\": \"prepare_child_command\", \"out\": [");
                for (int k = 0; k < oc; k++) {
                    if (k) printf(", ");
                    printf("\"%s\"", out[k]);
                }
                printf("]}\n");
                free(av);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
