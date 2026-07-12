#ifndef CLI_PORT_TOOLS_SCHEMA_SANITIZER_H
#define CLI_PORT_TOOLS_SCHEMA_SANITIZER_H

#include <stddef.h>

/* PoP ports from tools/schema_sanitizer.py — see port_tools_schema_sanitizer.c.
 * Each returns a malloc'd JSON string (caller frees). */

char *cli_tools_schema_sanitizer__sanitize_single_tool(const char *tool_json);
char *cli_tools_schema_sanitizer__strip_ref_siblings(const char *json_input);
char *cli_tools_schema_sanitizer__strip_top_level_combinators(const char *json_input);
char *cli_tools_schema_sanitizer_strip_nullable_unions(const char *json_input);
char *cli_tools_schema_sanitizer__strip_pattern_and_format(const char *tools_json, int *out_stripped);
char *cli_tools_schema_sanitizer__strip_slash_enum(const char *tools_json, int *out_stripped);

#endif
