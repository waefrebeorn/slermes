#ifndef SLERMES_BROWSER_TOOL_INSTALL_H
#define SLERMES_BROWSER_TOOL_INSTALL_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_install browser_tool_install_t;

browser_tool_install_t *browser_tool_install_init(void);
void browser_tool_install_cleanup(browser_tool_install_t *s);

bool browser_maybe_autoinstall_chromium(void);
bool browser_chromium_installed(void);
bool browser_is_local_backend(void);
bool browser_is_local_sidecar_key(const char *session_key);
bool browser_allow_private_urls(void);
bool browser_is_always_blocked_url(const char *url);
bool browser_is_safe_url(const char *url);
bool browser_is_camofox_mode(void);

#endif /* SLERMES_BROWSER_TOOL_INSTALL_H */
