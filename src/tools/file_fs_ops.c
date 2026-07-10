/*
 * file_fs_ops.c — filesystem read / write / type-detection helpers,
 * ported from tools/file_operations.py.
 *
 * Faithful to the Python source where the C primitive maps 1:1:
 *  - is_image        mirrors _is_image (IMAGE_EXTENSIONS set, case-insensitive)
 *  - is_likely_binary mirrors _is_likely_binary (BINARY_EXTENSIONS ext
 *                       check OR >30% non-printable in first 1000 bytes)
 *  - patch_replace   is the first-occurrence string-replace primitive
 *                       (Python's full patch_replace is fuzzy + guarded; this is
 *                       the lower-level building block it delegates the actual
 *                       byteswap to)
 *  - detect_file_line_ending / file_has_bom are thin wrappers that read
 *                       the on-disk file then delegate to the stateless
 *                       file_text_ops._detect_line_ending / _has_bom (already
 *                       oracle-verified 1:1 in v551)
 *  - read_file_raw  POSIX fread of the whole file (Python uses `cat`)
 *  - delete_path     unlink/rmdir with the is_write_denied guard
 *                       (Python: delete_path -> _python_delete ->
 *                       _is_write_denied returns WriteResult(error=...);
 *                       the C backend is POSIX-only so it returns bool,
 *                       false == denied, matching the error-carrying result)
 *
 * Oracle-verified against LIVE tools/file_operations.py (see
 * tests/sta_oracle_file_fs_ops.py).
 */

#include "file_fs_ops.h"
#include "file_text_ops.h"
#include "hermes_file_safety.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* ---- BINARY_EXTENSIONS (tools/binary_extensions.py, ported frozen set) ---- */
static const char *BINARY_EXT[] = {
    /* images */ ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".webp", ".tiff", ".tif",
    /* video */  ".mp4", ".mov", ".avi", ".mkv", ".webm", ".wmv", ".flv", ".m4v", ".mpeg", ".mpg",
    /* audio */  ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a", ".wma", ".aiff", ".opus",
    /* archives */ ".zip", ".tar", ".gz", ".bz2", ".7z", ".rar", ".xz", ".z", ".tgz", ".iso",
    /* executables */ ".exe", ".dll", ".so", ".dylib", ".bin", ".o", ".a", ".obj", ".lib",
    ".app", ".msi", ".deb", ".rpm",
    /* documents (exclude .pdf — text, agents inspect it) */
    ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
    ".odt", ".ods", ".odp",
    /* fonts */ ".ttf", ".otf", ".woff", ".woff2", ".eot",
    /* bytecode / vm */ ".pyc", ".pyo", ".class", ".jar", ".war", ".ear", ".node", ".wasm", ".rlib",
    /* databases */ ".sqlite", ".sqlite3", ".db", ".mdb", ".idx",
    /* design / 3d */ ".psd", ".ai", ".eps", ".sketch", ".fig", ".xd", ".blend", ".3ds", ".max",
    /* flash */ ".swf", ".fla",
    /* lock / profiling */ ".lockb", ".dat", ".data",
    NULL
};

/* ---- IMAGE_EXTENSIONS (tools/file_operations.py:500) ---- */
static const char *IMAGE_EXT[] = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".webp", NULL
};

/* PoP: file_fs_ops_read_file_raw @ tools/file_operations.py:read_file_raw */
char *file_fs_ops_read_file_raw(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)sz, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

/* PoP: file_fs_ops_delete_path @ tools/file_operations.py:delete_path
 * Faithful: Python's delete_path -> _python_delete first calls
 * _is_write_denied(path) and returns WriteResult(error="Delete denied: ...")
 * for protected paths. POSIX-only backend (no Windows rm), so it
 * unlinks a file or rmdirs an empty dir; a denied path returns false. */
bool file_fs_ops_delete_path(const char *path)
{
    if (!path || !*path) return false;
    if (is_write_denied(path)) {
        hermes_log(2, "delete_path denied by write-deny list: %s", path);
        return false;
    }
    return unlink(path) == 0 || rmdir(path) == 0;
}

/* PoP: file_fs_ops_python_delete @ tools/file_operations.py:_python_delete */
bool file_fs_ops_python_delete(const char *path)
{
    return file_fs_ops_delete_path(path);
}

/* PoP: file_fs_ops_patch_replace @ tools/file_operations.py:patch_replace
 * First-occurrence replace primitive (Python's full method is fuzzy + guarded). */
char *file_fs_ops_patch_replace(const char *content, const char *old_text,
                                 const char *new_text)
{
    if (!content || !old_text || !new_text) return strdup(content ? content : "");
    const char *pos = strstr(content, old_text);
    if (!pos) return strdup(content);

    size_t before_len = (size_t)(pos - content);
    size_t old_len = strlen(old_text);
    size_t new_len = strlen(new_text);
    size_t rest_len = strlen(content) - before_len - old_len;
    size_t result_len = before_len + new_len + rest_len + 1;

    char *result = malloc(result_len);
    if (!result) return NULL;
    memcpy(result, content, before_len);
    memcpy(result + before_len, new_text, new_len);
    memcpy(result + before_len + new_len, pos + old_len, rest_len);
    result[result_len - 1] = '\0';
    return result;
}

static const char *ext_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}

static bool in_set(const char *ext, const char **set)
{
    for (int i = 0; set[i]; i++)
        if (strcasecmp(ext, set[i]) == 0) return true;
    return false;
}

/* PoP: file_fs_ops_is_likely_binary @ tools/file_operations.py:_is_likely_binary
 * ext in BINARY_EXTENSIONS -> true; else >30% non-printable in first 1000
 * bytes -> true; else false. Mirrors the .py content-sample branch. */
bool file_fs_ops_is_likely_binary(const char *path)
{
    if (!path) return false;
    if (in_set(ext_of(path), BINARY_EXT)) return true;

    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[1000];
    size_t read = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    size_t non_printable = 0;
    for (size_t i = 0; i < read; i++) {
        unsigned char c = buf[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t')
            non_printable++;
    }
    return read > 0 && ((double)non_printable / (double)read) > 0.30;
}

/* PoP: file_fs_ops_is_image @ tools/file_operations.py:_is_image
 * IMAGE_EXTENSIONS set, case-insensitive (includes .ico). */
bool file_fs_ops_is_image(const char *path)
{
    if (!path) return false;
    const char *ext = ext_of(path);
    return in_set(ext, IMAGE_EXT);
}

/* PoP: file_fs_ops_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
char *file_fs_ops_detect_file_line_ending(const char *path)
{
    char *content = file_fs_ops_read_file_raw(path);
    if (!content) return strdup("unknown");
    char *ending = file_text_ops_detect_line_ending(content);
    free(content);
    return ending;
}

/* PoP: file_fs_ops_file_has_bom @ tools/file_operations.py:_file_has_bom */
bool file_fs_ops_file_has_bom(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char bom[3];
    size_t read = fread(bom, 1, 3, f);
    fclose(f);
    return (read == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
}
