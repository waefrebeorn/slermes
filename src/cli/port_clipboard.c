/*
 * port_clipboard.c
 *
 * Pure helpers ported from hermes_cli/clipboard.py.
 *
 * The module is almost entirely platform backends - subprocess probes for
 * pbcopy / wl-copy / xclip / pngpaste, PowerShell Set-Clipboard, and file
 * copies. Only two predicates are dependency-free:
 *
 *   cb_is_remote_shell_session <- clipboard.py:is_remote_shell_session
 *   cb_macos_has_bitmap         <- clipboard.py:_macos_has_bitmap
 *
 * The env lookup is a parameter so the C side does not own os.environ.
 */

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  is_remote_shell_session  (clipboard.py:is_remote_shell_session)
 *
 *      bool(e.get("SSH_CONNECTION") or e.get("SSH_TTY") or e.get("SSH_CLIENT"))
 *
 *  Over SSH the native clipboard tools write the REMOTE machine's clipboard
 *  (or an X-forwarded one), which is almost never what the user wants - OSC 52
 *  reaches the LOCAL terminal instead. `env_has` stands in for the mapping
 *  lookup; a NULL or empty value is falsy, matching dict.get's `or` chain.
 * =========================================================================== */

/* PoP: is_remote_shell_session @ hermes_cli/clipboard.py:is_remote_shell_session */
int cb_is_remote_shell_session(int (*env_has)(const char *name))
{
    if (!env_has) return 0;
    return env_has("SSH_CONNECTION") || env_has("SSH_TTY") || env_has("SSH_CLIENT");
}

/* ===========================================================================
 *  _macos_has_bitmap  (clipboard.py:_macos_has_bitmap)
 *
 *      "«class PNGf»" in info or "«class TIFF»" in info
 *
 *  The pasteboard class tokens osascript reports for bitmap content. Both are
 *  UTF-8 string literals; the guillemets are C2 AB / C2 BB.
 * =========================================================================== */

/* PoP: _macos_has_bitmap @ hermes_cli/clipboard.py:_macos_has_bitmap */
int cb_macos_has_bitmap(const char *info)
{
    if (!info) return 0;
    static const char PNGf[]  = "\xc2\xab" "class PNGf"  "\xc2\xbb";
    static const char TIFF[]  = "\xc2\xab" "class TIFF"  "\xc2\xbb";
    return strstr(info, PNGf) != NULL || strstr(info, TIFF) != NULL;
}
