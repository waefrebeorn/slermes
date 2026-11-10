/*
 * patch_parser_util.h - patch-parser marker / emit helpers.
 *
 * Module-internal header (port_patch_parser.c <-> patch_parser_util.c).
 */
#ifndef PATCH_PARSER_UTIL_H
#define PATCH_PARSER_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void emit_json_str(const char *s);
char * match_file_marker(const char *line, const char *opkw);
bool match_move_marker(const char *line, char **src, char **dst);
char * xstrndup(const char *s, size_t n);

#endif /* PATCH_PARSER_UTIL_H */
