/*
 * port_plugin_messages.h — pure helpers from gateway/plugin_messages.py.
 * The adapter publish/edit IO stays unported.
 */
#ifndef PORT_PLUGIN_MESSAGES_H
#define PORT_PLUGIN_MESSAGES_H

/* _err(code, detail): {"ok":false,"error":code, "detail":detail?}. Returns
 * malloc'd JSON, caller frees. */
char *plm_err(const char *code, const char *detail);

/* _fit(text, limit): clamp text to the per-chat message limit with a trailing
 * ellipsis (U+2026), never splitting the message — the len-measured path
 * (limit<=0 -> unchanged; len(text)<=limit -> unchanged). Returns malloc'd,
 * caller frees. */
char *plm_fit(const char *text, int limit);

/* _supports_edit: False when the adapter's SUPPORTS_MESSAGE_EDITING flag is
 * False, else True iff it overrides edit_message. flags are the facts. */
int plm_supports_edit(int editing_flag, int has_custom_edit);

#endif /* PORT_PLUGIN_MESSAGES_H */
