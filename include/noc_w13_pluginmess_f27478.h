#ifndef NOC_W13_PLUGINMESS_F27478_H
#define NOC_W13_PLUGINMESS_F27478_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* A handle state is {"target_null":bool,"result":{...},"message_id":<raw>}. */
/* PoP: w13_pluginmess_f27478_err @ hermes_cli/plugin_messages.py:_err */
struct json_t *w13_pluginmess_f27478_err(const struct json_t *code, const struct json_t *detail);
/* PoP: w13_pluginmess_f27478_props @ hermes_cli/plugin_messages.py:PluginMessage.ok/error */
struct json_t *w13_pluginmess_f27478_props(const struct json_t *state);
/* PoP: w13_pluginmess_f27478_repr @ hermes_cli/plugin_messages.py:PluginMessage.__repr__ */
struct json_t *w13_pluginmess_f27478_repr(const struct json_t *state);
/* PoP: w13_pluginmess_f27478_refused @ hermes_cli/plugin_messages.py:PluginMessage._refused */
struct json_t *w13_pluginmess_f27478_refused(const struct json_t *state);
/* PoP: w13_pluginmess_f27478_settle @ hermes_cli/plugin_messages.py:PluginMessage._settle */
struct json_t *w13_pluginmess_f27478_settle(const struct json_t *state, const struct json_t *result);
/* PoP: w13_pluginmess_f27478_adopt @ hermes_cli/plugin_messages.py:PluginMessage._adopt */
struct json_t *w13_pluginmess_f27478_adopt(const struct json_t *state, const struct json_t *other);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_PLUGINMESS_F27478_H */
