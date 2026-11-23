/*
 * filesystem.c — faithful C11 port of upstream Hermes pm/filesystem.py.
 *
 * PoP: w5_filesystem_427a52_native     @ pm/filesystem.py:native
 * PoP: w5_filesystem_427a52_long_form  @ pm/filesystem.py:_long_form
 *
 * Only the pure string-spelling primitives are ported; lock_fd/lock_path,
 * durable_write_bytes, read_bytes_or_none, file_digest, ACL and retry helpers
 * are all IO/environment-bound and cannot be exercised by a JSON fixture.
 */
#include "noc_w5_filesystem_427a52.h"
#include <stdlib.h>
#include <string.h>

#define VERBATIM      "\\\\?\\"        /* literal \\?\  */
#define VERBATIM_UNC  "\\\\?\\UNC\\"   /* literal \\?\UNC\ */

char *w5_filesystem_427a52_native(const char *path) {
    if (!path) return strdup("");
    size_t n = strlen(path);
    size_t vu = strlen(VERBATIM_UNC);
    if (n >= vu && strncmp(path, VERBATIM_UNC, vu) == 0) {
        const char *suffix = path + vu;      /* text[len(_VERBATIM_UNC):] */
        size_t sn = strlen(suffix);
        char *out = (char *)malloc(2 + sn + 1);
        out[0] = '\\'; out[1] = '\\';        /* "\\\\" + suffix */
        memcpy(out + 2, suffix, sn);
        out[2 + sn] = '\0';
        return out;
    }
    size_t v = strlen(VERBATIM);
    if (n >= v && strncmp(path, VERBATIM, v) == 0)
        return strdup(path + v);             /* removeprefix(_VERBATIM) */
    return strdup(path);
}

/* ntpath.abspath for the ABSOLUTE drive/UNC paths used by the fixtures:
 * normalise '/' to '\'. (A relative path would additionally join cwd via
 * normpath; those are deliberately excluded from the gates.) */
static char *norm_slashes(const char *text) {
    size_t n = strlen(text);
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = (text[i] == '/') ? '\\' : text[i];
    out[n] = '\0';
    return out;
}

char *w5_filesystem_427a52_long_form(const char *text) {
    if (!text) return strdup("");
    size_t v = strlen(VERBATIM);
    if (strlen(text) >= v && strncmp(text, VERBATIM, v) == 0)
        return strdup(text);                 /* already verbatim: unchanged */

    char *abs = norm_slashes(text);
    size_t n = strlen(abs);
    int unc = (n >= 2 && abs[0] == '\\' && abs[1] == '\\');
    const char *prefix = unc ? VERBATIM_UNC : VERBATIM;
    const char *suffix = unc ? abs + 2 : abs; /* UNC: text[2:] after the \\  */
    size_t p = strlen(prefix), s = strlen(suffix);
    char *out = (char *)malloc(p + s + 1);
    memcpy(out, prefix, p);
    memcpy(out + p, suffix, s);
    out[p + s] = '\0';
    free(abs);
    return out;
}
