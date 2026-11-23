#ifndef NOC_W15_PLUGINISOL_0CD954_H
#define NOC_W15_PLUGINISOL_0CD954_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure isolation policy helpers in
 * hermes_cli/plugin_isolation.py (gate w15_pluginisol_0cd954).
 *
 * `cfg_isolation` is config["plugins"]["isolation"] as a string (NULL => absent
 * or null). `host_env` is the value of HERMES_PLUGIN_HOST_PROCESS injected by
 * the fixture; the process environment is never mutated by the C side. */

/* PoP: w15_pluginisol_0cd954_isolation_mode @ hermes_cli/plugin_isolation.py:isolation_mode
 * Returns a borrowed static string: "in_process" or "host". */
const char *w15_pluginisol_0cd954_isolation_mode(const char *cfg_isolation,
                                                const char *host_env);

/* PoP: w15_pluginisol_0cd954_in_process_import_refusal @ hermes_cli/plugin_isolation.py:in_process_import_refusal
 * Returns a malloc'd reason string (caller frees) or NULL when allowed. */
char *w15_pluginisol_0cd954_in_process_import_refusal(const char *what,
                                                     const char *source,
                                                     const char *cfg_isolation,
                                                     const char *host_env);

/* PoP: w15_pluginisol_0cd954_host_launcher @ hermes_cli/plugin_isolation.py:host_launcher
 * `launcher` is config["plugins"]["host"]["launcher"] (string or array).
 * Returns a caller-owned JSON array of strings. */
struct json_t *w15_pluginisol_0cd954_host_launcher(struct json_t *launcher);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PLUGINISOL_0CD954_H */
