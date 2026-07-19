/*
 * lsp_range_shift.h — public API for the pure agent/lsp/range_shift.py
 * helpers. Opaque, minimal includes (forward-declares json_t).
 */

#ifndef LSP_RANGE_SHIFT_H
#define LSP_RANGE_SHIFT_H

#include <stddef.h>

typedef struct json_t json_t;

typedef struct {
    int tag;        /* 0=equal,1=replace,2=delete,3=insert */
    int i1, i2, j1, j2;
} opcode_t;

typedef struct lsp_range_shift_t {
    int identity;     /* 1 => identity map (pre == post) */
    int pre_n, post_n;
    /* opcode table (used when !identity) */
    opcode_t *ops;
    int opn, opcap;
    char **pre_lines, **post_lines;
    int dummy_align;
} lsp_range_shift_t;

/* Build a shift map from pre/post text. Caller frees via lsp_free_line_shift.
 * (PoP: build_line_shift) */
lsp_range_shift_t *lsp_build_line_shift(const char *pre_text, const char *post_text);

/* Apply the map to a pre-edit 0-indexed line. Returns post line, or -1 if
 * the line was deleted by the edit. (PoP: build_line_shift closure) */
int lsp_line_shift(const lsp_range_shift_t *sh, int line);

/* Remap a diagnostic's range through the shift. Returns a malloc'd new JSON
 * string, or NULL if start maps to deleted. (PoP: shift_diagnostic_range) */
char *lsp_shift_diagnostic_range(const char *diag_json, const lsp_range_shift_t *sh);

/* Apply shift to every diagnostic in a baseline JSON array, dropping deleted
 * entries. Returns a malloc'd JSON array string. (PoP: shift_baseline) */
char *lsp_shift_baseline_json(const char *baseline_json, const lsp_range_shift_t *sh);

void lsp_free_line_shift(lsp_range_shift_t *sh);

#endif /* LSP_RANGE_SHIFT_H */
