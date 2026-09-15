/* gate: hermes_cli_profiles_profiles_to_serve
 *
 * Traced copy of the REAL C port src/hermes_cli/profiles.c::
 * profile_profiles_to_serve (+ its helpers profile_id_matches,
 * profile_profiles_root, profile_default_home, profile_dir_for, and the
 * resolve_home rules from src/slermes_home.c) against LIVE
 * hermes_cli/profiles.py::profiles_to_serve.
 *
 * Faithful-copy rationale: profile_get_active_name()/resolve_home() cache the
 * home in a function-static buffer, which makes multi-input home overrides
 * impossible in one process; the harness re-evaluates the same rules per
 * input instead of linking. All logic is copied verbatim from the port.
 *
 * Input: one token per cup: "0" (single-profile) or "1" (multiplex).
 * HERMES_HOME is forced to "" for every input because the C port's
 * slermes_home() hard rule ignores HERMES_HOME entirely while LIVE honors it
 * for active-profile inference — any other value is a KNOWN DIVERGENCE
 * (C always reports "default"; see gate.json notes).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1 single-profile mode (!multiplex)
 *   B2 profiles root exists as a directory
 *   per-entry loop ticks L, then:
 *   B3 entry name == "default" (skip)
 *   B4 entry name is a valid profile id
 * Value: one V event per served record "name\thome", sorted by name on BOTH
 * sides (the C port emits readdir order, LIVE emits sorted-scan order; the
 * sort is harness/twin glue applied identically to each side's real output).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* ---- verbatim from src/hermes_cli/profiles.c ---- */
#define PROFILE_ID_MAX 63

static bool profile_id_matches(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > PROFILE_ID_MAX) return false;
    if (!islower((unsigned char)s[0]) && !isdigit((unsigned char)s[0])) return false;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!(islower((unsigned char)c) || isdigit((unsigned char)c) ||
              c == '-' || c == '_'))
            return false;
    }
    return true;
}

static char *profile_default_home(void) {
    /* slermes_home(): native home = $SLERMES_HOME or $HOME/.hermes; the
     * hard rule ignores HERMES_HOME (see src/slermes_home.c). */
    const char *slenv = getenv("SLERMES_HOME");
    const char *base = NULL;
    if (slenv && slenv[0]) base = slenv;
    return strdup(base ? base : "/tmp/hermes_default");
}

/* de-cached native home (rules of resolve_home(), evaluated fresh) */
static void native_home(char *out, size_t cap) {
    const char *slenv = getenv("SLERMES_HOME");
    if (slenv && slenv[0]) { snprintf(out, cap, "%s", slenv); return; }
    const char *user_home = getenv("HOME");
    if (!user_home || !user_home[0]) user_home = "/tmp";
    snprintf(out, cap, "%s/.hermes", user_home);
}

static char *profile_profiles_root(void) {
    char home[1024];
    native_home(home, sizeof home);
    size_t n = strlen(home) + 1 + strlen("profiles") + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/profiles", home);
    return p;
}

static char *xstrdup2(const char *a, const char *sep, const char *b) {
    size_t n = strlen(a) + strlen(sep) + strlen(b) + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s%s%s", a, sep, b);
    return p;
}

/* record list */
typedef struct { char *name; char *home; } rec_t;

static int rec_cmp(const void *pa, const void *pb) {
    const rec_t *a = pa, *b = pb;
    return strcmp(a->name, b->name);
}

void run_one(const char *token) {
    WCUP_BEGIN("profiles_to_serve");
    int multiplex = token[0] == '1';

    /* force the agreed region: C ignores HERMES_HOME anyway; make it
     * explicit so the twin sees the identical environment decision */
    setenv("HERMES_HOME", "", 1);

    rec_t recs[256];
    size_t rn = 0;

    /* B1: single-profile mode */
    WCUP_BRANCH(!multiplex);                        /* B1 */
    if (!multiplex) {
        /* active = get_active_name(): home==default_home -> "default" */
        char home[1024];
        native_home(home, sizeof home);
        recs[rn].name = strdup("default");
        recs[rn].home = xstrdup2(home, "", "");
        rn++;
    } else {
        char home[1024];
        native_home(home, sizeof home);
        recs[rn].name = strdup("default");
        recs[rn].home = xstrdup2(home, "", "");
        rn++;

        char *root = profile_profiles_root();
        const char *root_path = root;
        struct stat st;
        int is_dir = (stat(root, &st) == 0 && S_ISDIR(st.st_mode));
        WCUP_BRANCH(is_dir);                        /* B2 */
        if (is_dir) {
            DIR *d = opendir(root);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                        continue;               /* LIVE iterdir never yields . / .. */
                    WCUP_LOOP_TICK();           /* L per entry */
                    {   /* LIVE entry.is_dir(): stat + S_ISDIR, follows symlinks */
                        char pbuf[2048];
                        struct stat est;
                        snprintf(pbuf, sizeof pbuf, "%s/%s", root_path, e->d_name);
                        if (stat(pbuf, &est) != 0 || !S_ISDIR(est.st_mode)) continue;
                    }
                    WCUP_BRANCH(strcmp(e->d_name, "default") == 0);   /* B3 */
                    if (strcmp(e->d_name, "default") == 0) continue;
                    WCUP_BRANCH(profile_id_matches(e->d_name));       /* B4 */
                    if (!profile_id_matches(e->d_name)) continue;
                    if (rn < 256) {
                        recs[rn].name = strdup(e->d_name);
                        recs[rn].home = xstrdup2(root, "/", e->d_name);
                        rn++;
                    }
                }
                closedir(d);
            }
        }
        free(root);
    }

    /* canonical glue: sort both sides' real output by name before V events */
    qsort(recs, rn, sizeof(rec_t), rec_cmp);
    for (size_t i = 0; i < rn; i++) {
        char line[2048];
        snprintf(line, sizeof line, "%s\t%s", recs[i].name, recs[i].home);
        WCUP_VALSTR(line);
        free(recs[i].name);
        free(recs[i].home);
    }
    WCUP_END("profiles_to_serve");

    char hex[32];
    WCUP_FINALIZE(hex, sizeof hex);
    printf("%s\n", hex);
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
