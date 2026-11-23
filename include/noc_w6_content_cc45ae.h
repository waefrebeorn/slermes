#ifndef NOC_W6_CONTENT_CC45AE_H
#define NOC_W6_CONTENT_CC45AE_H

#include "hermes_json.h"

/* Public C API for the pure content-block helpers of acp_adapter/content.py
 * (gate w6_content_cc45ae). acpcont_ prefix avoids collisions with the many
 * content_* symbols in this repo.
 *
 * Blocks/objects are represented as ordinary JSON objects whose keys are the
 * Python object's attributes; "__type" selects the acp.schema block class in
 * the oracle. Filesystem-backed resource_link reads are excluded (the oracle
 * uses non-file URIs, which take the deterministic "link only" branch). */
struct json_t *acpcont_resource_display_name(const char *uri,
                                             const struct json_t *name,
                                             const struct json_t *title);
struct json_t *acpcont_mime_main(const struct json_t *mime_type);
struct json_t *acpcont_is_text_resource(const struct json_t *mime_type);
struct json_t *acpcont_is_image_resource(const struct json_t *mime_type);
struct json_t *acpcont_format_resource_text(const char *uri, const char *body,
                                            const struct json_t *name,
                                            const struct json_t *title,
                                            const struct json_t *note);
struct json_t *acpcont_attr(const struct json_t *obj, const char *name);
struct json_t *acpcont_image_block_to_openai_part(const struct json_t *block);
struct json_t *acpcont_extract_text(const struct json_t *prompt);
struct json_t *acpcont_blocks_to_openai_user_content(const struct json_t *prompt);

#endif /* NOC_W6_CONTENT_CC45AE_H */
