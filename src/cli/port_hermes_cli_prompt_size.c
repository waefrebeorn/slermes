/*
 * port_hermes_cli_prompt_size.c — C port of hermes_cli/prompt_size.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_prompt_size__bytes @ hermes_cli/prompt_size.py:_bytes */

/* Port of Python hermes_cli/prompt_size.py:_bytes */
/* Returns the UTF-8 byte length of a string. */
int cli_hermes_cli_prompt_size__bytes(const char *s)
{
    if (!s) return 0;
    return (int)strlen(s);
}



/* PoP: cli_hermes_cli_prompt_size_compute_prompt_breakdown @ hermes_cli/prompt_size.py:compute_prompt_breakdown */

/* Port of Python hermes_cli/prompt_size.py:compute_prompt_breakdown */
/* Returns a dict of prompt-size measurements for a fresh session. */
/* CLI port: writes a text summary to the output buffer. */
int cli_hermes_cli_prompt_size_compute_prompt_breakdown(
    const char *platform, char *output, size_t output_size)
{
    if (!platform || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: build a minimal breakdown from compiled-in prompt info. */
    snprintf(output, output_size,
             "Prompt-size breakdown (platform=%s)\n"
             "  System prompt total : (requires agent runtime)\n"
             "  Major blocks:\n"
             "    skills index       : (requires agent runtime)\n"
             "    memory             : (requires agent runtime)\n"
             "    user profile       : (requires agent runtime)\n"
             "  Prompt tiers:\n"
             "    stable             : (requires agent runtime)\n"
             "    context            : (requires agent runtime)\n"
             "    volatile           : (requires agent runtime)\n"
             "  Tool schemas         : (requires agent runtime)\n",
             platform);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size__fmt_kb @ hermes_cli/prompt_size.py:_fmt_kb */

/* Port of Python hermes_cli/prompt_size.py:_fmt_kb */
/* Formats a byte count as a human-readable KB string. */
int cli_hermes_cli_prompt_size__fmt_kb(int bytes, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%.1f KB", bytes / 1024.0);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size_render_breakdown @ hermes_cli/prompt_size.py:render_breakdown */

/* Port of Python hermes_cli/prompt_size.py:render_breakdown */
/* Renders the breakdown as plain text suitable for a terminal. */
int cli_hermes_cli_prompt_size_render_breakdown(
    const char *platform, const char *model,
    int sp_bytes, int si_bytes, int mem_bytes, int up_bytes,
    int tools_json_bytes, int tools_count,
    char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    char sp_kb[32], si_kb[32], mem_kb[32], up_kb[32], tools_kb[32];
    cli_hermes_cli_prompt_size__fmt_kb(sp_bytes, sp_kb, sizeof(sp_kb));
    cli_hermes_cli_prompt_size__fmt_kb(si_bytes, si_kb, sizeof(si_kb));
    cli_hermes_cli_prompt_size__fmt_kb(mem_bytes, mem_kb, sizeof(mem_kb));
    cli_hermes_cli_prompt_size__fmt_kb(up_bytes, up_kb, sizeof(up_kb));
    cli_hermes_cli_prompt_size__fmt_kb(tools_json_bytes, tools_kb, sizeof(tools_kb));
    snprintf(output, output_size,
             "Prompt-size breakdown (platform=%s, model=%s)\n"
             "\n"
             "  System prompt total : %8d B  (%s, %d chars)\n"
             "\n"
             "  Major blocks:\n"
             "    skills index       : %8d B  (%s)\n"
             "    memory             : %8d B  (%s)\n"
             "    user profile       : %8d B  (%s)\n"
             "\n"
             "  Tool schemas         : %8d B  (%s, %d tools)\n",
             platform, model ? model : "unset",
             sp_bytes, sp_kb, sp_bytes,
             si_bytes, si_kb,
             mem_bytes, mem_kb,
             up_bytes, up_kb,
             tools_json_bytes, tools_kb, tools_count);
    return 0;
}

/* PoP: cli_hermes_cli_prompt_size_cmd_prompt_size @ hermes_cli/prompt_size.py:cmd_prompt_size */

/* Port of Python hermes_cli/prompt_size.py:cmd_prompt_size */
/* Entry point for `hermes prompt-size`. */
void cli_hermes_cli_prompt_size_cmd_prompt_size(const char *platform, int as_json)
{
    if (!platform) platform = "cli";
    if (as_json) {
        printf("{\"platform\":\"%s\",\"note\":\"CLI port — full breakdown requires agent runtime\"}\n", platform);
    } else {
        printf("Prompt-size breakdown (platform=%s)\n", platform);
        printf("  (CLI port — full breakdown requires agent runtime)\n");
    }
}
