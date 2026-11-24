#ifndef NOC_W19_PLUGINISOL_F4E6F5_H
#define NOC_W19_PLUGINISOL_F4E6F5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_pluginisol_f4e6f5_hermes_module @ hermes_cli/plugin_isolation_audit.py:_SourceVisitor._hermes
 * True when the import's top-level package is Hermes process state. */
int w19_pluginisol_f4e6f5_hermes_module(const char *module);

/* PoP: w19_pluginisol_f4e6f5_dedupe @ hermes_cli/plugin_isolation_audit.py:_dedupe
 * Order-preserving de-duplication (list(dict.fromkeys(items))). */
json_t *w19_pluginisol_f4e6f5_dedupe(json_t *items);

/* PoP: w19_pluginisol_f4e6f5_python_files @ hermes_cli/plugin_isolation_audit.py:_python_files
 * Filters a sorted list of plugin-relative paths by the skip-directory and test-file rules. */
json_t *w19_pluginisol_f4e6f5_python_files(json_t *paths);

/* PoP: w19_pluginisol_f4e6f5_ctx_method @ hermes_cli/plugin_isolation_audit.py:_SourceVisitor.visit_Call
 * {"reason":str|null,"note":str|null,"registers":bool} for one ctx method call at `rel`:`line`. */
json_t *w19_pluginisol_f4e6f5_ctx_method(const char *name, const char *rel, long long line);

/* PoP: w19_pluginisol_f4e6f5_hook_note @ hermes_cli/plugin_isolation_audit.py:_SourceVisitor.visit_Call
 * The degraded-hook note, or NULL when the hook keeps its live payload. */
char *w19_pluginisol_f4e6f5_hook_note(const char *hook);

/* PoP: w19_pluginisol_f4e6f5_registry_reason @ hermes_cli/plugin_isolation_audit.py:_flag_registry_call
 * The direct-registry-call reason, or NULL when the call is allowed. */
char *w19_pluginisol_f4e6f5_registry_reason(const char *target, int model_provider,
                                            const char *rel, long long line);

/* PoP: w19_pluginisol_f4e6f5_patch_reason @ hermes_cli/plugin_isolation_audit.py:_flag_write */
char *w19_pluginisol_f4e6f5_patch_reason(const char *rel, long long line,
                                         const char *owner, const char *attr);

/* PoP: w19_pluginisol_f4e6f5_setattr_reason @ hermes_cli/plugin_isolation_audit.py:visit_Call
 * The `setattr()` reason for a resolved Hermes module owner. */
char *w19_pluginisol_f4e6f5_setattr_reason(const char *rel, long long line, const char *owner);

/* PoP: w19_pluginisol_f4e6f5_manifest_kind_reason @ hermes_cli/plugin_isolation_audit.py:_MANIFEST_KIND_REASONS
 * The manifest-kind reason after the str(kind or "").strip().lower() coercion, or NULL. */
char *w19_pluginisol_f4e6f5_manifest_kind_reason(const char *kind);

/* PoP: w19_pluginisol_f4e6f5_streaming_marker @ hermes_cli/plugin_isolation_audit.py:_STREAMING_MARKERS
 * The first streaming marker the dashboard API source carries, or NULL. */
const char *w19_pluginisol_f4e6f5_streaming_marker(const char *source);

/* PoP: w19_pluginisol_f4e6f5_report @ hermes_cli/plugin_isolation_audit.py:IsolationReport
 * {"dict": to_dict(), "summary": summary()} for a report assembled from its parts. */
json_t *w19_pluginisol_f4e6f5_report(const char *verdict, json_t *reasons,
                                     json_t *notes, json_t *ctx_methods);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PLUGINISOL_F4E6F5_H */
