/*
 * noc_w23_editcheck_cd6717.h - C surface for upstream agent/lsp/edit_check.py
 * (gate w23_editcheck_cd6717).
 *
 * The post-write LSP check never holds the write: it diagnoses the bytes the
 * edit wrote, waits for that verdict, and only when it carries diagnostics
 * gets the pre-edit verdict and reports the ones the edit introduced.  The
 * language server, the asyncio loop and the clock are seams the caller
 * supplies; this file owns the parts that decide what the caller is told:
 *
 *   - the pre->post line map over 0-indexed lines (difflib SequenceMatcher,
 *     autojunk=False, exactly Python's opcodes) and its application to a
 *     baseline, including the multi-line collapse at a deleted start;
 *   - the diagnostic identity key the delta filter compares;
 *   - the introduced-diagnostics filter itself;
 *   - the per-file verdict cache (sha256 of the disk-read text, 256 entries,
 *     oldest evicted) that lets the next edit reuse this one's verdict;
 *   - the inline wait budget.
 *
 * Nothing here spawns a server or sleeps.
 */
#ifndef NOC_W23_EDITCHECK_CD6717_H
#define NOC_W23_EDITCHECK_CD6717_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* edit_check.WRITE_BUDGET / INLINE_GRACE / _VERDICT_CAP. */
#define W23_EDITCHECK_CD6717_WRITE_BUDGET 0.08
#define W23_EDITCHECK_CD6717_INLINE_GRACE 0.08
#define W23_EDITCHECK_CD6717_VERDICT_CAP 256

/* {"write_budget", "inline_grace", "verdict_cap"}. */
json_t *w23_editcheck_cd6717_consts(void);

/* inline_wait(started): None -> INLINE_GRACE, else the part of WRITE_BUDGET
 * still left at `now` (time.monotonic()).  Never negative. */
double w23_editcheck_cd6717_inline_wait(const json_t *started, double now);

/* sha256 hex of as_disk_read(text): BOM dropped, CRLF/CR -> LF.  Caller frees. */
char *w23_editcheck_cd6717_digest(const char *text);

/* _diagnostic_key(d): severity \0 code \0 source \0 message.strip() \0
 * "line:char-line:char", with the module's `or` fallbacks.  Caller frees.
 * NULL when `diag` is not an object (Python raises AttributeError there). */
char *w23_editcheck_cd6717_diag_key(const json_t *diag);

/* shift_baseline(baseline, build_line_shift(pre_text, post_text)): every
 * diag's line range remapped pre->post, entries whose start line was deleted
 * dropped.  `pre`/`post` NULL => [] (Python's `if pre_text else []`). */
json_t *w23_editcheck_cd6717_shift_baseline(const json_t *baseline,
                                            const char *pre_text,
                                            const char *post_text);

/* The diagnostics this edit introduced: diags minus the identities the
 * line-shifted baseline still carries.  pre/post NULL => diags unchanged
 * (the filter only runs when both snapshots are known). */
json_t *w23_editcheck_cd6717_introduced(const json_t *diags,
                                        const json_t *baseline,
                                        const char *pre_text,
                                        const char *post_text);

/* Replay a list of cache operations in order and answer the per-op results:
 *   {"op":"remember","path":p,"text":t,"diags":[...]}  -> null
 *   {"op":"recall","path":p,"text":t}                  -> list|null
 * Returns {"results":[...],"size":<entries>}. */
json_t *w23_editcheck_cd6717_cache_ops(const json_t *ops);

#ifdef __cplusplus
}
#endif
#endif
