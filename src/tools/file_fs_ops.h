#ifndef SLERMES_FILE_FS_OPS_H
#define SLERMES_FILE_FS_OPS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * file_fs_ops — filesystem read / write / type-detection helpers, ported from
 * tools/file_operations.py. Focused module (extracted from port_file_operations.c
 * in the v553 monolith split) so file-IO concerns live in their own translation
 * unit with no god-file coupling.
 *
 * Public API (each has a /* PoP: c @ tools/file_operations.py:_py *​/ in the .c):
 *   file_fs_ops_read_file_raw        — read file bytes (POSIX)
 *   file_fs_ops_delete_path          — unlink/rmdir with write-deny guard
 *   file_fs_ops_python_delete        — alias of delete_path
 *   file_fs_ops_patch_replace        — first-occurrence string replace
 *   file_fs_ops_is_likely_binary      — ext set + >30% non-printable heuristic
 *   file_fs_ops_is_image             — IMAGE_EXTENSIONS check
 *   file_fs_ops_detect_file_line_ending — read + _detect_line_ending
 *   file_fs_ops_file_has_bom         — read first 3 bytes for UTF-8 BOM
 */

/* PoP: fo_read_file_raw @ tools/file_operations.py:read_file_raw */
char *file_fs_ops_read_file_raw(const char *path);

/* PoP: fo_delete_path @ tools/file_operations.py:delete_path */
bool file_fs_ops_delete_path(const char *path);

/* PoP: fo_python_delete @ tools/file_operations.py:_python_delete */
bool file_fs_ops_python_delete(const char *path);

/* PoP: fo_patch_replace @ tools/file_operations.py:patch_replace */
/* (primitive: first-occurrence replace; Python's full method is fuzzy + guarded) */
char *file_fs_ops_patch_replace(const char *content, const char *old_text,
                                const char *new_text);

/* PoP: fo_is_likely_binary @ tools/file_operations.py:_is_likely_binary */
bool file_fs_ops_is_likely_binary(const char *path);

/* PoP: fo_is_image @ tools/file_operations.py:_is_image */
bool file_fs_ops_is_image(const char *path);

/* PoP: file_fs_ops_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
char *file_fs_ops_detect_file_line_ending(const char *path);

/* PoP: fo_file_has_bom @ tools/file_operations.py:_file_has_bom */
bool file_fs_ops_file_has_bom(const char *path);

/* PoP: identify_binary_bytes @ tools/file_operations.py:identify_binary_bytes */
/* Best-effort human name for binary content from magic bytes ("PNG image
 * data" / "unknown binary"). Caller frees. */
char *file_fs_ops_identify_binary_bytes(const unsigned char *sample, size_t len);

/* PoP: fo_describe_binary_file @ tools/file_operations.py:describe_binary_file */
/* One-line binary refusal: "Binary file (<kind>, <size>) — cannot display
 * as text." Caller frees. */
char *file_fs_ops_describe_binary_file(const unsigned char *sample, size_t len,
                                       long long file_size);

/* PoP: fo_is_likely_binary_bytes @ tools/file_operations.py:_is_likely_binary_bytes */
/* Byte-layer binary detection: valid UTF-8 (allowing one incomplete multibyte
 * sequence at the very end) is text; NULs / invalid UTF-8 are binary. */
bool file_fs_ops_is_likely_binary_bytes(const unsigned char *sample, size_t len);

/* PoP: fo_not_regular_error @ tools/file_operations.py:_not_regular_error */
/* Error text for a path that exists but would block if read. Caller frees. */
char *file_fs_ops_not_regular_error(const char *path);

/* PoP: fo_size_probe_cmd @ tools/file_operations.py:_size_probe_cmd */
/* Shell command to get a regular file's byte size without opening it
 * (avoids FIFO/socket/devices that never reach EOF). Caller frees. */
char *file_fs_ops_size_probe_cmd(const char *path);

#endif /* SLERMES_FILE_FS_OPS_H */
