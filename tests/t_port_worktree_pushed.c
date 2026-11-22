/* t_port_worktree_pushed.c — harness for cli_gaps.c worktree_branch_pushed_exact.
 * Reads {"branch":<str>,"local_head":<str>,"remote_heads":<obj>}, emits input+out.
 * Gate: run_one_oracle.sh worktree_pushed oracle_worktree_pushed.py ""
 *       "src/cli_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int worktree_branch_pushed_exact(const char *branch, const char *local_head,
                                 const json_t *remote_heads);

static const char *js(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static void em(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        int out = worktree_branch_pushed_exact(js(in, "branch"), js(in, "local_head"),
                                               json_obj_get(in, "remote_heads"));
        printf("{\"branch\":"); em(in, "branch");
        printf(",\"local_head\":"); em(in, "local_head");
        printf(",\"remote_heads\":"); em(in, "remote_heads");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
