/*
 * noc_w20_codeexecut_b10023.h -- public C API for the port of upstream Hermes
 *   tools/code_execution_lifecycle.py (gate w20_codeexecut_b10023).
 *
 * The ported part is the whole static judgement of the module: the AST-free
 * straight-line scanner behind _is_inert_python, the three name tables
 * (_INERT_IMPORTS, _SAFE_BUILTINS, _SINK_ATTRS + prefixes) and the exact
 * rejection text lifecycle_guard_rejection builds. The import of
 * cron.lifecycle_guard (the raw-text scan) stays upstream: its two predicates
 * are passed in by the caller.
 */
#ifndef NOC_W20_CODEEXECUT_B10023_H
#define NOC_W20_CODEEXECUT_B10023_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 when `code` is straight-line top-level code that cannot start or signal a
 * process (Python: _is_inert_python(code)); 0 for anything else, including
 * source that does not parse. */
int w20_codeexecut_b10023_is_inert_python(const char *code);

/* The rejection text, or NULL when the cell is allowed (Python:
 * lifecycle_guard_rejection(code)). `contains_command` / `contains_host_kill`
 * are the two cron.lifecycle_guard predicates, `matched` its
 * first_gateway_lifecycle_match (NULL / "" for no match) and `host_kill_text`
 * its HOST_INTERPRETER_KILL_REJECTION constant, returned verbatim on that
 * branch. Caller frees a non-NULL return with free(). */
char *w20_codeexecut_b10023_rejection(const char *code, int contains_command,
                                      int contains_host_kill, const char *matched,
                                      const char *host_kill_text);

/* Table membership: top-level module name in _INERT_IMPORTS. */
int w20_codeexecut_b10023_inert_import(const char *name);

/* Table membership: name in _SAFE_BUILTINS. */
int w20_codeexecut_b10023_safe_builtin(const char *name);

/* 1 when an attribute of this name reaches a process or dynamic code:
 * `attr in _SINK_ATTRS or attr in _INTROSPECTION_ATTRS or
 *  attr.startswith(_SINK_ATTR_PREFIXES + _INTROSPECTION_PREFIXES)`. */
int w20_codeexecut_b10023_sink_attr(const char *name);

/* _MAX_MATCH_CHARS (120). */
int w20_codeexecut_b10023_max_match_chars(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_CODEEXECUT_B10023_H */
