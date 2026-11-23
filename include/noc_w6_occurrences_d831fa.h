#ifndef NOC_W6_OCCURRENCES_D831FA_H
#define NOC_W6_OCCURRENCES_D831FA_H

/* Port of upstream Hermes cron/occurrences.py::scheduled_instant.
 *
 * Canonicalise an aware ISO-8601 instant to UTC and render Python's
 * datetime.isoformat() shape, e.g. "2024-01-15T05:00:00+00:00". Returns NULL
 * for non-strings, naive values and malformed/out-of-range dates (Python
 * returns None). Caller frees the returned string. */
char *w6_occurrences_d831fa_scheduled_instant(const char *value);

#endif /* NOC_W6_OCCURRENCES_D831FA_H */
