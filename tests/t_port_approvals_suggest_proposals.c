/* Oracle harness for hermes_cli/approvals_suggest.py build_proposals / add_example. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "port_approvals_suggest_proposals.h"
#include "approval.h"

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

/* Sort a char* array in-place (bubble sort for small N) */
static void sort_strs(char **arr, size_t n)
{
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (strcmp(arr[i], arr[j]) > 0) {
                char *t = arr[i]; arr[i] = arr[j]; arr[j] = t;
            }
}

static void print_proposal_json(const proposal_t *p)
{
    printf("{\"count\":%d,", proposal_count(p));

    /* examples (sorted) */
    size_t n_ex;
    const char **ex = proposal_examples(p, &n_ex);
    /* copy to mutable array for sorting */
    char **ex_sorted = calloc(n_ex ? n_ex : 1, sizeof(char *));
    for (size_t i = 0; i < n_ex; i++) ex_sorted[i] = (char *)ex[i];
    sort_strs(ex_sorted, n_ex);
    printf("\"examples\":[");
    for (size_t i = 0; i < n_ex; i++) {
        if (i) printf(",");
        json_print_str(ex_sorted[i]);
    }
    free(ex_sorted);
    printf("],");

    printf("\"kind\":");
    json_print_str(proposal_kind(p));

    printf(",\"pattern\":");
    json_print_str(proposal_pattern(p));

    /* classes (sorted) */
    size_t n_cls;
    const char **cls = proposal_classes(p, &n_cls);
    char **cls_sorted = calloc(n_cls ? n_cls : 1, sizeof(char *));
    for (size_t i = 0; i < n_cls; i++) cls_sorted[i] = (char *)cls[i];
    sort_strs(cls_sorted, n_cls);
    printf(",\"classes\":[");
    for (size_t i = 0; i < n_cls; i++) {
        if (i) printf(",");
        json_print_str(cls_sorted[i]);
    }
    free(cls_sorted);
    printf("]}");
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

        if (strcmp(op, "normalize_command") == 0) {
            json_t *cj = json_obj_get(c, "command");
            const char *cmd = cj && cj->type == JSON_STRING ? cj->str_val : "";
            char *norm = approval_normalize_command(cmd);
            json_print_str(norm ? norm : "");
            printf("\n");
            free(norm);
        } else if (strcmp(op, "build_proposals") == 0) {
            json_t *rj = json_obj_get(c, "records");
            json_t *ej = json_obj_get(c, "existing");
            json_t *mn = json_obj_get(c, "min_count");
            json_t *ln = json_obj_get(c, "limit");

            size_t n_rec = rj && rj->type == JSON_ARRAY ? rj->c.count : 0;
            /* records: array of (char*, char*) pointers */
            char **cmds = calloc(n_rec ? n_rec : 1, sizeof(char *));
            char **descs = calloc(n_rec ? n_rec : 1, sizeof(char *));
            const char ***records = calloc(n_rec ? n_rec : 1, sizeof(const char **));
            for (size_t ri = 0; ri < n_rec; ri++) {
                json_t *pair = json_get(rj, ri);
                records[ri] = malloc(2 * sizeof(const char *));
                if (pair->type == JSON_ARRAY && pair->c.count >= 2) {
                    json_t *cmd = json_get(pair, 0);
                    json_t *desc = json_get(pair, 1);
                    cmds[ri] = strdup(cmd && cmd->type == JSON_STRING ? cmd->str_val : "");
                    descs[ri] = strdup(desc && desc->type == JSON_STRING ? desc->str_val : "");
                } else {
                    cmds[ri] = strdup("");
                    descs[ri] = strdup("");
                }
                records[ri][0] = cmds[ri];
                records[ri][1] = descs[ri];
            }

            size_t n_ex = ej && ej->type == JSON_ARRAY ? ej->c.count : 0;
            const char **existing = calloc(n_ex ? n_ex : 1, sizeof(const char *));
            char **ex_strs = calloc(n_ex ? n_ex : 1, sizeof(char *));
            for (size_t ri = 0; ri < n_ex; ri++) {
                json_t *ej2 = json_get(ej, ri);
                if (ej2 && ej2->type == JSON_STRING) {
                    ex_strs[ri] = strdup(ej2->str_val);
                    existing[ri] = ex_strs[ri];
                }
            }

            int min_count = mn && mn->type == JSON_NUMBER ? (int)mn->num_val : 2;
            int limit = ln && ln->type == JSON_NUMBER ? (int)ln->num_val : 20;

            size_t n_out = 0;
            proposal_t **results = build_proposals(records, n_rec,
                existing, n_ex, min_count, limit, &n_out);

            printf("[");
            for (size_t ri = 0; ri < n_out; ri++) {
                if (ri) printf(",");
                print_proposal_json(results[ri]);
                proposal_free(results[ri]);
            }
            printf("]\n");

            free(results);
            for (size_t ri = 0; ri < n_rec; ri++) {
                free((void *)records[ri][0]);
                free((void *)records[ri][1]);
                free(records[ri]);
            }
            free(records);
            free(cmds);
            free(descs);
            for (size_t ri = 0; ri < n_ex; ri++) free(ex_strs[ri]);
            free(existing);
            free(ex_strs);
        } else if (strcmp(op, "add_example") == 0) {
            json_t *ej = json_obj_get(c, "examples");
            size_t n = ej && ej->type == JSON_ARRAY ? ej->c.count : 0;
            proposal_t *p = proposal_create("git push *", "glob");
            for (size_t ri = 0; ri < n; ri++) {
                json_t *e = json_get(ej, ri);
                if (e && e->type == JSON_STRING)
                    proposal_add_example(p, e->str_val);
            }
            print_proposal_json(p);
            printf("\n");
            proposal_free(p);
        } else {
            printf("UNKNOWN_OP\n");
        }
    }

    json_free(root);
    return 0;
}
