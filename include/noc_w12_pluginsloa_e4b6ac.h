/* noc_w12_pluginsloa_e4b6ac.h — public C API port of hermes_cli/plugins_loader.py */
#ifndef NOC_W12_PLUGINSLOA_E4B6AC_H
#define NOC_W12_PLUGINSLOA_E4B6AC_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _dist_name: leading distribution name up to the first requirement operator,
 * whitespace or bracket, then stripped. */
void w12_pluginsloa_e4b6ac_dist_name(const char *req, char *out, size_t cap);

/* _load_error_text: a human-readable load failure. For SystemExit the code's
 * repr is named; otherwise the plain str(exc) text. */
void w12_pluginsloa_e4b6ac_load_error_text(bool systemexit, const json_t *code,
                          const char *text, char *out, size_t cap);

/* _platform_name_from_manifest: strip a trailing "-platform" from the manifest
 * name, else use the directory basename, else the name. */
void w12_pluginsloa_e4b6ac_platform_name(const char *name, const char *path, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_PLUGINSLOA_E4B6AC_H */
