/* gate: port__hermes_cli_auth_py___assert_nous_inference_jwt_usable — traced copy of auth_assert_nous_inference_jwt_usable (json_param) */
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


extern auth_error_t * auth_assert_nous_inference_jwt_usable(const json_t *state, const char *access_token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_assert_nous_inference_jwt_usable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_assert_nous_inference_jwt_usable");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    auth_error_t * result = auth_assert_nous_inference_jwt_usable(_j0, argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_assert_nous_inference_jwt_usable");
    }
    return 0;
}
