#ifndef NOC_W9_PLUGINRELOA_37AFA1_H
#define NOC_W9_PLUGINRELOA_37AFA1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_pluginreloa_37afa1_tree_fingerprint @ providers/plugin_reload.py:tree_fingerprint
 *
 * Seam for the os.walk/os.stat layer: `files` is a JSON array of
 * {"name": str, "size": <int64 decimal string>,
 *  "mtime_ns": <int64 decimal string>} already found in the plugin root.
 * The C port reproduces the .py/plugin-manifest filter, the per-name sort,
 * the "relpath:size:mtime_ns" join and the sha1()[:12] digest of the live
 * function.  Returns a JSON string (12 lowercase hex chars). */
struct json_t *w9_pluginreloa_37afa1_tree_fingerprint(const struct json_t *files);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_PLUGINRELOA_37AFA1_H */
