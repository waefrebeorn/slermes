/* linux_desktop_entry.h — Port of hermes_cli/linux_desktop_entry.py.
 * Install and remove the Linux XDG desktop entry (hermes.desktop).
 */
#ifndef SLERMES_LINUX_DESKTOP_ENTRY_H
#define SLERMES_LINUX_DESKTOP_ENTRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: is_supported @ hermes_cli/linux_desktop_entry.py:is_supported */
bool lde_is_supported(void);

/* PoP: _xdg_data_home @ hermes_cli/linux_desktop_entry.py:_xdg_data_home */
/* Return the XDG data home (XDG_DATA_HOME or ~/.local/share). Caller frees. */
char *lde_xdg_data_home(void);

/* PoP: desktop_entry_path @ hermes_cli/linux_desktop_entry.py:desktop_entry_path */
/* Where the hermes.desktop entry lives. Caller frees. */
char *lde_desktop_entry_path(void);

/* PoP: icon_path @ hermes_cli/linux_desktop_entry.py:icon_path */
/* The app icon shipped in the desktop workspace. Caller frees. */
char *lde_icon_path(const char *project_root);

/* PoP: resolve_exec_command @ hermes_cli/linux_desktop_entry.py:resolve_exec_command */
/* Build the absolute Exec= command line for `hermes desktop`. Caller frees. */
char *lde_resolve_exec_command(void);

/* PoP: _quote_exec_arg @ hermes_cli/linux_desktop_entry.py:_quote_exec_arg */
/* Quote one Exec argument per the desktop entry spec. Caller frees. */
char *lde_quote_exec_arg(const char *arg);

/* PoP: render_desktop_entry @ hermes_cli/linux_desktop_entry.py:render_desktop_entry */
/* Render the full [Desktop Entry] file text. Caller frees. */
char *lde_render_desktop_entry(const char *exec_command, const char *icon);

/* PoP: refresh_desktop_databases @ hermes_cli/linux_desktop_entry.py:refresh_desktop_databases */
/* Reindex menu caches; run each tool only when it exists. Returns a malloc'd
 * JSON array of tool names that ran (caller frees), and sets *out_ran to count. */
char *lde_refresh_desktop_databases(const char *applications_dir, int *out_ran);

/* PoP: _run_quiet @ hermes_cli/linux_desktop_entry.py:_run_quiet */
bool lde_run_quiet(const char *const *cmd, int cmd_count);

/* PoP: install_desktop_entry @ hermes_cli/linux_desktop_entry.py:install_desktop_entry */
/* Write (or refresh) the Hermes desktop entry. Returns its path (caller frees)
 * or NULL on non-Linux or write failure. */
char *lde_install_desktop_entry(const char *project_root);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_LINUX_DESKTOP_ENTRY_H */
