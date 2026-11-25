/*
 * noc_w34_coverage_56490e.h - C surface for upstream
 * persistence/sqlite/conversation/_store/coverage.py (gate w34_coverage_56490e).
 *
 * The pure core of carried-row coverage for compaction: the text a message
 * carries, the whole-line containment predicate the compressor's rewrite
 * argument rests on, the image-placeholder rewrite predicate, and the three
 * span computations the commit's positional rewind is sized by.
 *
 * Seams (passed in, never touched here): the state.db connection and every
 * statement it runs, the live StoreHolder, and the writer's own message
 * serializer plus the stored-row digest those claims compare through.
 */
#ifndef NOC_W34_COVERAGE_56490E_H
#define NOC_W34_COVERAGE_56490E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {content: <any>} -> the text a message carries: a string as-is, the
 * text/input_text/output_text parts of a list joined by "\n" (each through
 * Python str(), falsey text carrying nothing); "" for anything else. */
json_t *w34_coverage_56490e_text_of(const json_t *in);

/* in: {inner: str, outer: str} -> bool. True when inner sits in outer as whole
 * lines (blank runs beside the hit count as line edges). Byte-wise substring
 * search; line-edge blanks are the str.isspace() set, UTF-8 decoded. */
json_t *w34_coverage_56490e_holds_as_block(const json_t *in);

/* in: {stored: <any>, held: <any>} -> bool. True when the two lists have equal
 * length and held is stored with at least one image part (image_url /
 * input_image / image) swapped for a text part, every other part Python-equal. */
json_t *w34_coverage_56490e_images_replaced(const json_t *in);

/* in: {carried: {"<row id>": <dict index>, ...}, compacted_count: int,
 *      tail_count: int} -> int. How many carried dicts name a row of the
 * carried tail, i.e. a dict index at or above compacted_count - tail_count. */
json_t *w34_coverage_56490e_carried_tail_slots(const json_t *in);

/* in: {tail: [<message dict>, ...]} -> int. Durable rows behind the carried
 * tail beyond one per dict: the _merged_durable_rows stamp less one and less
 * the _absorbed_row_ids it lists (floored at 0), plus _unnamed_durable_rows.
 * A dict carrying an integer _row_id was written back as a single row: no add. */
json_t *w34_coverage_56490e_uncounted_merged_rows(const json_t *in);

/* in: {covered_active: [<row id>, ...], tail_count: int,
 *      merged_away: [<row id>, ...]} -> the newest covered ids behind
 * tail_count carried dicts, widened while the selected window holds rows a
 * merge already stands for. covered_active[-width:] slice semantics, including
 * Python's whole-list window at width 0. */
json_t *w34_coverage_56490e_tail_originals(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
