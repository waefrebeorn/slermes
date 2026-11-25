/* Harness for the ported `browser_vault action=list` result.
 *
 *   <base> [can_prompt]  -> the JSON upstream would print
 */
#include "browser_vault_list.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <vault dir> [can_prompt]\n", argv[0]); return 2; }
    int can_prompt = argc >= 3 ? atoi(argv[2]) : 1;
    json_t *o = browser_vault_list_json(argv[1], can_prompt);
    char *s = json_serialize(o);
    json_free(o);
    if (!s) { printf("{\"success\":false,\"error\":\"serialize failed\"}\n"); return 2; }
    fputs(s, stdout);
    free(s);
    return 0;
}
