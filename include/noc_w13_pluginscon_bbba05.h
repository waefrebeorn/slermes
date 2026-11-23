#ifndef NOC_W13_PLUGINSCON_BBBA05_H
#define NOC_W13_PLUGINSCON_BBBA05_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_pluginscon_bbba05_plugin_skill_owners @ hermes_cli/plugins_content.py:plugin_skill_owners */
struct json_t *w13_pluginscon_bbba05_plugin_skill_owners(const struct json_t *in);
/* PoP: w13_pluginscon_bbba05_skill_name_valid @ hermes_cli/plugins_content.py:register_skill */
struct json_t *w13_pluginscon_bbba05_skill_name_valid(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_PLUGINSCON_BBBA05_H */
