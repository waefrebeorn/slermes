/* gate: _parse_status(porcelain) -> (branch, counts).
 * Canonical form: fixed key order with an explicit "absent" marker, because python omits
 * keys it did not see and the C json object cannot be compared field-by-field otherwise. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
char *coding_context_parse_git_status(const char *porcelain);
static void put(const json_t *o, const char *k, char *out, size_t cap, int *first) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    char val[256];
    if (v && v->type == JSON_STRING) snprintf(val, sizeof val, "%s", v->str_val ? v->str_val : "");
    else if (v && v->type == JSON_NUMBER) snprintf(val, sizeof val, "%lld", (long long)v->num_val);
    else snprintf(val, sizeof val, "absent");
    size_t used = strlen(out);
    snprintf(out + used, cap - used, "%s%s=%s", *first ? "" : "&", k, val);
    *first = 0;
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char canon[2048] = ""; int first = 1;
        WCUP_BEGIN("_parse_status");
        char *js = coding_context_parse_git_status(argv[i]);
        json_t *root = js ? json_parse(js, NULL) : NULL;
        const json_t *b = root ? json_obj_get(root, "branch") : NULL;
        const json_t *c = root ? json_obj_get(root, "counts") : NULL;
        put(b, "head", canon, sizeof canon, &first);
        put(b, "upstream", canon, sizeof canon, &first);
        put(b, "ahead", canon, sizeof canon, &first);
        put(b, "behind", canon, sizeof canon, &first);
        put(c, "staged", canon, sizeof canon, &first);
        put(c, "modified", canon, sizeof canon, &first);
        put(c, "untracked", canon, sizeof canon, &first);
        put(c, "conflicts", canon, sizeof canon, &first);
        WCUP_VALSTR(canon);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_parse_status");
        if (root) json_free(root);
        if (js) free(js);
    }
    return 0;
}
