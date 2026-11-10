/*
 * port_worktree_ops.c
 *
 * Pure helpers ported from hermes_cli/worktree_ops.py (50 defs, previously 0
 * ported).
 *
 *   wo_normalize_bash_path <- _normalize_git_bash_path
 *   wo_within_root         <- _path_is_within_root
 *   wo_include_entries     <- _worktreeinclude_entries
 *
 * The git execution / worktree setup / maintenance half stays in Python.
 */

#include "port_worktree_ops.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _normalize_git_bash_path  (worktree_ops.py:_normalize_git_bash_path)
 *
 *      if not p or sys.platform != "win32": return p
 *      m = re.match(r"^/(?:(?:cygdrive|mnt)/)?([a-zA-Z])/(.*)$", p)
 *      if m: return f"{m.group(1).upper()}:\\{m.group(2).replace('/', chr(92))}"
 *      return p
 *
 *  Translate a Git Bash path (/c/.., /cygdrive/c/.., /mnt/c/..) to C:\.. on
 *  Windows. The prefix is OPTIONAL, so both "/c/x" and "/cygdrive/c/x" map.
 *  The drive letter is UPPER-CASED and the remaining separators become "\".
 *  `is_windows` stands in for sys.platform != "win32". Caller frees.
 * =========================================================================== */

/* PoP: _normalize_git_bash_path @ hermes_cli/worktree_ops.py:_normalize_git_bash_path */
char *wo_normalize_bash_path(const char *p, int is_windows)
{
    if (!p || !is_windows) return p ? strdup(p) : NULL;

    /* ^/(?:(?:cygdrive|mnt)/)?([a-zA-Z])/(.*)$ - hand-rolled, no regex needed */
    const char *q = p;
    if (*q != '/') return strdup(p);
    q++;

    if (strncmp(q, "cygdrive/", 9) == 0) q += 9;
    else if (strncmp(q, "mnt/", 4) == 0) q += 4;

    if (!isalpha((unsigned char)*q)) return strdup(p);
    char drive = (char)toupper((unsigned char)*q);
    q++;
    if (*q != '/') return strdup(p);
    q++;

    /* f"{DRIVE}:\\{rest with / -> \\}" */
    size_t n = strlen(q);
    char *out = (char *)malloc(n + 4);
    if (!out) return NULL;
    out[0] = drive;
    out[1] = ':';
    out[2] = '\\';
    for (size_t i = 0; i < n; i++) out[3 + i] = (q[i] == '/') ? '\\' : q[i];
    out[3 + n] = '\0';
    return out;
}

/* ===========================================================================
 *  _path_is_within_root  (worktree_ops.py:_path_is_within_root)
 *
 *      try: path.relative_to(root); return True
 *      except ValueError: return False
 *
 *  True when a resolved path stays within the expected root. `path` and `root`
 *  are ALREADY resolved by the caller (the live fn resolves before calling).
 *  A path that merely SHARES A PREFIX is not within the root: "C:/a/bc" is not
 *  within "C:/a/b".
 * =========================================================================== */

/* PoP: _path_is_within_root @ hermes_cli/worktree_ops.py:_path_is_within_root */
int wo_within_root(const char *path, const char *root)
{
    if (!path || !root) return 0;

    /* Path.relative_to compares CANONICAL paths, so both sides must be
     * normalized: lower-case and / -> \ (os.path.normcase on Windows). */
    size_t pl = strlen(path), rl = strlen(root);
    char *pc = (char *)malloc(pl + 1);
    char *rc = (char *)malloc(rl + 1);
    if (!pc || !rc) { free(pc); free(rc); return 0; }
    for (size_t i = 0; i < pl; i++) {
        char ch = path[i];
        if (ch == '/') ch = '\\';
        pc[i] = (char)tolower((unsigned char)ch);
    }
    pc[pl] = '\0';
    for (size_t i = 0; i < rl; i++) {
        char ch = root[i];
        if (ch == '/') ch = '\\';
        rc[i] = (char)tolower((unsigned char)ch);
    }
    rc[rl] = '\0';

    int ok = 0;
    if (strncmp(pc, rc, rl) == 0)
        ok = (pc[rl] == '\0' || pc[rl] == '\\' || pc[rl] == '/');
    free(pc);
    free(rc);
    return ok;
}

/* ===========================================================================
 *  _worktreeinclude_entries  (worktree_ops.py:_worktreeinclude_entries)
 *
 *      for line in text.splitlines():
 *          entry = line.strip()
 *          if entry and not entry.startswith("#"): entries.append(entry)
 *
 *  Non-blank, non-comment entries of .worktreeinclude ([] when absent). The
 *  file is read as utf-8-sig (a Notepad BOM would otherwise glue to the first
 *  entry), so the port strips a leading BOM from the FIRST line. `lines` is a
 *  NULL-terminated array; `had_bom` says whether the file began with a BOM.
 *  Caller frees each string and the array.
 * =========================================================================== */

/* PoP: _worktreeinclude_entries @ hermes_cli/worktree_ops.py:_worktreeinclude_entries */
char **wo_include_entries(const char *const *lines, int had_bom, int *out_count)
{
    int nl = 0;
    if (lines) while (lines[nl]) nl++;

    char **out = (char **)calloc((size_t)nl + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int n = 0;
    for (int i = 0; i < nl; i++) {
        const char *line = lines[i] ? lines[i] : "";

        /* utf-8-sig: strip a BOM from the first line */
        if (i == 0 && had_bom &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF)
            line += 3;

        /* .strip() */
        const char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                           p[len - 1] == '\r' || p[len - 1] == '\n')) len--;

        if (len == 0) continue;          /* blank */
        if (p[0] == '#') continue;       /* comment */

        char *entry = (char *)malloc(len + 1);
        if (entry) {
            memcpy(entry, p, len);
            entry[len] = '\0';
            out[n++] = entry;
        }
    }

    if (out_count) *out_count = n;
    return out;
}
