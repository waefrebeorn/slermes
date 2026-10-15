/* gate: port__hermes_cli_web_routers_profiles_py___merge_profile_tree — traced copy of ws_profiles_merge_profile_tree (json_param) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern void ws_profiles_merge_profile_tree(json_t *merged, json_t *projects,
                                    const char *profile, int preview_limit);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_profiles_merge_profile_tree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_profiles_merge_profile_tree");
    json_t *_j0 = json_parse(argv[i], NULL);
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = ws_profiles_merge_profile_tree(_j0, _j1, argv[2 + (i - 2 + 2) % 5], (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    if (_j1) json_free(_j1);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_profiles_merge_profile_tree");
    }
    return 0;
}
