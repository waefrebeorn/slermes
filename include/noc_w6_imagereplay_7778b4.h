#ifndef NOC_W6_IMAGEREPLAY_7778B4_H
#define NOC_W6_IMAGEREPLAY_7778B4_H

/*
 * Public C API for the port of agent/image_replay.py (gate w6_imagereplay_7778b4).
 * All exported symbols are imagereplay_-prefixed.
 */
#include "hermes_json.h"

/* resolve_replay_recent_images(cfg) -> JSON int. */
json_t *imagereplay_resolve_recent(const json_t *cfg);

/* replay_frontier(total, keep) -> JSON int. */
json_t *imagereplay_frontier(double total, double keep);

/* _split_rebuilt_row(content) -> JSON array of parts, or JSON null.
 * text part: ["text", str]; image part: ["image", ref, detail|null, marker_line]. */
json_t *imagereplay_split_row(const char *content);

/* replay_recent_images(messages, keep, urls) -> {"attached":int,"messages":[...]}.
 * `urls` maps a retained ref to its data URL (injected stand-in for
 * _retained_data_url, which touches the filesystem). */
json_t *imagereplay_replay_recent(const json_t *messages, double keep, const json_t *urls);

#endif /* NOC_W6_IMAGEREPLAY_7778B4_H */
