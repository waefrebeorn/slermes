/* cpu_profile.c — C home for upstream Hermes hermes_cli/cpu_profile.py.
 *
 * PoP: w8_cpuprofile_resolve_profile_path_ex @ hermes_cli/cpu_profile.py:resolve_profile_path
 *
 * Faithful port of resolve_profile_path(path).  `home` is injected for the
 * hermes_constants.get_hermes_home() seam and for ~ expansion.  Path.resolve()
 * is mirrored as lexical normalisation (drive-qualified absolute paths only,
 * as used by the fixtures): drop "." and empty components, pop on "..",
 * collapse separators, and emit pathlib's POSIX spelling.
 */
#include "noc_w8_cpuprofile_dfa160.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int is_sep(char c) { return c == '/' || c == '\\'; }

/* Path.resolve() lexical normalisation for Windows-style paths. */
static void normalise(const char *combined, char *out, size_t cap) {
    char stack[128][1024];
    int n = 0;
    const char *p = combined;
    while (*p) {
        while (is_sep(*p)) p++;
        if (!*p) break;
        const char *s = p;
        while (*p && !is_sep(*p)) p++;
        size_t len = (size_t)(p - s);
        if (len == 1 && s[0] == '.') continue;
        if (len == 2 && s[0] == '.' && s[1] == '.') {
            if (n > 1) { n--; continue; }  /* never pop the drive prefix */
            if (n == 1) continue;          /* at drive root: .. is a no-op */
            continue;
        }
        if (n < 128 && len < 1024) {
            memcpy(stack[n], s, len);
            stack[n][len] = '\0';
            n++;
        }
    }
    size_t o = 0;
    for (int i = 0; i < n; i++) {
        if (i && o + 1 < cap) out[o++] = '/';
        size_t l = strlen(stack[i]);
        for (size_t j = 0; j < l && o + 1 < cap; j++) out[o++] = stack[i][j];
    }
    out[o] = '\0';
    if (o == 0) snprintf(out, cap, ".");
}

json_t *w8_cpuprofile_resolve_profile_path_ex(const char *path, const char *home) {
    const char *t = path ? path : "";
    const char *h = home ? home : ".";
    char expanded[8192];

    /* Path(path).expanduser(): only a leading "~" (or "~/") expands. */
    if (t[0] == '~' && (t[1] == '\0' || is_sep(t[1]))) {
        snprintf(expanded, sizeof(expanded), "%s%s", h, t + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", t);
    }

    int absolute = (strlen(expanded) >= 3 && isalpha((unsigned char)expanded[0])
                    && expanded[1] == ':' && is_sep(expanded[2]));

    char combined[16384];
    if (absolute) {
        /* Keep the drive letter as the first path component ("C:"). */
        char drive[3] = { expanded[0], ':', '\0' };
        snprintf(combined, sizeof(combined), "%s/%s", drive, expanded + 3);
    } else {
        /* get_hermes_home() / "logs" / "profiles" / target */
        snprintf(combined, sizeof(combined), "%s/logs/profiles/%s", h, expanded);
    }

    char resolved[16384];
    normalise(combined, resolved, sizeof(resolved));
    return json_string(resolved);
}

