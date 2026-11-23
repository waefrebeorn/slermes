#ifndef NOC_W15_MAINDESKTO_7D5D41_H
#define NOC_W15_MAINDESKTO_7D5D41_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/main_desktop_tcc.py (gate w15_maindeskto_7d5d41).
 * All results are freshly allocated json_t values freed by the caller.
 * subprocess.success/text are explicit fixtures: `ok` models whether run()
 * succeeded, `stdout_text` is the captured stdout (NULL == None). */

/* PoP: w15_maindeskto_7d5d41_macos_codesigning_identity_valid @ hermes_cli/main_desktop_tcc.py:_macos_codesigning_identity_valid */
struct json_t *w15_maindeskto_7d5d41_macos_codesigning_identity_valid(const char *identity,
                                                                      const char *stdout_text,
                                                                      int ok);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_MAINDESKTO_7D5D41_H */
