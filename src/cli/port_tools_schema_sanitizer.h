#ifndef CLI_PORT_TOOLS_SCHEMA_SANITIZER_H
#define CLI_PORT_TOOLS_SCHEMA_SANITIZER_H

#include <stddef.h>
#include "hermes_json.h"   /* defines json_t */

/* PoP ports from tools/schema_sanitizer.py — see port_tools_schema_sanitizer.c.
 * String-returning variants return a malloc'd JSON string (caller frees).
 * _node variants operate on json_t* (caller frees the returned node). */

char *cli_tools_schema_sanitizer__sanitize_single_tool(const char *tool_json);
json_t *cli_tools_schema_sanitizer__sanitize_single_tool_node(json_t *tool_in);
char *cli_tools_schema_sanitizer__sanitize_tool_schemas(const char *tools_json);
char *cli_tools_schema_sanitizer__strip_ref_siblings(const char *json_input);
json_t *cli_tools_schema_sanitizer__strip_ref_siblings_node(json_t *node);
char *cli_tools_schema_sanitizer__strip_top_level_combinators(const char *json_input);
json_t *cli_tools_schema_sanitizer__strip_top_level_combinators_node(json_t *params);
char *cli_tools_schema_sanitizer_strip_nullable_unions(const char *json_input);
json_t *cli_tools_schema_sanitizer_strip_nullable_unions_node(json_t *node);
char *cli_tools_schema_sanitizer__strip_pattern_and_format(const char *tools_json, int *out_stripped);
char *cli_tools_schema_sanitizer__strip_slash_enum(const char *tools_json, int *out_stripped);

#endif
