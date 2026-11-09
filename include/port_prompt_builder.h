/*
 * port_prompt_builder.h -- pure helpers ported from agent/prompt_builder.py.
 *
 * The config / file-scan / guidance half stays in Python.
 */
#ifndef PORT_PROMPT_BUILDER_H
#define PORT_PROMPT_BUILDER_H

#include <stddef.h>

/* _strip_yaml_frontmatter -- drop optional --- YAML frontmatter. Caller frees. */
char *pb_strip_yaml_frontmatter(const char *content);

/* format_steer_marker -- wrap a mid-turn steer in the out-of-band marker.
 * Caller frees. */
char *pb_format_steer_marker(const char *steer_text);

/* _format_backend_probe -- render the probe's os/kernel lines as
 * "  OS: <os> <kernel>", or "" when neither is usable. Caller frees. */
char *pb_format_backend_probe(const char *output);

/* _plugin_backend_is_remote -- `attr_is_remote` stands in for the plugin
 * attribute lookup (0 when the backend is unknown to the registry). */
int pb_plugin_backend_is_remote(const char *backend, int attr_is_remote);

#endif /* PORT_PROMPT_BUILDER_H */
