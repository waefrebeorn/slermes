/*
 * _venv_entry.c — faithful C11 port of upstream Hermes pm/_venv_entry.py.
 *
 * PoP: w5_venventry_82a592_plan @ pm/_venv_entry.py:main
 *
 * main() has no callable, testable API: it is pure argument dispatch plus
 * process execution. This port reproduces the deterministic plan it computes
 * (usage/code/module/script decision, resulting sys.argv, front sys.path
 * insertion when not safe); the site.addsitedir / runpy / exec side effects are
 * process-bound and instrumented at the oracle instead.
 */
#include "noc_w5_venventry_82a592.h"
#include <stdlib.h>
#include <string.h>

static char **copy_args(const char *const *src, size_t n) {
    if (!n) return NULL;
    char **a = (char **)calloc(n, sizeof(char *));
    if (!a) return NULL;
    for (size_t i = 0; i < n; i++) a[i] = strdup(src[i] ? src[i] : "");
    return a;
}

/* dirname(ntpath.abspath(p)) for the absolute script paths used by fixtures:
 * normalise '/' to '\', then keep everything before the final separator. */
static char *script_insert_path(const char *p) {
    size_t n = strlen(p);
    char *q = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) q[i] = (p[i] == '/') ? '\\' : p[i];
    q[n] = '\0';
    char *slash = strrchr(q, '\\');
    size_t k = slash ? (size_t)(slash - q) : 0;
    char *out = (char *)malloc(k + 1);
    memcpy(out, q, k);
    out[k] = '\0';
    free(q);
    return out;
}

int w5_venventry_82a592_plan(const char *const *argv, size_t argc, int safe,
                             const char *cwd, w5_venventry_82a592_plan_t *out) {
    memset(out, 0, sizeof(*out));
    out->action = "usage";
    out->site = "";
    out->insert_path = NULL;

    /* Python: if len(sys.argv) < 3: _usage() BEFORE SITE is read.
     * sys.argv == [script] + argv, so len == 1 + argc. */
    if (argc < 2) { out->insert_path = strdup(""); return 1; }

    out->site = argv[0] ? argv[0] : "";     /* site.addsitedir(SITE) ran */
    const char *const *args = argv + 1;     /* ARGS after SITE */
    size_t nargs = argc - 1;
    const char *a0 = args[0] ? args[0] : "";

    if (!strcmp(a0, "-c") || !strcmp(a0, "-m")) {
        int is_c = (a0[1] == 'c');
        if (nargs < 2) { out->insert_path = strdup(""); return 1; } /* usage */
        out->action = is_c ? "code" : "module";
        size_t m = nargs - 1;
        char **av = (char **)malloc((m ? m : 1) * sizeof(char *));
        if (!av) return 0;
        av[0] = strdup(is_c ? "-c" : (args[1] ? args[1] : ""));
        for (size_t i = 2; i < nargs; i++) av[i - 1] = strdup(args[i] ? args[i] : "");
        out->nargv = m;
        out->argv_out = av;
        if (!safe) {
            out->inserted = 1;
            out->insert_path = is_c ? strdup("") : strdup(cwd ? cwd : "");
        } else {
            out->insert_path = strdup("");
        }
        return 1;
    }
    if (a0[0] == '-') { out->insert_path = strdup(""); return 1; } /* usage */

    out->action = "script";
    out->nargv = nargs;
    out->argv_out = copy_args(args, nargs);
    if (!safe) {
        out->inserted = 1;
        out->insert_path = script_insert_path(a0);
    } else {
        out->insert_path = strdup("");
    }
    return 1;
}

void w5_venventry_82a592_plan_free(w5_venventry_82a592_plan_t *p) {
    if (!p) return;
    free(p->insert_path);
    for (size_t i = 0; i < p->nargv; i++) free(p->argv_out[i]);
    free(p->argv_out);
    memset(p, 0, sizeof(*p));
}
