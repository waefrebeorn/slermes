#ifndef NOC_W19_PATTERNS_A4E066_H
#define NOC_W19_PATTERNS_A4E066_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_patterns_a4e066_js_only @ plugins/security-guidance/patterns.py:_JS_ONLY
 * ``p.endswith(_JS_EXTS)``. */
int w19_patterns_a4e066_js_only(const char *path);

/* PoP: w19_patterns_a4e066_py_only @ plugins/security-guidance/patterns.py:_PY_ONLY
 * ``p.endswith(_PY_EXTS)``. */
int w19_patterns_a4e066_py_only(const char *path);

/* PoP: w19_patterns_a4e066_not_docs @ plugins/security-guidance/patterns.py:_NOT_DOCS
 * ``not p.endswith(_DOC_EXTS)`` (None is not a Python str, but the port keeps the
 * predicate total: a missing path is not a doc file). */
int w19_patterns_a4e066_not_docs(const char *path);

/* PoP: w19_patterns_a4e066_rule @ plugins/security-guidance/patterns.py:_rule
 * ``{"ruleName": name, "reminder": reminder, **triggers}``; only the given trigger keys
 * are present.  ``triggers`` is a JSON object (may be NULL for none). */
struct json_t *w19_patterns_a4e066_rule(const char *name, const char *reminder,
                                       const struct json_t *triggers);

/* PoP: w19_patterns_a4e066_count @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS */
int w19_patterns_a4e066_count(void);

/* PoP: w19_patterns_a4e066_names @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS
 * The rule names in table order. */
struct json_t *w19_patterns_a4e066_names(void);

/* PoP: w19_patterns_a4e066_pattern @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS
 * One table entry as wire data: ``ruleName``, ``reminder`` (exact UTF-8), ``regex`` and
 * ``substrings`` when the rule carries them, and each callable predicate (``path_filter``
 * / ``path_check``) as the vector of its answers over ``probe_paths``.  NULL when the
 * name is not in the table. */
struct json_t *w19_patterns_a4e066_pattern(const char *name, const struct json_t *probe_paths);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PATTERNS_A4E066_H */
