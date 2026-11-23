#ifndef NOC_W9_CHATCATALOG_A2A9D5_H
#define NOC_W9_CHATCATALOG_A2A9D5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_chatcatalog_a2a9d5_model_id_is_generation @ hermes_cli/chat_catalog.py:model_id_is_generation
 * Faithful port of the _GENERATION_ID_RE search: case-insensitive, token
 * boundary is start/[/_.-] and end/[/_.-]. */
int w9_chatcatalog_a2a9d5_model_id_is_generation(const struct json_t *model_id);

/* PoP: w9_chatcatalog_a2a9d5_tag_list_is_generation @ hermes_cli/chat_catalog.py:_tag_list_is_generation
 * True only for a JSON array holding at least one _GENERATION_TYPES tag. */
int w9_chatcatalog_a2a9d5_tag_list_is_generation(const struct json_t *tags);

/* PoP: w9_chatcatalog_a2a9d5_output_modalities_are_generation @ hermes_cli/chat_catalog.py:_output_modalities_are_generation
 * Non-empty list whose lowercased kinds intersect image/video but not text/chat. */
int w9_chatcatalog_a2a9d5_output_modalities_are_generation(const struct json_t *outputs);

/* PoP: w9_chatcatalog_a2a9d5_catalog_item_is_generation @ hermes_cli/chat_catalog.py:catalog_item_is_generation
 * Fail-open decision over a catalog row's own id / capabilities / type / tags /
 * architecture.output_modalities / metadata.tags. Pure: no process state. */
int w9_chatcatalog_a2a9d5_catalog_item_is_generation(const struct json_t *item);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_CHATCATALOG_A2A9D5_H */
