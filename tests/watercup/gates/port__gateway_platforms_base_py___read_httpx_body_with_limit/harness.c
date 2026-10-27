/* gate: port__gateway_platforms_base_py___read_httpx_body_with_limit — traced copy of _read_httpx_body_with_limit (json_ret) */
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


extern json_t* _read_httpx_body_with_limit(const char* body_data, size_t body_len, const char* content_length_header);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _read_httpx_body_with_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_read_httpx_body_with_limit");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t* result = _read_httpx_body_with_limit(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_read_httpx_body_with_limit");
    }
    return 0;
}
