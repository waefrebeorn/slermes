#ifndef PORT_TURN_SUMMARY_H
#define PORT_TURN_SUMMARY_H

#include <stddef.h>

/* Port of Python: agent/turn_summary.py — per-turn accounting for the CLI.
 * Display-only and pure: a tally struct plus the formatters.
 */

#define TS_MAX_VERBS 32
#define TS_MAX_NOUNS 4

typedef struct {
    char *verb;                 /* "edited" / "read" / ...            */
    char *nouns[TS_MAX_NOUNS];  /* plural noun forms, first-seen order */
    int   counts[TS_MAX_NOUNS];
    int   n_nouns;
} ts_verb_group_t;

typedef struct {
    ts_verb_group_t verbs[TS_MAX_VERBS];
    int   n_verbs;
    int   other_tools;
    int   lines_added;
    int   lines_removed;
    int   has_line_deltas;
} ts_tally_t;

void ts_tally_init(ts_tally_t *tally);
void ts_tally_clear(ts_tally_t *tally);
int  ts_tally_total_tools(const ts_tally_t *tally);

/* Record one completed tool call. Failed calls and internal "_"-prefixed
 * tools are skipped, as the Python collector does. `result_json` is the raw
 * tool result when the caller has it (NULL otherwise). */
void ts_tally_record(ts_tally_t *tally, const char *tool_name,
                     const char *result_json, int is_error);

/* (added, removed) from a patch tool result, or 0 when the result carries no
 * countable diff. `result` may be a JSON object or its JSON text. */
int ts_extract_line_deltas(const char *tool_name, const char *result,
                           int *added, int *removed);

/* "12.4s" / "2m05s" — malloc'd. */
char *ts_format_elapsed(double seconds);

/* "1 file" / "3 files" — malloc'd. */
char *ts_pluralize(int count, const char *plural_noun);

/* "↓ 1.2k tok" — malloc'd; empty string for a non-positive count. */
char *ts_format_token_flow(long output_tokens, const char *arrow);

/* The rendered line, or "" when the turn had nothing worth saying. */
char *ts_format_summary(double elapsed_seconds, const ts_tally_t *tally,
                        int max_segments);

#endif /* PORT_TURN_SUMMARY_H */
