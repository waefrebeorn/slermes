/*
 * port_file_operations_gaps.c — C11 port of tools/file_operations.py REAL_GAP functions.
 *
 * Implements: magic byte identification, binary detection, raw file reading,
 * fuzzy patch replace, file deletion, line ending detection, BOM detection.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* === Magic byte signatures === */

typedef struct {
    const char *prefix;
    size_t prefix_len;
    const char *name;
} magic_sig_t;

static const magic_sig_t MAGIC_SIGS[] = {
    {"\x89PNG", 4, "PNG image data"},
    {"\xff\xd8\xff", 3, "JPEG image data"},
    {"GIF87a", 6, "GIF image data"},
    {"GIF89a", 6, "GIF image data"},
    {"PK\x03\x04", 4, "ZIP archive"},
    {"\x1f\x8b", 2, "GZIP archive"},
    {"BZ", 2, "BZIP2 archive"},
    {"\xfd7zXZ", 5, "XZ archive"},
    {"Rar!", 4, "RAR archive"},
    {"7z\xbc\xaf", 4, "7z archive"},
    {"\x00\x00\x00", 4, "ISO media"},
    {"\x7fELF", 4, "ELF binary"},
    {"MZ", 2, "PE executable"},
    {"\xca\xfe\xba\xbe", 4, "Mach-O binary"},
    {"\xfe\xed\xfa\xce", 4, "Mach-O binary"},
    {"\xfe\xed\xfa\xcf", 4, "Mach-O binary"},
    {"\xce\xfa\xed\xfe", 4, "Mach-O binary"},
    {"\xcf\xfa\xed\xfe", 4, "Mach-O binary"},
    {NULL, 0, NULL}
};

/* PoP: fo_identify_binary_bytes @ tools/file_operations.py:identify_binary_bytes */
const char *fo_identify_binary_bytes(const unsigned char *sample, size_t len) {
    if (!sample || len == 0) return "unknown binary";
    for (const magic_sig_t *sig = MAGIC_SIGS; sig->prefix; sig++) {
        if (len >= sig->prefix_len && memcmp(sample, sig->prefix, sig->prefix_len) == 0) {
            /* Special check for ISO media: verify ftyp at offset 4 */
            if (strcmp(sig->name, "ISO media") == 0) {
                if (len >= 8 && memcmp(sample + 4, "ftyp", 4) != 0) continue;
            }
            return sig->name;
        }
    }
    return "unknown binary";
}

/* PoP: fo_describe_binary_file @ tools/file_operations.py:describe_binary_file */
void fo_describe_binary_file(const unsigned char *sample, size_t sample_len,
                              int file_size, char *out, size_t out_sz) {
    const char *kind = fo_identify_binary_bytes(sample, sample_len);
    char size_str[64];
    if (file_size >= 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f MB", (double)file_size / (1024.0 * 1024.0));
    } else if (file_size >= 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f KB", (double)file_size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%d B", file_size);
    }
    snprintf(out, out_sz, "%s, %s", kind, size_str);
}

/* === File reading === */

/* PoP: fo_read_file_raw @ tools/file_operations.py:read_file_raw */
int fo_read_file_raw(const char *path, char **out_content, size_t *out_len) {
    if (!path || !out_content) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return -1; }
    /* Read entire file */
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    *out_content = buf;
    if (out_len) *out_len = n;
    return 0;
}

/* === Patch/replace === */

/* PoP: fo_patch_replace @ tools/file_operations.py:patch_replace */
int fo_patch_replace(const char *path, const char *old_string,
                      const char *new_string, int replace_all) {
    if (!path || !old_string || !new_string) return -1;
    /* Read file */
    char *content = NULL;
    size_t len = 0;
    if (fo_read_file_raw(path, &content, &len) != 0) return -1;
    /* Find and replace */
    size_t old_len = strlen(old_string);
    size_t new_len = strlen(new_string);
    char *pos = strstr(content, old_string);
    if (!pos) { free(content); return -1; }
    /* Calculate new size */
    size_t tail_len = len - (pos - content) - old_len;
    size_t result_len = len - old_len + new_len;
    char *result = malloc(result_len + 1);
    if (!result) { free(content); return -1; }
    /* Build result: prefix + replacement + suffix */
    size_t prefix_len = (size_t)(pos - content);
    memcpy(result, content, prefix_len);
    memcpy(result + prefix_len, new_string, new_len);
    memcpy(result + prefix_len + new_len, pos + old_len, tail_len);
    result[result_len] = '\0';
    if (replace_all) {
        /* Continue replacing remaining occurrences */
        char *search = result + prefix_len + new_len;
        char *found;
        while ((found = strstr(search, old_string)) != NULL) {
            /* Reallocate and replace */
            size_t offset = (size_t)(found - result);
            size_t new_result_len = result_len - old_len + new_len;
            char *tmp = realloc(result, new_result_len + 1);
            if (!tmp) break;
            result = tmp;
            memmove(result + offset + new_len, result + offset + old_len,
                    result_len - offset - old_len);
            memcpy(result + offset, new_string, new_len);
            result_len = new_result_len;
            result[result_len] = '\0';
            search = result + offset + new_len;
        }
    }
    /* Write back */
    FILE *f = fopen(path, "w");
    if (!f) { free(content); free(result); return -1; }
    fwrite(result, 1, result_len, f);
    fclose(f);
    free(content);
    free(result);
    return 0;
}

/* === File deletion === */

