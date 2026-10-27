/* gate: port__cli_py__CanonicalUsage — traced copy of cli_canonical_usage_new (json_param) */
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


extern cli_canonical_usage_t * cli_canonical_usage_new(long long input_tokens,
                                               long long output_tokens,
                                               long long cache_read_tokens,
                                               long long cache_write_tokens,
                                               long long reasoning_tokens,
                                               int request_count,
                                               json_t *raw_usage);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_canonical_usage_new <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_canonical_usage_new");
    json_t *_j6 = json_parse(argv[2 + (i - 2 + 6) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_canonical_usage_t * result = cli_canonical_usage_new((long)strtol(argv[i], NULL, 10), (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), _j6);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j6) json_free(_j6);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_canonical_usage_new");
    }
    return 0;
}
