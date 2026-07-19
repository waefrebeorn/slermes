/*
 * cron_scheduler_helpers.h — public API for the pure cron scheduler string
 * helpers ported from cron/scheduler.py (see src/cron/port_cron_scheduler_helpers.c).
 *
 * These are classification/normalization helpers that do NOT touch the file
 * lock, subprocess, asyncio loop, or delivery adapters. Opaque, minimal-includes.
 */

#ifndef CRON_SCHEDULER_HELPERS_H
#define CRON_SCHEDULER_HELPERS_H

#include <stddef.h>

/* Detect [SILENT]/NO_REPLY/NO REPLY sentinels (bracketed prefix, whole
 * response, or first/last non-empty line). Returns 1 if delivery suppressed. */
int scheduler_is_cron_silence_response(const char *text);

/* None/"" -> "local"; any non-empty scalar -> its own strdup. Caller frees. */
char *scheduler_normalize_deliver_value(const char *deliver);

/* Comma-join a list of parts (NULL/whitespace-only entries skipped).
 * Empty result -> "local". Caller frees. */
char *scheduler_normalize_deliver_value_list(const char **parts, int n);

/* Compact one-line cron failure message for chat delivery. job_name may be
 * NULL (falls back to "cron job"); error may be NULL. Caller frees. */
char *scheduler_summarize_cron_failure(const char *job_name, const char *error);

#endif /* CRON_SCHEDULER_HELPERS_H */
