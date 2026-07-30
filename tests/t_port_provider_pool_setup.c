/*
 * t_port_provider_pool_setup.c — faithful verification harness for
 * config_setup_supports_same_provider_pool_setup() in
 * src/cli/config_setup.c (port of hermes_cli/setup.py:
 * _supports_same_provider_pool_setup).
 *
 * The fixture (argv[1]) is one provider name per line. For each non-empty
 * line we call the C function and emit one JSON line:
 *   {"provider":"<name>","supports":<true|false>}
 * The Python oracle (tests/sta_oracle_provider_pool_setup.py) recomputes
 * from the LIVE hermes_cli.auth.PROVIDER_REGISTRY with the exact canonical
 * logic; the runner diffs them.
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — defined in src/cli/config_setup.c. Avoid pulling
 * hermes.h (libdb chain). */
bool config_setup_supports_same_provider_pool_setup(const char *provider);

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <providers.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* trim trailing newline / CR / whitespace */
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' ||
                         line[n-1] == ' ' || line[n-1] == '\t'))
            line[--n] = '\0';
        if (n == 0) continue;

        bool sup = config_setup_supports_same_provider_pool_setup(line);
        printf("{\"provider\":\"%s\",\"supports\":%s}\n", line, sup ? "true" : "false");
    }
    fclose(f);
    return 0;
}
