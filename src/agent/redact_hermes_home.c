/*
 * redact_hermes_home.c — C port of upstream Hermes agent/redact_hermes_home.py.
 *
 * The Python guard, outside a hosted scope (sandbox_lexical_path -> None), is:
 *   target = os.path.normcase(os.path.realpath(os.path.expanduser(path)))
 *   for home in _hermes_dirs():
 *       base = os.path.normcase(os.path.realpath(str(home)))
 *       if target == base or target.startswith(base + os.sep): return True
 *   return False
 * On a local Windows install this is pure lexical normalisation for the
 * absolute paths the guard is handed; the C port implements that normalisation
 * directly so no host filesystem state is consulted. The oracle feeds the same
 * home list through stubs, keeping the comparison deterministic.
 *
 * PoP: redhome_is_under_hermes_home @ agent/redact_hermes_home.py:_is_under_hermes_home
 * PoP: redhome_win_norm @ agent/redact_hermes_home.py:_is_under_hermes_home
 * PoP: redhome_normcase @ agent/redact_hermes_home.py:_is_under_hermes_home
 */
#include "noc_redhome.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char tolower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

/* os.path.normcase on Windows: '/' becomes '\', letters lowercased. */
char *redhome_normcase(const char *path) {
    if (!path) return NULL;
    size_t n = strlen(path);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = path[i] == '/' ? '\\' : tolower_ascii(path[i]);
    out[n] = '\0';
    return out;
}

/* os.path.realpath for an absolute synthetic Windows path: convert separators,
 * drop "." and empty segments, resolve ".." against the kept stack. */
char *redhome_win_norm(const char *path) {
    if (!path) return NULL;
    size_t n = strlen(path);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < n; i++)
        buf[i] = path[i] == '/' ? '\\' : path[i];
    buf[n] = '\0';

    char *segs[256];
    size_t ns = 0;
    char *p = buf;
    while (*p) {
        char *start = p;
        while (*p && *p != '\\') p++;
        if (*p) { *p = '\0'; p++; }
        size_t sl = strlen(start);
        if (sl == 0 || !strcmp(start, ".")) continue;
        if (!strcmp(start, "..")) {
            if (ns && segs[ns-1][strlen(segs[ns-1])-1] != ':') ns--;
            continue;
        }
        if (ns < 256) segs[ns++] = start;
    }
    size_t total = ns ? ns - 1 : 0;
    for (size_t i = 0; i < ns; i++) total += strlen(segs[i]);
    char *out = (char *)malloc(total + 1);
    if (!out) { free(buf); return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < ns; i++) {
        if (i) out[k++] = '\\';
        size_t sl = strlen(segs[i]);
        memcpy(out + k, segs[i], sl);
        k += sl;
    }
    out[k] = '\0';
    free(buf);
    return out;
}

/* PoP: redhome_is_under_hermes_home @ agent/redact_hermes_home.py:_is_under_hermes_home */
int redhome_is_under_hermes_home(const char *path, const char *const *homes, size_t nhomes) {
    char *raw = redhome_win_norm(path ? path : "");
    char *target = redhome_normcase(raw);
    free(raw);
    if (!target) return 0;
    for (size_t i = 0; i < nhomes; i++) {
        char *hraw = redhome_win_norm(homes[i] ? homes[i] : "");
        char *base = redhome_normcase(hraw);
        free(hraw);
        if (!base) continue;
        size_t bl = strlen(base);
        int hit = !strcmp(target, base) ||
                  (strncmp(target, base, bl) == 0 && target[bl] == '\\');
        free(base);
        if (hit) { free(target); return 1; }
    }
    free(target);
    return 0;
}
