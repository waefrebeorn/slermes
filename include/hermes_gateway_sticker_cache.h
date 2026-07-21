/**
 * @file hermes_gateway_sticker_cache.h
 * @brief Telegram sticker cache API (port of Python gateway/sticker_cache.py).
 */
#ifndef HERMES_GATEWAY_STICKER_CACHE_H
#define HERMES_GATEWAY_STICKER_CACHE_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Sticker Cache (Telegram)
 * ================================================================ */

/* Look up a cached sticker description by file_unique_id.
 * Returns json_node_t with {description, emoji, set_name, cached_at} or NULL.
 * Caller must json_free the result. */
json_node_t *sticker_cache_get(const char *file_unique_id);

/* Store a sticker description in the JSON cache file.
 * Returns true on success. */
bool sticker_cache_set(const char *file_unique_id,
                        const char *description,
                        const char *emoji,
                        const char *set_name);

/* Build warm-style injection text for a sticker description.
 * Returns malloc'd string (caller must free). */
char *sticker_build_injection(const char *description,
                               const char *emoji,
                               const char *set_name);

/* Build injection text for animated/video stickers we can't analyze.
 * Returns malloc'd string (caller must free). */
char *sticker_build_animated_injection(const char *emoji);

#endif /* HERMES_GATEWAY_STICKER_CACHE_H */