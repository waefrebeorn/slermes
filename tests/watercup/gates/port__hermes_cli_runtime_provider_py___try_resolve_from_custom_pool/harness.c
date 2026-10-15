/* gate: port__hermes_cli_runtime_provider_py___try_resolve_from_custom_pool — traced copy of _try_resolve_from_custom_pool (json_ret) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);


extern json_t * _try_resolve_from_custom_pool(const char *base_url,
                                      const char *provider_label,
                                      const char *api_mode_override,
                                      const char *provider_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _try_resolve_from_custom_pool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_try_resolve_from_custom_pool");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    json_t * result = _try_resolve_from_custom_pool(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char *_js = result ? json_serialize(result) : NULL;
    WCUP_VALSTR(_js ? _js : "null");
    if (result) json_free(result);
    if (_js) free(_js);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_try_resolve_from_custom_pool");
    }
    return 0;
}
