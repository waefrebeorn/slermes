#ifndef NOC_W9_PLUGINMESSAGE_825C6F_H
#define NOC_W9_PLUGINMESSAGE_825C6F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_pluginmessa_825c6f_err @ gateway/plugin_messages.py:_err */
struct json_t *w9_pluginmessa_825c6f_err(const struct json_t *in);

/* PoP: w9_pluginmessa_825c6f_fit @ gateway/plugin_messages.py:_fit
 * Text is clamped to the injected per-chat limit with a trailing ellipsis. */
struct json_t *w9_pluginmessa_825c6f_fit(const struct json_t *in);

/* PoP: w9_pluginmessa_825c6f_supports_edit @ gateway/plugin_messages.py:_supports_edit */
struct json_t *w9_pluginmessa_825c6f_supports_edit(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_PLUGINMESSAGE_825C6F_H */
