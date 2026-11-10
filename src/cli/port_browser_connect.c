/*
 * port_browser_connect.c
 *
 * Pure helpers ported from hermes_cli/browser_connect.py.
 *
 * That module is mostly I/O - subprocess launches, sqlite3 auth DBs, socket
 * probes, Windows registry lookups, shutil profile copies. This file holds the
 * dependency-free decision logic only:
 *
 *   bc_classify_default  <- browser_connect.py:_classify_default
 *   bc_first_present     <- browser_connect.py:_first_present
 *
 * Deliberately NOT ported: detect_default_windows/darwin/linux (registry,
 * LaunchServices, xdg), snapshot_real_profile (shutil tree copies),
 * launch_chrome_debug (subprocess), is_browser_debug_ready (sockets).
 */

#include "hermes_json.h"
#include "port_browser_connect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

/* ===========================================================================
 *  _classify_default  (browser_connect.py:_classify_default)
 *
 *      if any(match(value, chan) for chan in channels):
 *          return UNSUPPORTED_CHANNEL
 *      return next((browser for frag, browser in table if match(value, frag)), None)
 *
 *  Channels are checked FIRST and fail closed: a Beta/Dev/Canary identifier
 *  must return UNSUPPORTED_CHANNEL rather than matching the stable profile.
 *  `match` is a caller-supplied predicate (the Python side passes a re.search
 *  or equality closure); `table` is a NULL-terminated array of
 *  {fragment, canonical} pairs.
 * =========================================================================== */

/* PoP: _classify_default @ hermes_cli/browser_connect.py:_classify_default */
const char *bc_classify_default(const char *value,
                                const char *const *channels,
                                const bc_fragment_t *table,
                                int (*match)(const char *value, const char *needle))
{
    if (!value) return NULL;
    if (match) {
        for (size_t i = 0; channels && channels[i]; i++)
            if (match(value, channels[i])) return BC_UNSUPPORTED_CHANNEL;
        for (size_t i = 0; table && table[i].fragment; i++)
            if (match(value, table[i].fragment)) return table[i].canonical;
    }
    return NULL;
}

/* ===========================================================================
 *  _first_present  (browser_connect.py:_first_present)
 *
 *      next((p for p in paths if p and os.path.isfile(p)), None)
 *
 *  First entry that names an existing regular file, else NULL. The returned
 *  pointer aliases the input array - caller owns nothing.
 * =========================================================================== */

/* PoP: _first_present @ hermes_cli/browser_connect.py:_first_present */
const char *bc_first_present(const char *const *paths)
{
    if (!paths) return NULL;
    for (size_t i = 0; paths[i]; i++) {
        const char *p = paths[i];
        if (!p || !*p) continue;
        struct stat st;
        if (stat(p, &st) == 0 && S_ISREG(st.st_mode)) return p;
    }
    return NULL;
}
