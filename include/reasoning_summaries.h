#ifndef HERMES_REASONING_SUMMARIES_H
#define HERMES_REASONING_SUMMARIES_H

#include <stddef.h>

/* Port of Python agent/reasoning_summaries.py:separate_glued_reasoning_blocks.
 *
 * Return a malloc'd string: *delta*, prefixed with a paragraph break ("\n\n")
 * when it glues onto *previous* (a summary-part boundary the chat wire drops).
 *
 * A break is inserted when:
 *   - previous is non-empty and mid-line (does not end in whitespace), AND
 *   - delta opens a bold heading ("**"), AND
 *   - delta contains a CLOSED heading ("**" after the opening pair) — a
 *     token-streamed emphasis fragment that merely opens emphasis without
 *     closing is not a part boundary.
 *
 * Caller must free() the result. On NULL/empty delta returns strdup(delta)
 * (or "" if delta is NULL). */
char *rs_separate_glued_reasoning_blocks(const char *previous, const char *delta);

#endif /* HERMES_REASONING_SUMMARIES_H */
