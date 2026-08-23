/* gate: profiles_clone_ignore — traced C copy of
 * src/hermes_cli/profiles.c::profile_clone_ignore.
 * PoP of LIVE hermes_cli/profiles.py::_clone_all_copytree_ignore's inner
 * _ignore(directory, names) predicate (single-entry invocation).
 *
 * Input format: "source_dir|dir|entry".
 * The harness pins HERMES_HOME to /tmp/wcup_default_home before tracing so
 * both sides agree on the default-profile root used by the infra rung.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static bool ends_with(const char *s, const char *suffix) {
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcmp(s + ls - lx, suffix) == 0;
}

static const char *default_home(void) {
    /* mirrors slermes resolve_home() with HERMES_HOME=/tmp/wcup_default_home:
     * outside the native home, parent != "profiles" -> HERMES_HOME itself. */
    return "/tmp/wcup_default_home";
}

static void clone_ignore(const char *input) {
    WCUP_BEGIN("profile_clone_ignore");
    char src[256] = "", dir[256] = "", entry[128] = "";
    if (input) {
        char tmp[640];
        snprintf(tmp, sizeof tmp, "%s", input);
        char *p1 = strchr(tmp, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (p1 && p2) {
            *p1 = '\0'; *p2 = '\0';
            snprintf(src, sizeof src, "%s", tmp);
            snprintf(dir, sizeof dir, "%s", p1 + 1);
            snprintf(entry, sizeof entry, "%s", p2 + 1);
        }
    }
    WCUP_VALSTR(src);
    WCUP_VALSTR(dir);
    WCUP_VALSTR(entry);

    bool result = false;
    if (!entry[0]) {
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        /* Universal exclusions at any depth. */
        int universal = strcmp(entry, "__pycache__") == 0 || ends_with(entry, ".pyc") ||
                        ends_with(entry, ".pyo") || ends_with(entry, ".sock") ||
                        ends_with(entry, ".tmp");
        WCUP_BRANCH(universal);
        if (universal) {
            result = true;
        } else {
            bool at_root = strcmp(dir, src) == 0;
            WCUP_BRANCH(at_root);
            if (at_root) {
                static const char *HIST[] = {"state.db", "state.db-wal", "state.db-shm",
                    "sessions", "backups", "state-snapshots", "checkpoints", NULL};
                int hist = 0;
                for (int i = 0; HIST[i]; i++) {
                    WCUP_LOOP_TICK();
                    if (strcmp(entry, HIST[i]) == 0) { hist = 1; break; }
                }
                WCUP_BRANCH(hist);
                if (hist) {
                    result = true;
                } else {
                    bool is_default = strcmp(src, default_home()) == 0;
                    WCUP_BRANCH(is_default);
                    if (is_default) {
                        static const char *INFRA[] = {"hermes-agent", ".worktrees",
                            "profiles", "bin", "node_modules", NULL};
                        int infra = 0;
                        for (int i = 0; INFRA[i]; i++) {
                            WCUP_LOOP_TICK();
                            if (strcmp(entry, INFRA[i]) == 0) { infra = 1; break; }
                        }
                        WCUP_BRANCH(infra);
                        if (infra) result = true;
                    }
                }
            }
        }
    }
    char rbuf[8];
    snprintf(rbuf, sizeof rbuf, "%s", result ? "True" : "False");
    WCUP_VALSTR(rbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profile_clone_ignore");
}

int main(int argc, char **argv) {
    setenv("HERMES_HOME", "/tmp/wcup_default_home", 1);
    if (argc < 3) { fprintf(stderr, "usage: %s profile_clone_ignore <src|dir|entry>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) clone_ignore(argv[i]);
    return 0;
}
