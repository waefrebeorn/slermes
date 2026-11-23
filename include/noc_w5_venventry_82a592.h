#ifndef NOC_W5_VENVENTRY_82A592_H
#define NOC_W5_VENVENTRY_82A592_H
#include <stddef.h>

/* Argument-validation / dispatch plan computed by pm/_venv_entry.py:main().
 * Execution itself (site.addsitedir, runpy, exec) is process/IO-bound; the gate
 * captures the deterministic decisions: usage vs code/module/script, the
 * resulting sys.argv, and the front-of-sys.path entry inserted when not safe. */

typedef struct {
    const char *action;    /* "usage" | "code" | "module" | "script" */
    const char *site;      /* SITE argument, "" when usage */
    int         inserted;  /* 1 when main() inserted a front sys.path entry */
    char       *insert_path; /* malloc'd inserted value ("" for -c) */
    size_t      nargv;     /* length of argv_out */
    char      **argv_out;  /* malloc'd array of malloc'd strings */
} w5_venventry_82a592_plan_t;

/* argc is the number of arguments AFTER the script name: [SITE, ...ARGS].
 * 'safe' mirrors sys.flags.safe_path (falling back to isolated). 'cwd' is
 * injected for the -m branch (os.getcwd()). Returns 0 on allocation failure. */
int w5_venventry_82a592_plan(const char *const *argv, size_t argc, int safe,
                             const char *cwd, w5_venventry_82a592_plan_t *out);
void w5_venventry_82a592_plan_free(w5_venventry_82a592_plan_t *p);

#endif /* NOC_W5_VENVENTRY_82A592_H */
