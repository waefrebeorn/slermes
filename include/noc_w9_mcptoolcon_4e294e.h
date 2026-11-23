#ifndef NOC_W9_MCPTOOLCON_4E294E_H
#define NOC_W9_MCPTOOLCON_4E294E_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w9_mcptoolcon_4e294e_is_reserved_meta_key @ tools/mcp_tool_content.py:_is_reserved_mcp_meta_key */
int w9_mcptoolcon_4e294e_is_reserved_meta_key(const char *key);
/* PoP: w9_mcptoolcon_4e294e_strip_reserved_meta @ tools/mcp_tool_content.py:_strip_reserved_meta_keys */
struct json_t *w9_mcptoolcon_4e294e_strip_reserved_meta(const struct json_t *meta);
/* PoP: w9_mcptoolcon_4e294e_base_mime @ tools/mcp_tool_content.py:_base_mime */
char *w9_mcptoolcon_4e294e_base_mime(const struct json_t *mime);
/* PoP: w9_mcptoolcon_4e294e_image_extension @ tools/mcp_tool_content.py:_mcp_image_extension_for_mime_type */
char *w9_mcptoolcon_4e294e_image_extension(const struct json_t *mime, const char *ext_override);
/* PoP: w9_mcptoolcon_4e294e_resource_filename @ tools/mcp_tool_content.py:_mcp_resource_filename */
char *w9_mcptoolcon_4e294e_resource_filename(const char *uri, const struct json_t *mime, const char *fallback_ext);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_MCPTOOLCON_4E294E_H */
