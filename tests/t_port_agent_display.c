/* Oracle harness: agent/display.summarize_shell_command vs LIVE Python.
 * Usage: ./t_port_agent_display <command_file>
 * Reads the command from the file (so the shared run_oracle.sh contract of
 * passing a fixture path works), prints the summarized shell command.
 * The oracle (sta_oracle_agent_display.py) prints the same from LIVE Python;
 * diff for fidelity. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/port_agent_display.h"

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0'; fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <command_file>\n", argv[0]); return 2; }
    char *cmd = read_file(argv[1]);
    if (!cmd) return 2;
    char *out = cli_agent_display__summarize_shell_command(cmd);
    printf("%s\n", out ? out : "");
    free(out); free(cmd);
    return 0;
}
