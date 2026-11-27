/* gate: port__gateway_shutdown_forensics_py___proc_summary — traced copy of forensics_proc_summary (json_ret) */
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


extern json_node_t * forensics_proc_summary(pid_t pid);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s forensics_proc_summary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("forensics_proc_summary");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_node_t * result = forensics_proc_summary((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("forensics_proc_summary");
    }
    return 0;
}
