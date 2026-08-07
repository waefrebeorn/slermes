/* Oracle harness for cli.py _status_bar_goal_segment + _fmt_stash_age. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libjson/json.h"
#include "port_cli_status_pure.h"

static void json_print_str(const char *s)
{
    putchar('"');
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') printf("\\\"");
            else if (c == '\\') printf("\\\\");
            else if (c == '\n') printf("\\n");
            else if (c == '\r') printf("\\r");
            else if (c == '\t') printf("\\t");
            else if (c < 0x20) printf("\\u%04x", c);
            else printf("%c", c);
        }
    }
    putchar('"');
}

static double mono_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { free(err); return 1; }
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return 1;
    }

    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *oj = json_obj_get(c, "op");
        const char *op = oj && oj->type == JSON_STRING ? oj->str_val : "";

        if (strcmp(op, "_status_bar_goal_segment") == 0) {
            json_t *sj = json_obj_get(c, "snapshot");
            const char *snap = sj && sj->type == JSON_STRING ? sj->str_val : "{}";
            char *r = cli_status_goal_segment(snap);
            json_print_str(r ? r : "");
            printf("\n");
            free(r);
        } else if (strcmp(op, "_fmt_stash_age") == 0) {
            /* stashed_at is computed as now - delta_secs on BOTH sides so the
             * monotonic clock skew is identical. Deltas stay far from the
             * 10/90/3600 boundaries. */
            json_t *dj = json_obj_get(c, "delta_secs");
            double delta = dj && dj->type == JSON_NUMBER ? dj->num_val : 60.0;
            double now = mono_now();
            double stashed_at = now - delta;
            char *r = cli_status_fmt_stash_age(stashed_at, now);
            json_print_str(r ? r : "");
            printf("\n");
            free(r);
        } else {
            printf("UNKNOWN_OP\n");
        }
    }

    json_free(root);
    return 0;
}
