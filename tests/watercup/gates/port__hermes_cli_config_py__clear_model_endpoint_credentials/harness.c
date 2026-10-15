/* gate: port__hermes_cli_config_py__clear_model_endpoint_credentials — traced copy of config_clear_model_endpoint_credentials (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void config_clear_model_endpoint_credentials(const char *model_cfg_json,
                                              int clear_api_key, int clear_api_mode,
                                              int clear_base_url,
                                              char *out_json, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_clear_model_endpoint_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("config_clear_model_endpoint_credentials");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    config_clear_model_endpoint_credentials();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("config_clear_model_endpoint_credentials");
    }
    return 0;
}
