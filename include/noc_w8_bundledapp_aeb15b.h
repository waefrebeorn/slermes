#ifndef NOC_W8_BUNDLEDAPP_AEB15B_H
#define NOC_W8_BUNDLEDAPP_AEB15B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_bundledapp_aeb15b_only @ hermes_cli/bundled_app.py:_only */
struct json_t *w8_bundledapp_aeb15b_only(const struct json_t *candidates);

/* PoP: w8_bundledapp_aeb15b_launcher @ hermes_cli/bundled_app.py:_resolve_launcher
 * `files` is an array of {"path":str,"exec":bool}; the fixture supplies the
 * directory listing (the oracle stubs _files). Returns the single launcher
 * path as a string, or JSON null when ambiguous. */
struct json_t *w8_bundledapp_aeb15b_launcher(const struct json_t *files, const char *platform);

/* PoP: w8_bundledapp_aeb15b_layout @ hermes_cli/bundled_app.py:resolve_bundle_layout
 * Pure path/layout decision: {"ok":true, app_root/resources/payload/launcher}
 * or {"ok":false,"error":"not_bundled"|"no_app_dir"}. `app_root_is_dir` is the
 * fixture-supplied is_dir() answer for the final app root. */
struct json_t *w8_bundledapp_aeb15b_layout(const char *project_root, const char *platform,
                                           const struct json_t *files, int app_root_is_dir);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_BUNDLEDAPP_AEB15B_H */
