#ifndef NOC_W16_WEBSERVER_1A1654_H
#define NOC_W16_WEBSERVER_1A1654_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_webserver_1a1654_string_list @ hermes_cli/web_server_memory.py:_string_list */
struct json_t *w16_webserver_1a1654_string_list(const struct json_t *value);

/* PoP: w16_webserver_1a1654_schema_field_kind @ hermes_cli/web_server_memory.py:_schema_field_kind */
struct json_t *w16_webserver_1a1654_schema_field_kind(const struct json_t *raw,
                                                      const struct json_t *choices);

/* PoP: w16_webserver_1a1654_coerce_bool @ hermes_cli/web_server_memory.py:_coerce_bool
 * On ValueError returns {"error": "<python message>"}. */
struct json_t *w16_webserver_1a1654_coerce_bool(const struct json_t *value,
                                                const struct json_t *def);

/* PoP: w16_webserver_1a1654_normalize_schema @ hermes_cli/web_server_memory.py:_normalize_memory_provider_schema
 * Seam: raw_schema is the list get_config_schema() would return. */
struct json_t *w16_webserver_1a1654_normalize_schema(const struct json_t *raw_schema);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_WEBSERVER_1A1654_H */
