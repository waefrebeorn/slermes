/*
 * port_moa_slash.h — Faithful C11 ports of MoA slash-command handlers from
 * hermes_cli/moa_slash.py and gateway/moa_slash.py (REAL_GAP set).
 */

#ifndef PORT_MOA_SLASH_H
#define PORT_MOA_SLASH_H

/* hermes_cli/moa_slash.py — run a MoA slash command, return formatted text (caller frees). */
char *moa_cli_handle_slash_command(const char *prompt, const char *mode);
char *moa_cli_handle_slash_command_sync(const char *prompt, const char *mode);

/* gateway/moa_slash.py — parse "/moa <prompt> [mode]" and run, return formatted text (caller frees). */
char *moa_gateway_handle_command(const char *text);
void moa_gateway_register_slash_handler(void);

#endif /* PORT_MOA_SLASH_H */
