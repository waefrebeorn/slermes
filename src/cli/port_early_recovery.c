/*
 * port_early_recovery.c
 *
 * Pure helpers ported from hermes_cli/_early_recovery.py (64 defs, previously
 * 0 ported). The module is the post-killed-update repair path.
 *
 *   er_is_object_id        <- is_object_id
 *   er_git_dir             <- _git_dir
 *   er_recovery_closure_dir<- recovery_closure_dir
 *   er_interrupted_marker  <- interrupted_pull_marker
 *
 * The filesystem move / backoff half stays in Python; the path construction
 * and the commit-id check are here.
 */

#include "port_early_recovery.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* _early_recovery.py:RECOVERY_CLOSURE_DIR */
#define ER_RECOVERY_CLOSURE_DIR "hermes-update-recovery"

/* _early_recovery.py:INTERRUPTED_PULL_MARKER */
#define ER_INTERRUPTED_PULL_MARKER "hermes-update-pull"

/* ===========================================================================
 *  is_object_id  (_early_recovery.py:is_object_id)
 *
 *      return bool(re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", value or ""))
 *
 *  A git blob/commit id: exactly 40 (sha1) or 64 (sha256) lowercase hex
 *  characters, and NOTHING else. `re.fullmatch` anchors both ends, so
 *  "abc...def\n" does not match and neither does a 41-char id.
 * =========================================================================== */

/* PoP: is_object_id @ hermes_cli/_early_recovery.py:is_object_id */
int er_is_object_id(const char *value)
{
    const char *s = (value && value[0]) ? value : "";
    size_t n = strlen(s);
    if (n != 40 && n != 64) return 0;
    for (size_t i = 0; i < n; i++)
        if (!isxdigit((unsigned char)s[i]) || isupper((unsigned char)s[i])) return 0;
    return 1;
}

/* ===========================================================================
 *  _git_dir  (_early_recovery.py:_git_dir)
 *
 *      dot_git = root / ".git"
 *      if dot_git.is_file():
 *          text = dot_git.read_text(encoding="utf-8-sig").strip()
 *          if text.startswith("gitdir:"):
 *              return root / text[len("gitdir:"):].strip()
 *      return dot_git
 *
 *  A linked worktree's `.git` is a FILE containing "gitdir: <path>" rather than
 *  a directory. `dot_git_is_file` and `dot_git_text` stand in for the stat and
 *  the read; when `.git` is a directory both are ignored. Caller frees.
 * =========================================================================== */

/* PoP: _git_dir @ hermes_cli/_early_recovery.py:_git_dir */
char *er_git_dir(const char *root, int dot_git_is_file, const char *dot_git_text)
{
    size_t cap = strlen(root) + 16;
    char *dot = (char *)malloc(cap);
    if (!dot) return NULL;
    snprintf(dot, cap, "%s/.git", root);

    if (dot_git_is_file && dot_git_text) {
        /* utf-8-sig decode strips a leading BOM */
        const char *text = dot_git_text;
        if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
            (unsigned char)text[2] == 0xBF) text += 3;
        while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
        size_t tl = strlen(text);
        while (tl > 0 && (text[tl-1] == ' ' || text[tl-1] == '\t' ||
                          text[tl-1] == '\n' || text[tl-1] == '\r')) tl--;
        if (strncmp(text, "gitdir:", 7) == 0) {
            const char *target = text + 7;
            while (*target == ' ' || *target == '\t') target++;
            size_t gl = strlen(target);
            while (gl > 0 && (target[gl-1] == ' ' || target[gl-1] == '\t' ||
                              target[gl-1] == '\n' || target[gl-1] == '\r')) gl--;
            size_t cap2 = strlen(root) + gl + 2;
            char *out = (char *)malloc(cap2);
            if (!out) { free(dot); return NULL; }
            snprintf(out, cap2, "%s/%.*s", root, (int)gl, target);
            free(dot);
            return out;
        }
    }
    return dot;
}

/* ===========================================================================
 *  recovery_closure_dir  (_early_recovery.py:recovery_closure_dir)
 *
 *      if not is_object_id(pre):
 *          raise ValueError(f"not a commit id: {pre!r}")
 *      return _git_dir(root) / RECOVERY_CLOSURE_DIR / pre
 *
 *  The check is load-bearing: `pre` is interpolated into a path, so a revision
 *  name ("HEAD") or a relative path ("../x") must be rejected outright.
 *  `dot_git_is_file` / `dot_git_text` stand in for the `.git` stat and read.
 * =========================================================================== */

/* PoP: recovery_closure_dir @ hermes_cli/_early_recovery.py:recovery_closure_dir */
char *er_recovery_closure_dir(const char *root, const char *pre,
                              int dot_git_is_file, const char *dot_git_text,
                              char **err)
{
    if (err) *err = NULL;
    if (!er_is_object_id(pre)) {
        if (err) {
            size_t n = strlen(pre ? pre : "") + 32;
            *err = (char *)malloc(n);
            if (*err) snprintf(*err, n, "not a commit id: '%s'", pre ? pre : "");
        }
        return NULL;
    }
    char *git = er_git_dir(root, dot_git_is_file, dot_git_text);
    if (!git) return NULL;
    size_t cap = strlen(git) + strlen(ER_RECOVERY_CLOSURE_DIR) + strlen(pre) + 3;
    char *out = (char *)malloc(cap);
    if (!out) { free(git); return NULL; }
    snprintf(out, cap, "%s/%s/%s", git, ER_RECOVERY_CLOSURE_DIR, pre);
    free(git);
    return out;
}

/* ===========================================================================
 *  interrupted_pull_marker  (_early_recovery.py:interrupted_pull_marker)
 *
 *      return _git_dir(root) / INTERRUPTED_PULL_MARKER
 *
 *  Caller frees.
 * =========================================================================== */

/* PoP: interrupted_pull_marker @ hermes_cli/_early_recovery.py:interrupted_pull_marker */
char *er_interrupted_marker(const char *root, int dot_git_is_file, const char *dot_git_text)
{
    char *git = er_git_dir(root, dot_git_is_file, dot_git_text);
    if (!git) return NULL;
    size_t cap = strlen(git) + strlen(ER_INTERRUPTED_PULL_MARKER) + 2;
    char *out = (char *)malloc(cap);
    if (!out) { free(git); return NULL; }
    snprintf(out, cap, "%s/%s", git, ER_INTERRUPTED_PULL_MARKER);
    free(git);
    return out;
}
