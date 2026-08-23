/* gate: self_repo_guard_git_target_and_subcommand — traced C copy of
 * src/tools/self_repo_guard.c::srg_git_target_and_subcommand.
 * Faithful port of LIVE Python tools/self_repo_guard.py::_git_target_and_subcommand.
 *
 * Input grammar (argv-safe, parsed identically by twin):
 *   cwd|envspec|argspec   with '-' meaning empty
 *   envspec: KEY=VALUE pairs joined by ';'
 *   argspec: args joined by '+'
 * Paths must exist on disk so LIVE Path.resolve() and C realpath() agree.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include "watercup.h"

static char *resolve_path(const char *path_str, const char *base) {
    if (!path_str) return NULL;
    const char *p = path_str;
    char *expanded = NULL;
    if (p[0] == '~') {
        const char *home = getenv("HOME");
        if (home && p[1] == '/') {
            expanded = malloc(strlen(home) + strlen(p) + 1);
            sprintf(expanded, "%s%s", home, p + 1);
        } else if (home && p[1] == '\0') {
            expanded = strdup(home);
        }
    }
    if (!expanded) expanded = strdup(p);
    char *joined;
    if (expanded[0] != '/') {
        size_t bl = strlen(base), nl = strlen(expanded);
        joined = malloc(bl + 1 + nl + 1);
        memcpy(joined, base, bl); joined[bl] = '/';
        memcpy(joined + bl + 1, expanded, nl + 1);
        free(expanded);
    } else joined = expanded;
    char *resolved = realpath(joined, NULL);
    if (resolved) { free(joined); return resolved; }
    return joined;
}

static void trace_one(const char *cwd, char **args, char (*env)[2][256], int nenv) {
    WCUP_BEGIN("git_target_and_subcommand");
    char target[PATH_MAX];
    snprintf(target, sizeof target, "%s", cwd ? cwd : "/");
    char work_tree[256]; work_tree[0] = '\0';
    /* aliases packed as key=value joined by '+' */
    char aliases[1024]; aliases[0] = '\0';
    int index = 0;
    int running = 1;
    while (running) {
        WCUP_LOOP_TICK();
        const char *arg = args[index];
        if (!arg) { WCUP_BRANCH(0); break; }
        WCUP_BRANCH(1);
        if (strcmp(arg, "--") == 0) { WCUP_BRANCH(1); index++; break; }
        WCUP_BRANCH(0);
        int is_C = (strcmp(arg, "-C") == 0 && args[index + 1]);
        WCUP_BRANCH(is_C);
        if (is_C) {
            char *r = resolve_path(args[index + 1], target);
            if (r) { snprintf(target, sizeof target, "%s", r); free(r); }
            index += 2; continue;
        }
        int is_Cattached = (strncmp(arg, "-C", 2) == 0 && strlen(arg) > 2);
        WCUP_BRANCH(is_Cattached);
        if (is_Cattached) {
            char *r = resolve_path(arg + 2, target);
            if (r) { snprintf(target, sizeof target, "%s", r); free(r); }
            index += 1; continue;
        }
        int is_wtflag = (strcmp(arg, "--work-tree") == 0 || strcmp(arg, "--git-dir") == 0 ||
                         strcmp(arg, "--namespace") == 0 || strcmp(arg, "--exec-path") == 0);
        WCUP_BRANCH(is_wtflag);
        if (is_wtflag) {
            int wt_val = (strcmp(arg, "--work-tree") == 0 && args[index + 1]);
            WCUP_BRANCH(wt_val);
            if (wt_val) snprintf(work_tree, sizeof work_tree, "%s", args[index + 1]);
            index += 2; continue;
        }
        int is_wteq = (strncmp(arg, "--work-tree=", 12) == 0);
        WCUP_BRANCH(is_wteq);
        if (is_wteq) {
            snprintf(work_tree, sizeof work_tree, "%s", arg + 12);
            index += 1; continue;
        }
        int is_c = (strcmp(arg, "-c") == 0 && args[index + 1] &&
                    strncmp(args[index + 1], "alias.", 6) == 0 &&
                    strchr(args[index + 1], '='));
        WCUP_BRANCH(is_c);
        if (is_c) {
            char cfg[256]; snprintf(cfg, sizeof cfg, "%s", args[index + 1]);
            char *eq = strchr(cfg, '='); *eq = '\0';
            for (char *q = cfg + 6; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
            char alias_entry[512];
            snprintf(alias_entry, sizeof alias_entry, "%s=%s",
                     cfg + 6, eq + 1);
            if (aliases[0]) strcat(aliases, "+");
            strcat(aliases, alias_entry);
            index += 2; continue;
        }
        int is_calias = (strncmp(arg, "-calias.", 8) == 0 && strchr(arg, '='));
        WCUP_BRANCH(is_calias);
        if (is_calias) {
            char tmp[256]; snprintf(tmp, sizeof tmp, "%s", arg + 2);
            char *eq = strchr(tmp, '='); *eq = '\0';
            for (char *q = tmp + 6; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
            char alias_entry[512];
            snprintf(alias_entry, sizeof alias_entry, "%s=%s", tmp + 6, eq + 1);
            if (aliases[0]) strcat(aliases, "+");
            strcat(aliases, alias_entry);
            index += 1; continue;
        }
        int is_dash = (arg[0] == '-');
        WCUP_BRANCH(is_dash);
        if (is_dash) { index += 1; continue; }
        break;
    }
    /* explicit_work_tree = work_tree or env GIT_WORK_TREE */
    char explicit_wt[256];
    if (work_tree[0]) snprintf(explicit_wt, sizeof explicit_wt, "%s", work_tree);
    else {
        explicit_wt[0] = '\0';
        for (int i = 0; i < nenv; i++)
            if (strcmp(env[i][0], "GIT_WORK_TREE") == 0) {
                snprintf(explicit_wt, sizeof explicit_wt, "%s", env[i][1]);
                break;
            }
    }
    int has_explicit = (explicit_wt[0] != '\0');
    WCUP_BRANCH(has_explicit);
    if (has_explicit) {
        char *r = resolve_path(explicit_wt, target);
        if (r) { snprintf(target, sizeof target, "%s", r); free(r); }
    }
    char subcommand[128]; subcommand[0] = '\0';
    if (args[index]) {
        snprintf(subcommand, sizeof subcommand, "%s", args[index]);
        for (char *q = subcommand; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
    }
    int has_sub = (subcommand[0] != '\0');
    WCUP_BRANCH(has_sub);
    WCUP_VALSTR(target);
    WCUP_VALSTR(subcommand);
    /* remaining args after the subcommand */
    char rest[512]; rest[0] = '\0';
    if (has_sub) {
        for (int i = index + 1; args[i]; i++) {
            if (rest[0]) strcat(rest, "+");
            strcat(rest, args[i]);
        }
    }
    WCUP_VALSTR(rest);
    WCUP_VALSTR(aliases);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("git_target_and_subcommand");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s git_target_and_subcommand <cwd|env|args>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char spec[1024]; snprintf(spec, sizeof spec, "%s", argv[i]);
        char *parts[3]; int np = 0;
        char *start = spec;
        while (np < 3) {
            char *bar = strchr(start, '|');
            if (bar) *bar = '\0';
            parts[np++] = start;
            if (!bar) break;
            start = bar + 1;
        }
        const char *cwd = parts[0];
        static char (*env)[2][256] = NULL; static int nenv_store = 0;
        char *(*envp)[2][256] = &env;
        int nenv = 0;
        char envspec[512]; envspec[0] = '\0';
        if (np > 1) snprintf(envspec, sizeof envspec, "%s", parts[1]);
        if (strcmp(envspec, "-") != 0 && envspec[0]) {
            char *save = envspec;
            while (*save && nenv < 16) {
                char *semi = strchr(save, ';');
                if (semi) *semi = '\0';
                char *eq = strchr(save, '=');
                if (eq) {
                    *eq = '\0';
                    snprintf((*envp)[nenv][0], 256, "%s", save);
                    snprintf((*envp)[nenv][1], 256, "%s", eq + 1);
                    nenv++;
                }
                if (!semi) break;
                save = semi + 1;
            }
        }
        char argspec[512];
        if (np > 2) snprintf(argspec, sizeof argspec, "%s", parts[2]); else argspec[0] = '\0';
        char *args[32]; int nargs = 0;
        if (strcmp(argspec, "-") != 0 && argspec[0]) {
            char *save2 = argspec;
            while (*save2 && nargs < 31) {
                char *plus = strchr(save2, '+');
                if (plus) *plus = '\0';
                args[nargs++] = save2;
                if (!plus) break;
                save2 = plus + 1;
            }
        }
        args[nargs] = NULL;
        (void)nenv_store;
        trace_one(cwd, args, (char (*)[2][256])env, nenv);
    }
    return 0;
}
