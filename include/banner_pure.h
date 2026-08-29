/* banner_pure.h — Port of hermes_cli/banner.py pure helpers.
 * Deterministic string/URL/format functions (no git, no subprocess, no TTY).
 */
#ifndef SLERMES_BANNER_PURE_H
#define SLERMES_BANNER_PURE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _canonical_github_remote @ hermes_cli/banner.py:_canonical_github_remote */
/* Return "host/owner/repo" for common GitHub remote URL forms. Caller frees. */
char *banner_canonical_github_remote(const char *url);

/* PoP: _is_ssh_remote @ hermes_cli/banner.py:_is_ssh_remote */
bool banner_is_ssh_remote(const char *url);

/* PoP: _is_official_ssh_remote @ hermes_cli/banner.py:_is_official_ssh_remote */
bool banner_is_official_ssh_remote(const char *url);

/* PoP: _format_context_length @ hermes_cli/banner.py:_format_context_length */
/* Format a token count: 128000 -> "128K", 1048576 -> "1M". Caller frees. */
char *banner_format_context_length(long long tokens);

/* PoP: _display_toolset_name @ hermes_cli/banner.py:_display_toolset_name */
/* Normalize internal toolset ids: strip trailing "_tools". Caller frees. */
char *banner_display_toolset_name(const char *toolset_name);

/* ── Git/update/snapshot surface (banner.py) ──────────────────────── */
bool banner_is_full_sha(const char *value);
void banner_snapshot_fingerprint(char *out_fp, size_t out_sz);
void banner_save_snapshot(const char *tools_json, const char *enabled_toolsets_json,
                          const char *availability_json, const char *toolset_map_json,
                          const char *skills_by_category_json);
void banner_compute_git_banner_state(char *out_json, size_t out_sz);
char *banner_resolve_repo_dir(void);
char *banner_git_short_hash(const char *repo_dir, const char *rev);

/* PoP: build_info_get_build_sha @ hermes_cli/build_info.py:get_build_sha */
char *build_info_get_build_sha(int short_len);

int  banner_check_for_updates(void);
void banner_prefetch_update_check(void);
int  banner_get_update_result(double timeout_seconds);
void banner_compute_toolset_availability(const char *enable_toolsets_json,
                                         char *out_json, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BANNER_PURE_H */
