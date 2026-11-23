#ifndef NOC_W12_MAINDEPHI_E98FD2_H
#define NOC_W12_MAINDEPHI_E98FD2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_maindephi_e98fd2_missing_optional_deps_message @ hermes_cli/main_dep_hints.py:missing_optional_deps_message
 * Returns the exact repair guidance string; caller frees. `extra` is accepted because the
 * live Python function expects it (it is unused in that function's body). */
char *w12_maindephi_e98fd2_missing_optional_deps_message(const char *surface,
                                                          const char *what,
                                                          const char *extra);

/* PoP: w12_maindephi_e98fd2_smart_app_control_block_message @ hermes_cli/main_dep_hints.py:smart_app_control_block_message
 * Returns the exact guidance string when str(error) contains both "DLL load failed" and
 * "_ssl", otherwise NULL (Python None). Caller frees. */
char *w12_maindephi_e98fd2_smart_app_control_block_message(const struct json_t *error);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_MAINDEPHI_E98FD2_H */
