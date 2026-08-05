/* port_turn_summary.h — C11 port of agent/turn_summary.py
 *
 * Per-turn accounting for the interactive CLI: a pure formatter
 * (format_turn_summary / format_elapsed / format_token_flow) plus a small
 * observer (TurnSummaryCollector) that tallies tool calls from the
 * tool-progress feed. No I/O, no agent-loop state.
 *
 * Faithful-port notes:
 *  - TurnTally.verbs is dict[str, dict[str, int]] (verb -> {plural_noun: count}),
 *    modelled as an insertion-ordered list of (verb, [(plural, count)...]).
 *  - record_tool accepts the tool result as a json_t* (NULL allowed). Diff
 *    extraction only inspects JSON already present in the result payload — it
 *    never shells out to git or re-reads files.
 */

#ifndef PORT_TURN_SUMMARY_H
#define PORT_TURN_SUMMARY_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TURN_SUMMARY_PREFIX        "\xE2\x8B\xAF" /* ⋯ */
#define TURN_SUMMARY_MIN_TOOLLESS 2.0
#define TURN_SUMMARY_MAX_SEGMENTS  4

/* One plural-noun -> count entry under a verb. */
typedef struct noun_entry {
    char *plural;
    int   count;
} noun_entry_t;

/* A verb with its (plural_noun -> count) tallies, insertion-ordered. */
typedef struct verb_entry {
    char         *verb;
    noun_entry_t *nouns;
    size_t        n_nouns;
    size_t        cap_nouns;
} verb_entry_t;

/* What a single turn did, observed from the tool-progress feed. */
typedef struct turn_tally {
    verb_entry_t *verbs;     /* insertion-ordered */
    size_t        n_verbs;
    size_t        cap_verbs;
    int           other_tools;
    int           lines_added;
    int           lines_removed;
    bool          has_line_deltas;
} turn_tally_t;

turn_tally_t *turn_tally_new(void);
void          turn_tally_free(turn_tally_t *t);
int           turn_tally_total_tools(const turn_tally_t *t);  /* total_tools */

/* --- pure formatters --- */
/* format_elapsed(seconds) -> "12.4s" / "2m05s". Caller frees. */
char *turn_summary_format_elapsed(double seconds);
/* _pluralize(count, plural) -> "1 file" / "3 files". Caller frees. */
char *turn_summary_pluralize(int count, const char *plural_noun);
/* format_turn_summary(elapsed, tally, max_segments) -> summary line or "". Caller frees. */
char *turn_summary_format(double elapsed_seconds, const turn_tally_t *tally,
                           int max_segments);
/* format_token_flow(count, arrow) -> "↓ 1.2k tok" or "". Caller frees. */
char *turn_summary_format_token_flow(const json_t *output_tokens, const char *arrow);
/* _count_diff_lines(diff) -> (added, removed). */
void  turn_summary_count_diff_lines(const char *diff, int *out_added, int *out_removed);

/* --- collector --- */
typedef struct turn_summary_collector turn_summary_collector_t;

turn_summary_collector_t *turn_summary_collector_new(void);
void   turn_summary_collector_free(turn_summary_collector_t *c);
void   turn_summary_collector_begin(turn_summary_collector_t *c);
/* record_tool(tool_name, result_json, is_error). result_json may be NULL. */
void   turn_summary_collector_record(turn_summary_collector_t *c,
                                     const char *tool_name,
                                     const json_t *result,
                                     bool is_error);
const turn_tally_t *turn_summary_collector_tally(const turn_summary_collector_t *c);
/* render(elapsed) -> summary line (see format_turn_summary). Caller frees. */
char *turn_summary_collector_render(const turn_summary_collector_t *c,
                                    double elapsed_seconds);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TURN_SUMMARY_H */
