/*
 * binary_extensions.c — Name parity wrapper for Python tools/binary_extensions.py
 *
 * NOTE: The C implementation lives in lib/libbinary/binary.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/binary_extensions.py.
 * C implementation: lib/libbinary/binary.c
 *
 * Binary file extension detection
 *
 * Key functions ported:
 *   is_binary_extension(), is_text_extension()
 */
#include "hermes_core_types.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_binary_extensions_has_binary_extension @ tools/binary_extensions.py:has_binary_extension */
int tools_binary_extensions_has_binary_extension(const char *arg) {
    /* Python: path[dot:].lower() in BINARY_EXTENSIONS (no I/O).
     * Arg = file path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *dot = strrchr(arg, '.');
    if (!dot) { printf("0\n"); return 0; }
    static const char *exts[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".ico", ".tiff",
        ".mp4", ".mov", ".avi", ".mkv", ".webm", ".mp3", ".wav", ".ogg",
        ".flac", ".m4a", ".pdf", ".zip", ".gz", ".tar", ".7z", ".rar",
        ".exe", ".dll", ".so", ".dylib", ".bin", ".dat", ".class", ".pyc"
    };
    size_t n = strlen(dot);
    char buf[16];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, dot, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    int hit = 0;
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (strcmp(buf, exts[i]) == 0) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: is_pdf_path @ tools/binary_extensions.py:is_pdf_path */
int tools_binary_extensions_is_pdf_path(const char *path) {
    /* Python: return path.lower().endswith(".pdf") */
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 4) return 0;
    const char *ext = path + len - 4;
    return (ext[0] == '.' && (ext[1] == 'p' || ext[1] == 'P') &&
            (ext[2] == 'd' || ext[2] == 'D') && (ext[3] == 'f' || ext[3] == 'F'));
}
