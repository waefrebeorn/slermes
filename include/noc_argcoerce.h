#ifndef NOC_ARGCOERCE_H
#define NOC_ARGCOERCE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: argcoerce_coerce_number @ tools/arg_coercion.py:_coerce_number
 * Returns int/float as a JSON number, or the original string when unparseable. */
struct json_t *argcoerce_coerce_number(const char *value, int integer_only);

/* PoP: argcoerce_coerce_boolean @ tools/arg_coercion.py:_coerce_boolean
 * Returns a JSON bool for "true"/"false" (case-insensitive, stripped), else the string. */
struct json_t *argcoerce_coerce_boolean(const char *value);

/* PoP: argcoerce_schema_allows_null @ tools/arg_coercion.py:_schema_allows_null */
int argcoerce_schema_allows_null(const struct json_t *schema);

/* PoP: argcoerce_schema_accepts_kind @ tools/arg_coercion.py:_schema_accepts_kind
 * schema/expected fragments are passed directly; $ref resolution is identity
 * (fixtures are ref-free), matching the injected resolve_local_schema_ref. */
int argcoerce_schema_accepts_kind(const struct json_t *schema, const char *kind,
                                  const struct json_t *root_schema);

/* PoP: argcoerce_coerce_value @ tools/arg_coercion.py:_coerce_value */
struct json_t *argcoerce_coerce_value(const char *value, const struct json_t *expected_type,
                                      const struct json_t *schema);

/* PoP: argcoerce_normalize_json_strings @ tools/arg_coercion.py:_normalize_json_strings_for_schema */
struct json_t *argcoerce_normalize_json_strings(const struct json_t *value,
                                                const struct json_t *schema,
                                                const struct json_t *root_schema);

#ifdef __cplusplus
}
#endif
#endif /* NOC_ARGCOERCE_H */
