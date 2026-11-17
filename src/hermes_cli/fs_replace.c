/* fs_replace.c -- os.replace for the C port.
 *
 * POSIX rename() atomically replaces an existing destination. Windows/MinGW
 * rename() FAILS with EEXIST instead, so every atomic-write path in this tree
 * (temp file + rename) silently kept the STALE file after its first write: a
 * config edit, a cache refresh or a state update reported success and then read
 * back the old contents. Python's os.replace overwrites on every platform.
 *
 * Kept in its own translation unit so one definition serves every caller and
 * the symbol is easy to spot in the link line.
 */
#include <stdio.h>

#include "port_utils_ports.h"

int util_rename_replace(const char *src, const char *dst) {
    if (!src || !dst) return -1;
    if (rename(src, dst) == 0) return 0;
    /* Destination exists (or the move is cross-volume): clear it and retry.
     * The window is not atomic; content correctness is what matters here. */
    if (remove(dst) != 0) return -1;
    if (rename(src, dst) != 0) return -1;
    return 0;
}
