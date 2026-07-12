/* Oracle harness: agent/display build_tool_preview / build_tool_label /
 * redact_tool_args_for_display vs LIVE Python.
 * Usage (shared runner contract): ./t_port_display_tool_preview <args.json> <mode> <tool> [max_len]
 *   mode: preview | label | redact
 * Prints the resulting string; the oracle emits the same from LIVE Python. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "cli/port_display_tool_preview.h"

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
    if (argc < 4) { fprintf(stderr, "usage: %s <args.json> <preview|label|redact> <tool> [max_len]\n", argv[0]); return 2; }
    char *args = read_file(argv[1]);
    if (!args) return 2;
    const char *mode = argv[2];
    const char *tool = argv[3];
    int max_len = (argc > 4) ? atoi(argv[4]) : 0;
    char *out = NULL;
    if (strcmp(mode, "preview") == 0)
        out = cli_agent_display__build_tool_preview(tool, args, max_len);
    else if (strcmp(mode, "label") == 0)
        out = cli_agent_display__build_tool_label(tool, args, max_len, 1);
    else if (strcmp(mode, "redact") == 0)
        out = cli_agent_display__redact_tool_args_for_display(tool, args);
    else { fprintf(stderr, "unknown mode %s\n", mode); free(args); return 2; }
    printf("%s\n", out ? out : "");
    free(out); free(args);
    return 0;
}
