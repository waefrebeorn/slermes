#ifndef NOC_W17_PLUGINSPRO_9AEFDD_H
#define NOC_W17_PLUGINSPRO_9AEFDD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_pluginspro_9aefdd_classify @ hermes_cli/plugins_provenance.py:plugins_provenance
 * Pure 2x2 of row-presence x .git-presence. rows is an object name->row(dict|null);
 * dirs is an array of {"name":str,"has_git":bool}. Returns an array of
 * {"name":str,"klass":str,"saved_update_url":str|null} in dirs order.
 */
struct json_t *w17_pluginspro_9aefdd_classify(const struct json_t *rows,
                                              const struct json_t *dirs);

/* PoP: w17_pluginspro_9aefdd_git_origin @ hermes_cli/plugins_provenance.py:_git_origin_url
 * Ports the .git/config fallback parser over config FILE TEXT (no filesystem,
 * no git subprocess). Returns a JSON string (origin URL) or null.
 */
struct json_t *w17_pluginspro_9aefdd_git_origin(const char *config_text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSPRO_9AEFDD_H */
