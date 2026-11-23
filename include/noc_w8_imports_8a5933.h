#ifndef NOC_W8_IMPORTS_8A5933_H
#define NOC_W8_IMPORTS_8A5933_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_imports_8a5933_import_text_or_none @ session_rules/imports.py:_import_text_or_none
 * Tagged result: {"ok":true,"value":<value>} or {"ok":false,"error":"<msg>"}.
 * Caller frees. */
struct json_t *w8_imports_8a5933_text_or_none(const struct json_t *value, const char *field);

/* PoP: w8_imports_8a5933_import_int_or_none @ session_rules/imports.py:_import_int_or_none
 * Tagged result. Integer values are returned as DECIMAL STRINGS (int64 exactness).
 * Caller frees. */
struct json_t *w8_imports_8a5933_int_or_none(const struct json_t *value, const char *field);

/* PoP: w8_imports_8a5933_non_finite_path @ session_rules/imports.py:_import_non_finite_path
 * Returns a malloc'd first non-finite path, or NULL. JSON strings equal to
 * "NaN"/"Infinity"/"-Infinity" are treated as non-finite float markers. */
char *w8_imports_8a5933_non_finite_path(const struct json_t *data, const char *path);

/* PoP: w8_imports_8a5933_bounds @ session_rules/imports.py:IMPORT_INT_MIN/MAX
 * {"min":"-9223372036854775808","max":"9223372036854775807"}. Caller frees. */
struct json_t *w8_imports_8a5933_bounds(void);

/* PoP: w8_imports_8a5933_field_tables @ session_rules/imports.py:IMPORT_*_FIELDS
 * Object of the five import field tables. Caller frees. */
struct json_t *w8_imports_8a5933_field_tables(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_IMPORTS_8A5933_H */