/*
 * noc_w28_files_ee43ed_p1.h - C surface for the PATH SCOPING & SANITISATION
 * partition of upstream hermes_cli/web_routers/files.py (gate
 * w28_files_ee43ed_p1).
 *
 * What a request path may NAME, before any byte moves: the credential
 * basename/component denylists, the home-store resolution, the chat-image
 * filename sanitiser, ``file:`` URL decoding, the container-path hints, the
 * managed-root containment verdict and the ``If-None-Match`` tag parser.
 * Every filesystem answer (stat, realpath, scandir) arrives as a fixture flag;
 * every status code returns as data instead of an exception.
 */
#ifndef NOC_W28_FILES_EE43ED_P1_H
#define NOC_W28_FILES_EE43ED_P1_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Basename denylist: .env / .env.<suffix> / .envrc + the credential-store names. */
bool w28_files_ee43ed_p1_is_sensitive_filename(const char *name);

/* Basename denylist OR a credential-directory component run anywhere in the path. */
bool w28_files_ee43ed_p1_is_sensitive_path(const char *path);

/* agent.file_safety.deny_path_under: equal, or below base after stripping a trailing sep. */
bool w28_files_ee43ed_p1_deny_path_under(const char *resolved, const char *base, int nt);

/* {path, real, files[], dirs[], nt} -> sensitive-by-name OR resolved into a store. */
bool w28_files_ee43ed_p1_is_local_sensitive_path(const json_t *req);

/* _sanitize_chat_image_filename -> basename, control runs -> '_', trim, or "pasted-image". */
void w28_files_ee43ed_p1_sanitize_filename(const char *filename, int nt, char *out, size_t cap);

/* The upload stem: Path(sane).stem, re.sub([^A-Za-z0-9_.-]+ -> "_"), strip("._-"). */
void w28_files_ee43ed_p1_filename_stem(const char *filename, int nt, char *out, size_t cap);

/* {raw, nul} -> {ok, text, status, detail}: strip, then refuse U+0000. */
json_t *w28_files_ee43ed_p1_path_text(const json_t *req);

/* _path_is_under: target == root or root in target.parents (folded when nt). */
bool w28_files_ee43ed_p1_path_is_under(const char *root, const char *target, int nt);

/* {url, nt} -> {ok, path, status, detail}: decodes once; refuses %2f (and %5c on nt). */
json_t *w28_files_ee43ed_p1_file_url_path(const json_t *req);

/* A container path the translator may be asked about: leading '/', no '//', no '..'. */
bool w28_files_ee43ed_p1_agent_container_translatable(const char *raw);

/* {raw, docker_active, msys_same, roots[]} -> an unmapped docker container path. */
bool w28_files_ee43ed_p1_unmapped_container_path(const json_t *req);

/* {text, root, default_path, locked, for_write, exists, nt} ->
 * {ok, under_root, status, detail} — _resolve_managed_path's refusals only. */
json_t *w28_files_ee43ed_p1_managed_path_verdict(const json_t *req);

/* If-None-Match -> the tag set, sorted (set semantics, so order is canonical). */
json_t *w28_files_ee43ed_p1_if_none_match(const char *header);

/* True when the header names the etag or '*': the 304 branch. */
bool w28_files_ee43ed_p1_if_none_match_hit(const char *header, const char *etag);

#ifdef __cplusplus
}
#endif
#endif
