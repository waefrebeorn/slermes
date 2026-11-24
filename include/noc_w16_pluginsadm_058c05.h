#ifndef NOC_W16_PLUGINSADM_058C05_H
#define NOC_W16_PLUGINSADM_058C05_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_pluginsadm_058c05_dependency_conflict_message @ hermes_cli/plugins_admission.py:DependencyConflict.__init__ */
struct json_t *w16_pluginsadm_058c05_dependency_conflict_message(const struct json_t *cause,
                                                                const struct json_t *plugin);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PLUGINSADM_058C05_H */
