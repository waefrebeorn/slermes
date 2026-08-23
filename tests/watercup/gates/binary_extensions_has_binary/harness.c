/* gate: binary_extensions_has_binary — traced copy of
 * lib/libbinary/binary.c::has_binary_extension
 * (LIVE twin: tools/binary_extensions.py::has_binary_extension).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty path                  -> false
 *   B2: no '.' in path              -> false
 *   V1: lowercased extension from last dot
 *   B3: extension in BINARY_EXTENSIONS set -> result
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static const char *const BINARY_EXTENSIONS[] = {
    ".3ds", ".7z", ".a", ".aac", ".ai", ".aiff", ".app",
    ".avi", ".bin", ".blend", ".bmp", ".bz2",
    ".class", ".dat", ".data", ".db", ".deb", ".dll", ".doc", ".docx",
    ".dylib", ".ear", ".eot", ".eps", ".exe",
    ".fig", ".fla", ".flac", ".flv",
    ".gif", ".gz",
    ".ico", ".idx", ".iso",
    ".jar", ".jpeg", ".jpg",
    ".lib", ".lockb",
    ".m4a", ".m4v", ".max", ".mdb", ".mkv", ".mov", ".mp3", ".mp4",
    ".mpeg", ".mpg", ".msi",
    ".node",
    ".o", ".obj", ".odp", ".ods", ".odt", ".ogg", ".opus", ".otf",
    ".png", ".ppt", ".pptx", ".psd", ".pyc", ".pyo",
    ".rar", ".rlib", ".rpm",
    ".sketch", ".so", ".sqlite", ".sqlite3", ".swf",
    ".tar", ".tgz", ".tif", ".tiff", ".ttf",
    ".war", ".wasm", ".wav", ".webm", ".webp", ".wma", ".wmv",
    ".woff", ".woff2",
    ".xd", ".xls", ".xlsx", ".xz",
    ".z", ".zip",
};
#define NUM_EXTENSIONS (sizeof(BINARY_EXTENSIONS) / sizeof(BINARY_EXTENSIONS[0]))

static int ext_cmp(const void *a, const void *b) {
    const char *key = *(const char **)a;
    const char *ext = *(const char **)b;
    return strcmp(key, ext);
}

static bool wc_has_binary_extension(const char *path) {
    WCUP_BEGIN("has_binary_extension");
    bool result = false;
    char ext[64]; ext[0] = '\0';
    /* B1: empty */
    if (!path || !*path) {
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        /* B2: last dot */
        const char *dot = strrchr(path, '.');
        if (!dot) {
            WCUP_BRANCH(1);
            goto out;
        }
        WCUP_BRANCH(0);
        size_t len = 0;
        while (*dot && len < sizeof(ext) - 1) {
            char c = *dot++;
            ext[len++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        ext[len] = '\0';
    }
    WCUP_VALSTR(ext);
    {
        const char *key = ext;
        result = bsearch(&key, BINARY_EXTENSIONS, NUM_EXTENSIONS,
                         sizeof(const char *), ext_cmp) != NULL;
        WCUP_BRANCH(result);   /* B3 */
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("has_binary_extension");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_has_binary_extension <path>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_has_binary_extension(argv[i]);
    return 0;
}
