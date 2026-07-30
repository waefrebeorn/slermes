/*
 * manual_compression_feedback.h — User-facing summaries for manual compression.
 *
 * Port of Python agent/manual_compression_feedback.py (49 lines).
 * Generates consistent feedback strings for /compress and similar commands.
 */

#ifndef MANUAL_COMPRESSION_FEEDBACK_H
#define MANUAL_COMPRESSION_FEEDBACK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Output structure for a compression summary. */
typedef struct {
    bool noop;              /**< True if no messages changed */
    char headline[256];     /**< One-line summary (e.g. "Compressed: 42 -> 27 messages") */
    char token_line[256];   /**< Token count info (e.g. "Approx request size: ~15,432 tokens") */
    char note[512];         /**< Optional note about counterintuitive results (empty if none) */
} compression_feedback_t;
/**
 * describe_compression_lock_skip — Return user-facing text when manual
 * compression is skipped because the compression lock is held.
 *
 * @param lock_signal  Holder string when confirmed, or NULL/true when
 *                     acquisition failed without a confirmed holder.
 * @return             Static string. Caller must not free.
 */
const char *describe_compression_lock_skip(const char *lock_signal);

/**
 * summarize_manual_compression — Generate user-facing feedback for a manual
 * compression operation.
 *
 * @param before_count   Number of messages before compression
 * @param after_count    Number of messages after compression
 * @param before_tokens  Estimated token count before compression
 * @param after_tokens   Estimated token count after compression
 * @param out            Output structure (caller provides storage)
 */
void summarize_manual_compression(int before_count, int after_count,
                                  int before_tokens, int after_tokens,
                                  compression_feedback_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MANUAL_COMPRESSION_FEEDBACK_H */
