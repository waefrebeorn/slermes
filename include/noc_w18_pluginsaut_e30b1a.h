#ifndef NOC_W18_PLUGINSAUT_E30B1A_H
#define NOC_W18_PLUGINSAUT_E30B1A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: pluginsaut_held_with_ref @ hermes_cli/plugins_auto_update.py:held_with_ref */
int pluginsaut_held_with_ref(const struct json_t *record);

/* PoP: pluginsaut_summary_lines @ hermes_cli/plugins_auto_update.py:summary_lines
 * Plain-text lines for an update_all_unattended outcome. Caller frees the JSON. */
struct json_t *pluginsaut_summary_lines(const struct json_t *outcome);

/* PoP: pluginsaut_slash_action @ hermes_cli/plugins_auto_update.py:plugins_slash_action
 * Seams: `outcome` stands in for update_all_unattended(), `state` for
 * auto_update_state()/set_auto_update(). Caller frees the JSON array. */
struct json_t *pluginsaut_slash_action(const struct json_t *parts,
                                       const struct json_t *outcome,
                                       const struct json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_PLUGINSAUT_E30B1A_H */
