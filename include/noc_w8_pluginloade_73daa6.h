#ifndef NOC_W8_PLUGINLOADE_73DAA6_H
#define NOC_W8_PLUGINLOADE_73DAA6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_pluginloade_73daa6_plugin_child_dir @ plugins/plugin_loader.py:plugin_child_dir
 * root/name when name is a plain plugin directory segment
 * (^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$), else NULL (None). Returns a malloc'd
 * '/' separated path; caller frees. Non-string names are treated as None. */
char *w8_pluginloade_73daa6_plugin_child_dir(const char *root, const char *name);

/* PoP: w8_pluginloade_73daa6_load_would_deadlock @ plugins/plugin_loader.py:_load_would_deadlock
 * `graph` injects _LOAD_OWNERS (name->tid), _LOAD_WAITERS (tid->name),
 * importlib._bootstrap._blocking_on (tid->owner or [owner,...]) and `me`.
 * Returns 0/1 (False/True): whether the wait graph leads back to `me`. */
int w8_pluginloade_73daa6_load_would_deadlock(const struct json_t *graph);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_PLUGINLOADE_73DAA6_H */
