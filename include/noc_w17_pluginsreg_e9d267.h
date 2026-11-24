#ifndef NOC_W17_PLUGINSREG_E9D267_H
#define NOC_W17_PLUGINSREG_E9D267_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_pluginsreg_e9d267_normalize @ hermes_cli/plugins_registrars.py:_NAME_NORMALIZERS
 * mode: "strip" -> strip(), "lower" -> strip().lower(), "none" -> raw name.
 */
struct json_t *w17_pluginsreg_e9d267_normalize(const char *name, const char *mode);

/* PoP: w17_pluginsreg_e9d267_row @ hermes_cli/plugins_registrars.py:_ROWS_BY_METHOD
 * Look up a register_<category>_provider row by method name. Returns
 * {kind,registry_mod,base_ref,label,normalize,article} or null when unknown.
 * normalize is the row's options.get("normalize","strip") (null means raw).
 */
struct json_t *w17_pluginsreg_e9d267_row(const char *method);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSREG_E9D267_H */
