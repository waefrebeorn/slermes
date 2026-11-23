/*
 * main_resume_args.c — C port of upstream Hermes hermes_cli/main_resume_args.py.
 * Gate: w15_mainresume_0c8e53
 *
 * PoP: w15_mainresume_0c8e53_apply_in_dir_guard @ hermes_cli/main_resume_args.py:apply_in_dir
 * PoP: w15_mainresume_0c8e53_import_foreign_resume_guard @ hermes_cli/main_resume_args.py:import_foreign_resume
 * PoP: w15_mainresume_0c8e53_recreate_tui_worktree_guard @ hermes_cli/main_resume_args.py:recreate_resumed_tui_worktree
 *
 * Ported guards (Python source):
 *   apply_in_dir:                   in_dir = getattr(args,"in_dir",None); if not in_dir: return
 *   import_foreign_resume:          if not (isinstance(resume,str) and
 *                                       resume.strip().lower() in ("@claude","@codex")): return
 *                                   kind = resume.strip().lower().lstrip("@")
 *   recreate_resumed_tui_worktree:  if not resume or worktree: return; restore(resume)
 * The oracle injects sentinel-raising fakes for the dependency imports so the
 * live functions reveal their guard decisions without chdir/import/DB effects.
 */
#include "noc_w15_mainresume_0c8e53.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { if (b > a) memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && *v->str_val) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
    }
    return 0;
}

json_t *w15_mainresume_0c8e53_apply_in_dir_guard(const struct json_t *in_dir) {
    return json_bool(py_truthy(in_dir) ? true : false);
}

json_t *w15_mainresume_0c8e53_import_foreign_resume_guard(const struct json_t *resume) {
    json_t *out = json_object();
    if (!resume || resume->type != JSON_STRING) {
        json_set(out, "acts", json_bool(false));
        json_set(out, "kind", json_null());
        return out;
    }
    char *t = strip_ws(resume->str_val ? resume->str_val : "");
    for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    int acts = (!strcmp(t, "@claude") || !strcmp(t, "@codex"));
    json_set(out, "acts", json_bool(acts ? true : false));
    if (acts) {
        char *k = t;
        while (*k == '@') k++;
        json_set(out, "kind", json_string(k));
    } else {
        json_set(out, "kind", json_null());
    }
    free(t);
    return out;
}

json_t *w15_mainresume_0c8e53_recreate_tui_worktree_guard(const struct json_t *resume,
                                                          const struct json_t *worktree) {
    json_t *out = json_object();
    int acts = py_truthy(resume) && !py_truthy(worktree);
    json_set(out, "acts", json_bool(acts ? true : false));
    json_set(out, "resume", acts ? (resume ? json_copy(resume) : json_null()) : json_null());
    return out;
}