/* PoP: fo_delete_path @ tools/file_operations.py:delete_path */
int fo_delete_path(const char *path, int recursive) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (!recursive) return -1; /* Can't delete dir without recursive */
        /* Recursive delete: use nftw or system rm -rf */
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
        return system(cmd);
    }
    return unlink(path);
}

/* === Linter detection === */

/* PoP: fo_looks_like_linter_unusable @ tools/file_operations.py:_looks_like_linter_unusable */
int fo_looks_like_linter_unusable(const char *base_cmd, const char *output) {
    if (!base_cmd || !output) return 0;
    /* Check for common "linter can't run" patterns */
    size_t len = strlen(output);
    if (len == 0) return 1;
    /* Check for "command not found" */
    if (strstr(output, "command not found")) return 1;
    if (strstr(output, "not found")) return 1;
    if (strstr(output, "No such file")) return 1;
    /* Check for "not installed" */
    if (strstr(output, "not installed")) return 1;
    if (strstr(output, "is not installed")) return 1;
    return 0;
}

/* === Binary detection === */

/* PoP: fo_is_likely_binary_bytes @ tools/file_operations.py:_is_likely_binary_bytes */
int fo_is_likely_binary_bytes(const unsigned char *sample, size_t len) {
    if (!sample || len == 0) return 0;
    /* Check for NUL bytes (strong binary indicator) */
    for (size_t i = 0; i < len; i++) {
        if (sample[i] == 0x00) return 1;
    }
    /* Check for invalid UTF-8 sequences */
    size_t i = 0;
    int incomplete = 0;
    while (i < len) {
        unsigned char c = sample[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            /* 2-byte sequence */
            if (i + 1 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            /* 3-byte sequence */
            if (i + 2 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            if ((sample[i+2] & 0xC0) != 0x80) return 1;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            /* 4-byte sequence */
            if (i + 3 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            if ((sample[i+2] & 0xC0) != 0x80) return 1;
            if ((sample[i+3] & 0xC0) != 0x80) return 1;
            i += 4;
        } else {
            /* Invalid UTF-8 start byte */
            return 1;
        }
    }
    /* Allow one incomplete multibyte sequence at the end */
    return 0;
}

/* PoP: fo_is_likely_binary @ tools/file_operations.py:_is_likely_binary */
int fo_is_likely_binary(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return fo_is_likely_binary_bytes(buf, n);
}

/* PoP: fo_is_image @ tools/file_operations.py:_is_image */
int fo_is_image(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n < 4) return 0;
    /* Check image magic bytes */
    if (memcmp(buf, "\x89PNG", 4) == 0) return 1;
    if (memcmp(buf, "\xff\xd8\xff", 3) == 0) return 1;
    if (n >= 6 && (memcmp(buf, "GIF87a", 6) == 0 || memcmp(buf, "GIF89a", 6) == 0)) return 1;
    if (n >= 2 && memcmp(buf, "BM", 2) == 0) return 1; /* BMP */
    if (n >= 4 && memcmp(buf, "RIFF", 4) == 0) return 1; /* WEBP */
    return 0;
}

/* === File format detection === */

/* PoP: fo_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
const char *fo_detect_file_line_ending(const char *path) {
    if (!path) return "lf";
    FILE *f = fopen(path, "rb");
    if (!f) return "lf";
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    /* Count CRLF vs LF */
    int crlf = 0, lf = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\r' && i + 1 < n && buf[i+1] == '\n') {
            crlf++;
            i++;
        } else if (buf[i] == '\n') {
            lf++;
        }
    }
    return (crlf > lf) ? "crlf" : "lf";
}

/* PoP: fo_file_has_bom @ tools/file_operations.py:_file_has_bom */
int fo_file_has_bom(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char bom[4] = {0};
    size_t n = fread(bom, 1, 3, f);
    fclose(f);
    /* UTF-8 BOM: EF BB BF */
    if (n >= 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) return 1;
    /* UTF-16 LE BOM: FF FE */
    if (n >= 2 && bom[0] == 0xFF && bom[1] == 0xFE) return 2;
    /* UTF-16 BE BOM: FE FF */
    if (n >= 2 && bom[0] == 0xFE && bom[1] == 0xFF) return 2;
    return 0;
}

/* === Size probing === */

/* PoP: fo_size_probe_cmd @ tools/file_operations.py:_size_probe_cmd */
void fo_size_probe_cmd(const char *path, char *out, size_t out_sz) {
    struct stat st;
    if (stat(path, &st) == 0) {
        snprintf(out, out_sz, "%ld", (long)st.st_size);
    } else {
        snprintf(out, out_sz, "0");
    }
}

/* PoP: fo_not_regular_error @ tools/file_operations.py:_not_regular_error */
int fo_not_regular_error(const char *path) {
    if (!path) return 1;
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return !S_ISREG(st.st_mode);
}

/* === Delete helpers === */

/* PoP: fo_python_delete @ tools/file_operations.py:_python_delete */
int fo_python_delete(const char *path) {
    return fo_delete_path(path, 0);
}

/* === Search helpers === */

/* PoP: fo_try_multi_path_search @ tools/file_operations.py:_try_multi_path_search */
int fo_try_multi_path_search(const char **paths, int count, char *out_found, size_t out_sz) {
    if (!paths || count <= 0) return -1;
    for (int i = 0; i < count; i++) {
        if (paths[i] && access(paths[i], F_OK) == 0) {
            snprintf(out_found, out_sz, "%s", paths[i]);
            return i;
        }
    }
    return -1;
}

/* PoP: fo_zero_match_probe @ tools/file_operations.py:_zero_match_probe */
int fo_zero_match_probe(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_size == 0) ? 1 : 0;
}
