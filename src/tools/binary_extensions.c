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
/* PoP: tools_binary_extensions_has_binary_extension @ tools/binary_extensions.py:has_binary_extension */
/* Faithful: _has_extension_in(path, BINARY_EXTENSIONS) — lower final .suffix,
 * SQLite-sidecar base substitution, exact membership. Was: a stale 33-entry
 * table that MDIWRONGLY included ".pdf" (python deliberately EXCLUDES it) and
 * omitted most extensions + all sidecar handling. Table generated from the live
 * BINARY_EXTENSIONS (93 entries). */
static const char *BEXTS[] = {".3ds", ".7z", ".a", ".aac", ".ai", ".aiff", ".app", ".avi", ".bin", ".blend", ".bmp", ".bz2", ".class", ".dat", ".data", ".db", ".deb", ".dll", ".doc", ".docx", ".dylib", ".ear", ".eot", ".eps", ".exe", ".fig", ".fla", ".flac", ".flv", ".gif", ".gz", ".ico", ".idx", ".iso", ".jar", ".jpeg", ".jpg", ".lib", ".lockb", ".m4a", ".m4v", ".max", ".mdb", ".mkv", ".mov", ".mp3", ".mp4", ".mpeg", ".mpg", ".msi", ".node", ".o", ".obj", ".odp", ".ods", ".odt", ".ogg", ".opus", ".otf", ".png", ".ppt", ".pptx", ".psd", ".pyc", ".pyo", ".rar", ".rlib", ".rpm", ".sketch", ".so", ".sqlite", ".sqlite3", ".swf", ".tar", ".tgz", ".tif", ".tiff", ".ttf", ".war", ".wasm", ".wav", ".webm", ".webp", ".wma", ".wmv", ".woff", ".woff2", ".xd", ".xls", ".xlsx", ".xz", ".z", ".zip"};
static int _bex_in(const char *s, const char *const *set, int n) {
    for (int i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}
int tools_binary_extensions_has_binary_extension(const char *arg) {
    const char *path = arg ? arg : "";
    char suffix[64];
    const char *dot = strrchr(path, '.');
    if (!dot) suffix[0] = '\0';
    else { snprintf(suffix, sizeof suffix, "%s", dot); for (char *q = suffix; *q; q++) *q = (char)tolower((unsigned char)*q); }
    static const char *MARK[] = {"-wal", "-shm", "-journal"};
    static const char *SQLEXT[] = {".db", ".sqlite", ".sqlite3"};
    size_t sl = strlen(suffix);
    int nb = (int)(sizeof(BEXTS)/sizeof(BEXTS[0]));
    for (int i = 0; i < 3; i++) {
        size_t ml = strlen(MARK[i]);
        if (sl > ml && strcmp(suffix + sl - ml, MARK[i]) == 0) {
            char base[64];
            snprintf(base, sizeof base, "%.*s", (int)(sl - ml), suffix);
            if (_bex_in(base, SQLEXT, 3)) return _bex_in(base, BEXTS, nb);
        }
    }
    return _bex_in(suffix, BEXTS, nb);
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
