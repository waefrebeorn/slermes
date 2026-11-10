/*
 * port_aux_task_builder.h — pure registry-entry builder from
 * hermes_cli/plugins_aux_tasks.py build_auxiliary_task_entry. Returns NULL
 * (like the Python ValueError) for an invalid key/reservation/ownership.
 */
#ifndef PORT_AUX_TASK_BUILDER_H
#define PORT_AUX_TASK_BUILDER_H

/* Build a plugin auxiliary-task registry entry, or NULL when the key is
 * invalid/empty/non-alnum-underscore, reserved by a built-in, or already
 * registered by a different plugin. Parameterized facts:
 *   defaults_json   - the hardcoded defaults dict (object or NULL)
 *   registered_json - manager's "_aux_tasks" (object key->entry with "plugin")
 *   builtin_keys_json - reserved built-in keys (array of strings)
 * Returns malloc'd JSON, caller frees. */
char *bat_aux_task_entry(const char *me, const char *owner_id, const char *key,
                         const char *display_name, const char *description,
                         const char *defaults_json, const char *inherit_from,
                         const char *registered_json, const char *builtin_keys_json);

#endif /* PORT_AUX_TASK_BUILDER_H */
