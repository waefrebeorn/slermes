/*
 * noc_w18_pluginscmd_58cffc.h -- public C API for the deterministic
 * desktop-folder-name tail of hermes_cli/plugins_cmd_inspect.py
 * (wave18 gate w18_pluginscmd_58cffc).
 */
#ifndef NOC_W18_PLUGINSCMD_58CFFC_H
#define NOC_W18_PLUGINSCMD_58CFFC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_pluginscmd_58cffc_desktop_folder_name @ hermes_cli/plugins_cmd_inspect.py:_desktop_folder_name
 * Electron's desktopPluginFolderName: the subdirectory's last real segment,
 * else the repo name. Backslashes (Windows Path str()) are folded to '/',
 * empty / "." / "desktop" segments are dropped, and the LAST survivor wins.
 * Caller frees the returned string. */
char *w18_pluginscmd_58cffc_desktop_folder_name(const char *git_url, const char *subdir);

/* PoP: w18_pluginscmd_58cffc_repo_name_from_url @ hermes_cli/plugins_cmd.py:_repo_name_from_url
 * Facade fallback used by _desktop_folder_name when no subdir segment
 * survives. Caller frees the returned string. */
char *w18_pluginscmd_58cffc_repo_name_from_url(const char *url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_PLUGINSCMD_58CFFC_H */
