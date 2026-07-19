/*
 * classify_items_helpers.h — public API for the pure cron/scripts/
 * classify_items.py helpers. Opaque, minimal includes (forward-declares json_t
 * so callers don't need libjson internals).
 */

#ifndef CLASSIFY_ITEMS_HELPERS_H
#define CLASSIFY_ITEMS_HELPERS_H

#include <stddef.h>

typedef struct json_t json_t;

typedef struct {
    int index;
    int score;
    char *reason;   /* malloc'd; freed by cron_classify_free_scores */
} classify_score_t;

/* Prefer id/guid/message_id/url/link; else "item-<index>". Caller frees.
 * (PoP: _item_id) */
char *cron_classify_item_id(const json_t *item, int index);

/* Build the classifier prompt from a json_t array + criteria. Caller frees.
 * (PoP: _build_prompt) */
char *cron_classify_build_prompt(const json_t *items, const char *criteria);

/* Parse a tolerant JSON array of {index, score, reason}. Returns a malloc'd
 * array (count in *out_count); caller frees via cron_classify_free_scores.
 * Empty/invalid -> NULL + count 0. (PoP: _parse_scores) */
classify_score_t *cron_classify_parse_scores(const char *content, int n_items, int *out_count);

void cron_classify_free_scores(classify_score_t *scores, int count);

#endif /* CLASSIFY_ITEMS_HELPERS_H */
