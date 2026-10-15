/* gate: port__hermes_cli_web_routers_git_py__git_create_pr_route — traced copy of git_create_pr_route (json_ret) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);


extern json_t * git_create_pr_route(const char *path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s git_create_pr_route <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("git_create_pr_route");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t * result = git_create_pr_route(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("git_create_pr_route");
    }
    return 0;
}
