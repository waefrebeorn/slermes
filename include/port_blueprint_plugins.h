/*
 * port_blueprint_plugins.h — pure validators from cron/blueprint_plugins.py.
 *
 * _text, _strings and _placeholders are pure; the plugin-file/manager IO in
 * build_plugin_blueprint / plugin_blueprints stays unported. The BlueprintSlot
 * / key-name regexes mirror the module exactly.
 */
#ifndef PORT_BLUEPRINT_PLUGINS_H
#define PORT_BLUEPRINT_PLUGINS_H

/* _text(name, value): 0 ok (out = malloc'd stripped value) or -1 when value is
 * not a non-empty string (Python ValueError). Caller frees out. */
int bp_text(const char *name, const char *value, char **out);

/* _strings(name, value_json): 0 ok or -1 when value is not an iterable of
 * non-empty strings. value_json is a JSON array of strings. */
int bp_strings(const char *name, const char *value_json);

/* _placeholders(template, what): scans Python format-field syntax, returns 0
 * ok (out = malloc'd JSON array of the slot names, e.g. "[\"a\",\"b\"]") or -1
 * when a placeholder name is not a plain slot name (Python ValueError).
 * Caller frees out. */
int bp_placeholders(const char *template, const char *what, char **out);

#endif /* PORT_BLUEPRINT_PLUGINS_H */
