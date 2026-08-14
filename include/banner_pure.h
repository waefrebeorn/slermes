/* banner_pure.h — Port of hermes_cli/banner.py pure helpers.
 * Deterministic string/URL/format functions (no git, no subprocess, no TTY).
 */
#ifndef SLERMES_BANNER_PURE_H
#define SLERMES_BANNER_PURE_H

#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BANNER_PURE_H */
