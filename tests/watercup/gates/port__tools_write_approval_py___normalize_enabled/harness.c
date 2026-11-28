/* gate: port__tools_write_approval_py___normalize_enabled — traced copy of cli_tools_write_approval__normalize_enabled (json_param) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
typedef json_t json_node_t;
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern int cli_tools_write_approval__normalize_enabled(const json_node_t *raw);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_write_approval__normalize_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_write_approval__normalize_enabled");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int result = cli_tools_write_approval__normalize_enabled(_j0);
    WCUP_VALINT(result);
    if (_j0) json_free(_j0);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_write_approval__normalize_enabled");
    }
    return 0;
}
