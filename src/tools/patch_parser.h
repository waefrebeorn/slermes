#ifndef TOOLS_PATCH_PARSER_H
#define TOOLS_PATCH_PARSER_H

#include <stddef.h>

typedef enum { OP_ADD, OP_UPDATE, OP_DELETE, OP_MOVE } patch_op_type_t;

typedef struct {
    char prefix;        /* ' ', '-', '+' */
    char *content;
} patch_hunk_line_t;

typedef struct {
    char *context_hint; /* or NULL */
    patch_hunk_line_t *lines;
    size_t n_lines;
} patch_hunk_t;

typedef struct {
    patch_op_type_t op;
    char *file_path;
    char *new_path;     /* or NULL (move dst) */
    patch_hunk_t *hunks;
    size_t n_hunks;
} patch_op_t;

typedef struct {
    patch_op_t *ops;
    size_t n_ops;
    char *error;        /* or NULL */
} patch_parse_result_t;

/* PoP: parse_v4a_patch @ tools/patch_parser.py:parse_v4a_patch */
patch_parse_result_t patch_parser_parse_v4a(const char *patch_content);

/* Free a result returned by patch_parser_parse_v4a. */
void patch_parser_result_free(patch_parse_result_t *r);

/* Print result as canonical JSON (one line) for oracle comparison. */
void patch_parser_print_canonical(const patch_parse_result_t *r);

#endif
