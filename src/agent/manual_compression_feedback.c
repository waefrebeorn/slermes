/*
 * manual_compression_feedback.c — User-facing summaries for manual compression.
 *
 * Port of Python agent/manual_compression_feedback.py (120 lines).
 * Generates consistent feedback strings for /compress and similar commands.
 *
 * MIT License — WuBu Slermes Project
 */

#include "manual_compression_feedback.h"
#include <stdio.h>
#include <string.h>

/* Port of Python manual_compression_feedback.py:describe_compression_lock_skip(). */
const char *describe_compression_lock_skip(const char *lock_signal) {
    if (lock_signal && lock_signal[0]) {
        /* Use a static buffer since we can't allocate */
        static char buf[512];
        snprintf(buf, sizeof(buf),
                 "⏳ Compression already in progress for this session "
                 "(holder: %s). Please wait for it to finish.",
                 lock_signal);
        return buf;
    }
    return (
        "⏳ Compression skipped: could not acquire this session's "
        "compression lock. Another compression may still be running, or "
        "the lock check failed — try again shortly."
    );
}

/* Port of Python manual_compression_feedback.py:summarize_manual_compression(). */
void summarize_manual_compression(int before_count, int after_count,
                                  int before_tokens, int after_tokens,
                                  compression_feedback_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    int noop = (after_count == before_count);
    out->noop = noop;

    if (noop) {
        if (before_tokens < 0) before_tokens = 0;
        if (after_tokens < 0) after_tokens = 0;

        snprintf(out->headline, sizeof(out->headline),
                 "No changes from compression: %d messages", before_count);

        if (after_tokens == before_tokens) {
            snprintf(out->token_line, sizeof(out->token_line),
                     "Approx request size: ~%d tokens (unchanged)", before_tokens);
        } else {
            snprintf(out->token_line, sizeof(out->token_line),
                     "Approx request size: ~%d → ~%d tokens",
                     before_tokens, after_tokens);
        }
    } else {
        snprintf(out->headline, sizeof(out->headline),
                 "Compressed: %d → %d messages", before_count, after_count);

        snprintf(out->token_line, sizeof(out->token_line),
                 "Approx request size: ~%d → ~%d tokens",
                 before_tokens, after_tokens);
    }

    /* Note: fewer messages but more tokens is counterintuitive */
    if (!noop && after_count < before_count && after_tokens > before_tokens) {
        snprintf(out->note, sizeof(out->note),
                 "Note: fewer messages can still raise this estimate when "
                 "compression rewrites the transcript into denser summaries.");
    }
}
