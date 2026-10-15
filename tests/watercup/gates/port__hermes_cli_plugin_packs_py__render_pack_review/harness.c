/* gate: port__hermes_cli_plugin_packs_py__render_pack_review — traced copy of render_pack_review (json_param) */
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


extern void render_pack_review(const json_t *pack, const json_t *resolved);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s render_pack_review <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("render_pack_review");
    json_t *_j0 = json_parse(argv[i], NULL);
    json_t *_j1 = json_parse(argv[2 + (i - 2 + 1) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    void result = render_pack_review(_j0, _j1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    if (_j1) json_free(_j1);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("render_pack_review");
    }
    return 0;
}
