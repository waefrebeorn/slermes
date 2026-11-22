/* hermes_home_defaults.c — LIVE-verified port of hermes_constants.py
 * platform-default home helpers. Kept in a dedicated object so the gate can
 * verify the pure behavior without the heavy config import chain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Join two POSIX-style path components, collapsing duplicate separators. */
static void hj(char *out, size_t n, const char *a, const char *b) {
    size_t la = strlen(a), lb = b ? strlen(b) : 0;
    while (la && a[la-1] == '/') la--;
    while (lb && b[0] == '/') { b++; lb--; }
    if (la + lb + 2 > n) { out[0] = '\0'; return; }
    memcpy(out, a, la);
    out[la] = '/';
    memcpy(out + la + 1, b, lb);
    out[la + 1 + lb] = '\0';
}

/* PoP: _platform_default_hermes_home @ hermes_constants.py */
void hc_platform_default_hermes_home(const char *suffix,
                                     const char *local_appdata,
                                     const char *user_home,
                                     int win32,
                                     char *out, size_t n)
{
    const char *s = suffix ? suffix : "";
    char name[512];
    if (win32) {
        char trimmed[1024];
        const char *v = local_appdata ? local_appdata : "";
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        size_t len = strlen(v);
        while (len && (v[len-1] == ' ' || v[len-1] == '\t' ||
                       v[len-1] == '\n' || v[len-1] == '\r')) len--;
        if (len >= sizeof(trimmed)) len = sizeof(trimmed) - 1;
        memcpy(trimmed, v, len); trimmed[len] = '\0';
        char base[2048];
        if (trimmed[0]) {
            snprintf(base, sizeof(base), "%s", trimmed);
        } else {
            char local[2048];
            hj(local, sizeof(local), user_home ? user_home : "", "AppData");
            hj(base, sizeof(base), local, "Local");
        }
        snprintf(name, sizeof(name), "hermes%s", s);
        hj(out, n, base, name);
    } else {
        snprintf(name, sizeof(name), ".hermes%s", s);
        hj(out, n, user_home ? user_home : "", name);
    }
}

/* PoP: _get_platform_default_hermes_home @ hermes_constants.py */
void hc_get_platform_default_hermes_home(char *out, size_t n)
{
    hc_platform_default_hermes_home(getenv("HERMES_DATA_DIR_SUFFIX") ?: "",
                                   getenv("LOCALAPPDATA") ?: "",
                                   getenv("HOME") ?: "", 1, out, n);
}
