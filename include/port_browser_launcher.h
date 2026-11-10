/*
 * port_browser_launcher.h -- pure helpers ported from tools/browser_launcher.py.
 *
 * The download / spawn / singleton half stays in Python.
 */
#ifndef PORT_BROWSER_LAUNCHER_H
#define PORT_BROWSER_LAUNCHER_H

#include <stddef.h>

/* _has_display -- DISPLAY or WAYLAND_DISPLAY present and non-empty. `env` is a
 * flat [key, value, ...] array. */
int bl_has_display(const char *const *env);

/* build_chromium_argv -- the launch argv. `configured` stands in for
 * browser.chromium_args, `needs_bypass` for the host sandbox policy.
 * Returns a malloc'd NULL-terminated array of malloc'd strings; the caller
 * frees each string and the array. */
char **bl_build_argv(const char *executable, const char *user_data_dir,
                     int packaged, int is_macos, int headed,
                     int sandbox_bypass, int needs_bypass,
                     const char *const *configured,
                     const char *const *extra_args, int *out_count);

#endif /* PORT_BROWSER_LAUNCHER_H */
