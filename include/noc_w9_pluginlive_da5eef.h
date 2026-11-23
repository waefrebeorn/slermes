#ifndef NOC_W9_PLUGINLIVE_DA5EEF_H
#define NOC_W9_PLUGINLIVE_DA5EEF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* PoP: w9_pluginlive_da5eef_append @ agent/plugin_live_notes.py:_append
 * Append `notes` to message content: list -> [..., {"type":"text","text":notes}],
 * otherwise f"{content or ''}\n\n{notes}". Returns owned JSON (caller frees). */
struct json_t *w9_pluginlive_da5eef_append(const struct json_t *content,
                                            const char *notes);

/* PoP: w9_pluginlive_da5eef_queue @ agent/plugin_live_notes.py:queue_plugin_notice
 * Insertion-ordered dict with plugin_key replaced (others keep order, key moves last).
 * Returns owned JSON object (caller frees). */
struct json_t *w9_pluginlive_da5eef_queue(const struct json_t *notes,
                                            const char *plugin_key,
                                            const char *note);

/* PoP: w9_pluginlive_da5eef_pop @ agent/plugin_live_notes.py:pop_plugin_notices
 * "\n\n".join(notes.values()) in insertion order (redactor seam is identity).
 * Returns owned JSON string (caller frees). */
struct json_t *w9_pluginlive_da5eef_pop(const struct json_t *notes);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_PLUGINLIVE_DA5EEF_H */
