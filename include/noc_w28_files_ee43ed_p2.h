/*
 * noc_w28_files_ee43ed_p2.h - C surface for the LISTING / ENTRY SHAPING /
 * SIZE & MIME partition of upstream hermes_cli/web_routers/files.py (gate
 * w28_files_ee43ed_p2).
 *
 * What a directory listing and a file response LOOK like: the MIME and preview
 * language tables, binary sniffing, entry metadata, ordering (directories
 * first, case-insensitive), the parent link rule, the managed-files policy
 * shape, the size cap and the SSH error-code translation.
 */
#ifndef NOC_W28_FILES_EE43ED_P2_H
#define NOC_W28_FILES_EE43ED_P2_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _fs_mime_type: the module table, else application/octet-stream. */
void w28_files_ee43ed_p2_fs_mime_type(const char *path, char *out, size_t cap);

/* {requested, target, nt} -> {source, suffix, language}: the name a preview
 * takes its type from (report.html -> its extensionless target still previews
 * as HTML). The MIME value itself is fs_mime_type's table answer, kept out of
 * this shape because the system MIME database is a seam. */
json_t *w28_files_ee43ed_p2_preview_governing(const json_t *req);

/* _fs_looks_binary over a byte array: NUL, or >12% non-text control bytes. */
bool w28_files_ee43ed_p2_looks_binary(const json_t *bytes);

/* {locked, can_change_path} -> the *_managed_response_meta keys (no path text). */
json_t *w28_files_ee43ed_p2_managed_response_meta(const json_t *req);

/* {forced_root, hermes_home, default_root} -> {root_kind, locked, can_change_path}. */
json_t *w28_files_ee43ed_p2_managed_files_policy(const json_t *req);

/* {size, max_bytes} -> {ok, size, status, detail}: the 413 admission check. */
json_t *w28_files_ee43ed_p2_managed_file_size(const json_t *req);

/* {kind, name, size, mtime, locked, inside} -> a listing entry, null (vanished)
 * or the 403/500 verdict. kind: dir | file | broken | missing | outside. */
json_t *w28_files_ee43ed_p2_managed_file_entry(const json_t *req);

/* _fs_list_entries ordering: (not isDirectory, name.lower(), name), stable. */
json_t *w28_files_ee43ed_p2_fs_list_sort(const json_t *entries);

/* list_managed_files ordering: (not is_directory, name.lower()), stable. */
json_t *w28_files_ee43ed_p2_managed_list_sort(const json_t *entries);

/* The parent link: present unless target is a filesystem root or the locked root. */
bool w28_files_ee43ed_p2_list_has_parent(const char *target, const char *locked_root, int nt);

/* _FS_READDIR_HIDDEN basenames. */
bool w28_files_ee43ed_p2_is_readdir_hidden(const char *name);

/* SSH workspace error code -> {status, detail} (403/503/413/400/502/404/400). */
json_t *w28_files_ee43ed_p2_backend_error(const char *code);

/* {suffix, media_only} -> 415 unless the suffix is streamable. */
json_t *w28_files_ee43ed_p2_media_extension_verdict(const json_t *req);

const char *w28_files_ee43ed_p2_sandbox_not_found_detail(bool unmapped);

#ifdef __cplusplus
}
#endif
#endif
