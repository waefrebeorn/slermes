/*
 * noc_w25_rangeshift_920f52.h - C surface for upstream
 * agent/lsp/range_shift.py (gate w25_rangeshift_920f52).
 *
 * The diff-aware line-shift map the cross-edit LSP delta filter uses: a
 * pre->post 0-indexed line map built from difflib's opcodes, the range remap
 * for one diagnostic, and the whole-baseline sweep.  Every LSP seam (the
 * document store, the publishDiagnostics socket, the editor) stays in the
 * caller; what is ported here is a pure function of (pre_text, post_text,
 * diagnostic values).
 */
#ifndef NOC_W25_RANGESHIFT_920F52_H
#define NOC_W25_RANGESHIFT_920F52_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* shift(pre_line) -> post_line | null for one 0-indexed line.
 * pre_text/post_text may be NULL (Python None -> no lines).
 * PoP: w25_rangeshift_920f52_shift_line @ agent/lsp/range_shift.py:build_line_shift */
json_t *w25_rangeshift_920f52_shift_line(const char *pre_text,
                                         const char *post_text, int line);

/* Copy of diag with its line range remapped; null when the start line was
 * deleted.  A non-object diag is null (upstream would raise).
 * PoP: w25_rangeshift_920f52_shift_diag @ agent/lsp/range_shift.py:shift_diagnostic_range */
json_t *w25_rangeshift_920f52_shift_diag(const char *pre_text,
                                         const char *post_text,
                                         const json_t *diag);

/* Apply the map to every object element of baseline, dropping deleted entries
 * and non-object entries.
 * PoP: w25_rangeshift_920f52_shift_baseline @ agent/lsp/range_shift.py:shift_baseline */
json_t *w25_rangeshift_920f52_shift_baseline(const char *pre_text,
                                             const char *post_text,
                                             const json_t *baseline);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W25_RANGESHIFT_920F52_H */
