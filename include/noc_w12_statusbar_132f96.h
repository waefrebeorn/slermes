#ifndef NOC_W12_STATUSBAR_132F96_H
#define NOC_W12_STATUSBAR_132F96_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_statusbar_132f96_branch_label @ hermes_cli/status_bar_git.py:current_git_branch
 * The HEAD -> status-bar label rule from current_git_branch(): trim, then
 * `ref:` -> branch name (refs/heads/ stripped, otherwise the last path
 * segment); a detached HEAD renders as its first 8 codepoints + U+2026.
 * `head` is the already-read .git/HEAD text; the filesystem walk lives in the
 * caller.  Writes UTF-8 into out. */
void w12_statusbar_132f96_branch_label(const char *head, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_STATUSBAR_132F96_H */
