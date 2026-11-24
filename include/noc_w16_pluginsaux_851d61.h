#ifndef NOC_W16_PLUGINSAUX_851D61_H
#define NOC_W16_PLUGINSAUX_851D61_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_pluginsaux_851d61_build_entry @ hermes_cli/plugins_aux_tasks.py:build_auxiliary_task_entry */
struct json_t *w16_pluginsaux_851d61_build_entry(const char *me, const char *owner_id,
                                                 const struct json_t *key,
                                                 const char *display_name, const char *description,
                                                 const struct json_t *defaults,
                                                 const struct json_t *inherit_from,
                                                 const struct json_t *registered);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PLUGINSAUX_851D61_H */
