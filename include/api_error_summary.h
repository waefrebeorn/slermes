#ifndef API_ERROR_SUMMARY_H
#define API_ERROR_SUMMARY_H

#include <stddef.h>

/*
 * api_error_summary.h — Port of Python run_agent.AIAgent._summarize_api_error
 * MIT License — WuBu Slermes Project
 *
 * Produces human-readable one-liners from API error strings. Handles Cloudflare
 * HTML pages, JSON body errors, and malformed streaming responses. (GAP 3 from
 * the original hermes_gap_fixes.c monolith — split into a self-contained module.)
 */

/*
 * Summarize an API error into a human-readable string.
 * Returns a malloc'd string the caller must free.
 */
char *summarize_api_error(const char *raw_error);

#endif /* API_ERROR_SUMMARY_H */
