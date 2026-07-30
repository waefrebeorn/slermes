/*
 * t_port_tool_output_limits.c — oracle harness for the PURE tool-output-limit
 * helpers in src/cli/port_tools_tool_output_limits.c (ports of
 * tools/tool_output_limits.py: _coerce_positive_int, get_max_bytes,
 * get_max_lines, get_max_line_length). Deterministic.
 */

#include "tool_output_limits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void split_kv(const char *line, char *key, size_t ksz, const char **val) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < ksz) key[i++] = *line++;
    key[i] = '\0';
    if (*line == ' ') line++;
    *val = line;
}

int main(void) {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);

        if (strcmp(op, "coerce") == 0) {
            int v = 0, d = 0;
            sscanf(rest, "%d %d", &v, &d);
            int r = cli_tools_tool_output_limits__coerce_positive_int(v, d);
            printf("{\"op\":\"coerce\",\"in\":%d,\"default\":%d,\"out\":%d}\n", v, d, r);
        } else if (strcmp(op, "limits") == 0) {
            printf("{\"op\":\"limits\",\"max_bytes\":%d,\"max_lines\":%d,\"max_line_length\":%d}\n",
                   cli_tools_tool_output_limits_get_max_bytes(),
                   cli_tools_tool_output_limits_get_max_lines(),
                   cli_tools_tool_output_limits_get_max_line_length());
        } else {
            printf("{\"op\":\"unknown\",\"raw\":\"%s\"}\n", line);
        }
    }
    return 0;
}
