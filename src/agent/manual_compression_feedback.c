
/* PoP: describe_compression_lock_skip @ agent/manual_compression_feedback.py:describe_compression_lock_skip */
char *describe_compression_lock_skip(const char *lock_signal) {
    char *buf = malloc(256);
    if (!buf) return NULL;
    if (lock_signal && lock_signal[0]) {
        snprintf(buf, 256, "Compression already in progress for this session (holder: %s). Please wait for it to finish.", lock_signal);
    } else {
        snprintf(buf, 256, "Compression skipped: could not acquire this session's compression lock. Another compression may still be running, or the lock check failed - try again shortly.");
    }
    return buf;
}
op = (after_count == before_count);
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
                     "Approx request size: ~%d \xe2\x86\x92 ~%d tokens",
                     before_tokens, after_tokens);
        }
    } else {
        snprintf(out->headline, sizeof(out->headline),
                 "Compressed: %d \xe2\x86\x92 %d messages", before_count, after_count);

        snprintf(out->token_line, sizeof(out->token_line),
                 "Approx request size: ~%d \xe2\x86\x92 ~%d tokens",
                 before_tokens, after_tokens);
    }

    /* Note: fewer messages but more tokens is counterintuitive */
    if (!noop && after_count < before_count && after_tokens > before_tokens) {
        snprintf(out->note, sizeof(out->note),
                 "Note: fewer messages can still raise this estimate when "
                 "compression rewrites the transcript into denser summaries.");
    }
}
