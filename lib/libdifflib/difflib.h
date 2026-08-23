/*
 * difflib.h — C diff library (J15: Python difflib port).
 *
 * Provides unified diff generation and sequence matching.
 */

#ifndef HERMES_DIFFLIB_H
#define HERMES_DIFFLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max lines in diff output */
#define DIFFLIB_MAX_LINES 4096

/** Max line length */
#define DIFFLIB_MAX_LINE 4096

/**
 * Compute similarity ratio between two strings (0.0 = completely different, 1.0 = identical).
 * Uses longest common subsequence heuristic.
 */
double difflib_ratio(const char *a, const char *b);

/**
 * Generate unified diff between two multi-line strings.
 * Returns malloc'd string. Caller must free().
 * lines: number of context lines (default 3, pass -1 for 3)
 */
char *difflib_unified_diff(const char *a, const char *b, int context_lines);

/**
 * Generate a simplified side-by-side diff (for terminal display).
 * Returns malloc'd string. Caller must free().
 */
char *difflib_simple_diff(const char *a, const char *b);

/* ── Python difflib parity surface (added 2026-08-22 lib audit) ── */

/* One opcode from SequenceMatcher.get_opcodes(): tag is one of
 * 'e' (equal), 'r' (replace), 'd' (delete), 'i' (insert); ranges are
 * [i1,i2) over a's lines and [j1,j2) over b's lines. */
typedef struct {
    char tag;
    int  i1, i2, j1, j2;
} difflib_opcode_t;

/* PoP: difflib.SequenceMatcher(None, a_lines, b_lines).get_opcodes().
 * a/b are newline-separated texts split into lines (no trailing empty
 * element). Fills up to out_max opcodes; returns count written.
 * Uses the same recursive longest-matching-block strategy as Python. */
int difflib_get_opcodes(const char *a, const char *b,
                        difflib_opcode_t *out, int out_max);

/* PoP: difflib.SequenceMatcher(None, a, b).ratio() — character-level
 * Ratcliff-Obershelp similarity (2.0*M / len(a)+len(b)), matching
 * Python exactly for ASCII inputs. Distinct from difflib_ratio(), whose
 * LCS-line metric predates this port and is kept for existing callers. */
double difflib_seq_ratio(const char *a, const char *b);

/* PoP: difflib.get_close_matches(word, possibilities, n=3, cutoff=0.6).
 * Writes up to n best matches with ratio >= cutoff into out (capacity
 * out_max), best first; ties broken by possibility order (Python sorts
 * by (-ratio, word), which for equal ratios preserves scan order).
 * Returns number written. Strings in `possibilities` are NOT modified. */
int difflib_get_close_matches(const char *word, const char **possibilities,
                              int num_possibilities, int n, double cutoff,
                              const char **out, int out_max);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DIFFLIB_H */
