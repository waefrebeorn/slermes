/*
 * session_listing_helpers.h — public API for the pure hermes_cli/
 * session_listing.py helpers. Opaque, minimal includes (forward-declares json_t).
 */

#ifndef SESSION_LISTING_HELPERS_H
#define SESSION_LISTING_HELPERS_H

#include <stddef.h>

typedef struct json_t json_t;

/* Minimal shlex.split: returns malloc'd NULL-terminated argv + count.
 * (PoP: parse_session_listing_args tokenizer) */
char **session_listing_parse_args(const char *raw, int *out_count);
void session_listing_free_argv(char **argv, int count);

/* Parse `/sessions`-style args into flags + resume target.
 * (PoP: parse_session_listing_args) */
void session_listing_parse_flags(const char *raw_args,
                               int *include_all, int *include_unnamed,
                               char *target, size_t target_cap);

/* Render a compact Markdown-ish session list from a JSON array of rows.
 * Returns malloc'd string; caller frees. (PoP: format_gateway_session_listing) */
char *session_listing_format_gateway(const char *rows_json,
                                    int include_source,
                                    const char *title);

#endif /* SESSION_LISTING_HELPERS_H */
