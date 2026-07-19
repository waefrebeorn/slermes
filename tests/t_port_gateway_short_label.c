/*
 * t_port_gateway_short_label.c — faithful verification harness for
 * setup_gateway_platform_short_label in src/cli/config_setup.c
 * (port of hermes_cli/setup.py:_gateway_platform_short_label).
 *
 * Reads a label from each line of argv[1] (trailing newline stripped) and
 * emits one line per input: the short label. The Python oracle
 * (tests/sta_oracle_gateway_short_label.py) recomputes from the LIVE
 * hermes_cli/setup.py; the runner diffs them.
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — defined in src/cli/config_setup.c (port of
 * hermes_cli/setup.py). Avoid pulling heavy setup headers. */
char *setup_gateway_platform_short_label(const char *label);

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <labels.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, f)) != -1) {
        /* strip trailing \n and \r */
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        char *res = setup_gateway_platform_short_label(line);
        printf("%s\n", res ? res : "");
        free(res);
    }
    free(line);
    fclose(f);
    return 0;
}
