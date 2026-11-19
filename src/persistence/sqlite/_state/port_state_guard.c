/*
 * port_state_guard.c -- Port of persistence/sqlite/_state/guard.py pure
 * predicate logic (the live-DB test-isolation guard).
 *
 *   sg_running_under_pytest        <- _running_under_pytest (env truthiness)
 *   sg_process_looks_like_pytest   <- _process_looks_like_pytest (argv analysis)
 *   sg_is_production_state_db      <- _is_production_state_db (path predicate)
 *
 * The env reads, psutil ancestry walk, WeakSet registry and the locked
 * last-init-error record stay in Python; these cores are fed the values the
 * impure callers read. Path semantics follow the LIVE Windows authority
 * (both separators, '.' dropped, case-insensitive component equality).
 */

#include "port_state_guard.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* guard.py:_TEST_ISOLATION_MARKER_ENV / PYTEST_* truthiness: os.environ.get
 * returns None when unset and "" is falsy, so any non-empty value arms it. */

/* PoP: sg_running_under_pytest @ persistence/sqlite/_state/guard.py:_running_under_pytest */
bool sg_running_under_pytest(const char *pytest_current_test,
                             const char *pytest_version,
                             const char *test_isolation_marker)
{
    return (pytest_current_test && *pytest_current_test) ||
           (pytest_version && *pytest_version) ||
           (test_isolation_marker && *test_isolation_marker);
}

/* guard.py:_PYTEST_LAUNCHER_NAMES (matched against each token's basename so
 * /tmp/pytest-of-dev/... paths cannot false-positive). */
static bool sg_launcher_name(const char *name)
{
    return strcmp(name, "pytest") == 0 || strcmp(name, "py.test") == 0 ||
           strcmp(name, "pytest.exe") == 0 || strcmp(name, "py.test.exe") == 0;
}

/* One argv token -> str(arg).strip('"').strip("'").replace("\\","/")
 * .rsplit("/",1)[-1].lower(). The two strips are sequential (mixed quotes
 * survive), and the basename scan honours BOTH separators on every host.
 * Malloc'd; caller frees. */
static char *sg_argv_name(const char *arg)
{
    if (!arg)
        arg = "";
    size_t n = strlen(arg);
    size_t b = 0, e = n;
    while (b < e && arg[b] == '"')
        b++;
    while (e > b && arg[e - 1] == '"')
        e--;
    while (b < e && arg[b] == '\'')
        b++;
    while (e > b && arg[e - 1] == '\'')
        e--;
    size_t start = b;
    for (size_t i = b; i < e; i++)
        if (arg[i] == '\\' || arg[i] == '/')
            start = i + 1;
    char *out = (char *)malloc(e - start + 1);
    if (!out)
        return NULL;
    size_t k = 0;
    for (size_t i = start; i < e; i++)
        out[k++] = (char)tolower((unsigned char)arg[i]);
    out[k] = '\0';
    return out;
}

/* PoP: sg_process_looks_like_pytest @ persistence/sqlite/_state/guard.py:_process_looks_like_pytest */
bool sg_process_looks_like_pytest(const char *const *cmdline, int n)
{
    /* Unreadable cmdline => not pytest: guessing the other way would refuse
     * production opens. */
    if (!cmdline || n <= 0)
        return false;
    char **names = (char **)calloc((size_t)n, sizeof(char *));
    if (!names)
        return false;
    bool found = false;
    for (int i = 0; i < n; i++) {
        names[i] = sg_argv_name(cmdline[i]);
        if (!names[i])
            names[i] = strdup("");
        if (names[i] && sg_launcher_name(names[i]))
            found = true;
    }
    if (!found) {
        /* The repo test runner counts only as the script a Python
         * interpreter runs (first non-flag token after argv[0]). */
        if (!names[0] || strncmp(names[0], "python", 6) != 0) {
            found = false;
        } else {
            const char *script = "";
            for (int i = 1; i < n; i++) {
                if (names[i] && names[i][0] != '-') {
                    script = names[i];
                    break;
                }
            }
            found = strcmp(script, "run_tests_parallel.py") == 0;
        }
    }
    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
    return found;
}

/* --- Path-like component splitting (PurePath lexical semantics) --- */

typedef struct {
    char *buf;
    char **parts;
    int n;
} sg_path_t;

static void sg_path_free(sg_path_t *p)
{
    free(p->buf);
    free(p->parts);
    p->buf = NULL;
    p->parts = NULL;
    p->n = 0;
}

/* Split on '/' AND '\\' (the Windows authority accepts both), collapse
 * repeated separators, drop '.' components; '..' stays lexical. */
static int sg_path_split(const char *s, sg_path_t *out)
{
    out->buf = strdup(s ? s : "");
    out->parts = NULL;
    out->n = 0;
    if (!out->buf)
        return -1;
    size_t cap = 8;
    out->parts = (char **)malloc(cap * sizeof(char *));
    if (!out->parts) {
        sg_path_free(out);
        return -1;
    }
    char *p = out->buf;
    while (*p) {
        while (*p == '/' || *p == '\\')
            p++;
        if (!*p)
            break;
        char *start = p;
        while (*p && *p != '/' && *p != '\\')
            p++;
        if (*p) {
            *p = '\0';
            p++;
        }
        if (strcmp(start, ".") == 0)
            continue;
        if ((size_t)out->n == cap) {
            cap *= 2;
            char **np = (char **)realloc(out->parts, cap * sizeof(char *));
            if (!np) {
                sg_path_free(out);
                return -1;
            }
            out->parts = np;
        }
        out->parts[out->n++] = start;
    }
    return 0;
}

/* WindowsPath component equality is case-insensitive (normcase); ASCII
 * case folding approximates it for the ASCII roots this guard sees. */
static bool sg_part_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

/* PoP: sg_is_production_state_db @ persistence/sqlite/_state/guard.py:_is_production_state_db */
bool sg_is_production_state_db(const char *resolved, const char *root)
{
    if (!resolved || !root)
        return false;
    sg_path_t r = { NULL, NULL, 0 }, t = { NULL, NULL, 0 };
    if (sg_path_split(resolved, &r) != 0) {
        sg_path_free(&r);
        return false;
    }
    if (sg_path_split(root, &t) != 0) {
        sg_path_free(&r);
        sg_path_free(&t);
        return false;
    }
    bool hit = false;
    /* resolved.parent == root (a drive root is its own parent). */
    int rpn = r.n > 0 ? r.n - 1 : 0;
    if (r.n == 1) {
        size_t l = strlen(r.parts[0]);
        if (l && r.parts[0][l - 1] == ':')
            rpn = 1;
    }
    if (rpn == t.n) {
        hit = true;
        for (int i = 0; i < rpn; i++) {
            if (!sg_part_eq(r.parts[i], t.parts[i])) {
                hit = false;
                break;
            }
        }
    }
    if (!hit && r.n >= t.n) {
        /* resolved.relative_to(root).parts -- lexical prefix, else ValueError. */
        bool prefix = true;
        for (int i = 0; i < t.n; i++) {
            if (!sg_part_eq(r.parts[i], t.parts[i])) {
                prefix = false;
                break;
            }
        }
        if (prefix) {
            int rel = r.n - t.n;
            /* parts[0] == "profiles" is a plain (case-sensitive) str test. */
            hit = rel == 3 && strcmp(r.parts[t.n], "profiles") == 0;
        }
    }
    sg_path_free(&r);
    sg_path_free(&t);
    return hit;
}
