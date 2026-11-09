/*
 * port_clipboard.h -- pure predicates ported from hermes_cli/clipboard.py.
 *
 * The platform clipboard backends (subprocess probes, PowerShell, pngpaste)
 * stay in Python.
 */
#ifndef PORT_CLIPBOARD_H
#define PORT_CLIPBOARD_H

#include <stddef.h>

/* clipboard.py:is_remote_shell_session -- any of SSH_CONNECTION / SSH_TTY /
 * SSH_CLIENT present. env_has stands in for the mapping lookup. */
int cb_is_remote_shell_session(int (*env_has)(const char *name));

/* clipboard.py:_macos_has_bitmap -- the pasteboard holds PNG or TIFF bitmap
 * content (osascript's «class PNGf» / «class TIFF» tokens). */
int cb_macos_has_bitmap(const char *info);

#endif /* PORT_CLIPBOARD_H */
