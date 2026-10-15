/* gate: port__hermes_cli_doctor_py___fail_and_issue — traced copy of doctor_fail_and_issue (json_ret) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);


extern json_t * doctor_fail_and_issue(const char *check_name, const char *message);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_fail_and_issue <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("doctor_fail_and_issue");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t * result = doctor_fail_and_issue(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("doctor_fail_and_issue");
    }
    return 0;
}
