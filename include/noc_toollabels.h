#ifndef NOC_TOOLLABELS_H
#define NOC_TOOLLABELS_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: toollabels_app_title @ tools/tool_labels.py:app_title
 * Returns a malloc'd title. */
char *toollabels_app_title(const char *slug);

/* PoP: toollabels_action_phrase @ tools/tool_labels.py:_action_phrase
 * Returns a malloc'd, lowercased action phrase. */
char *toollabels_action_phrase(const char *tool, const char *app_slug);

/* PoP: toollabels_label_for_tool_name @ tools/tool_labels.py:label_for_tool_name
 * Returns ToolLabel.as_payload() as an object, or JSON null for a blank name.
 * verb/emoji/preview are injected results of the stubbed get_tool_verb,
 * registry.get_emoji and build_tool_preview dependencies. */
struct json_t *toollabels_label_for_tool_name(const char *name, const struct json_t *arguments,
                                              const char *verb, const char *emoji, const char *preview);

/* PoP: toollabels_bridge_labels @ tools/tool_labels.py:_bridge_labels
 * Returns a JSON array of ToolLabel payloads. */
struct json_t *toollabels_bridge_labels(const char *function_name, const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_TOOLLABELS_H */
