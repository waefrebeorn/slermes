/*
 * t_port_web_git_base.c — oracle harness for base_branch_list +
 * review_list sort/untracked-fill parity.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "port_web_git.h"

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");
    const char *repo = json_get_str(fx, "repo", "");

    json_t *out = NULL;
    if (strcmp(op, "base_branches") == 0) {
        out = web_git_base_branch_list(repo);
    } else if (strcmp(op, "review_list_sorted") == 0) {
        out = web_git_review_list(repo, "uncommitted", NULL);
    } else {
        return 2;
    }
    char *s = json_dumps(out, 0);
    printf("%s\n", s);
    free(s);
    json_free(out);
    json_free(fx);
    return 0;
}
