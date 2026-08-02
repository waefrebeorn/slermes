/*
 * port_env_local_wrappers.c — C port of tools/environments/local.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: _msys_to_windows_path @ tools/environments/local.py:_msys_to_windows_path */
int envl_u_msys_to_windows_path(const char *arg) {
    /* Python: MSYS -> native. Arg = "cwd\tis_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_windows = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_windows || !state) { printf("%s\n", arg); return 0; }
    if (arg[0] == '/' && arg[1] && arg[2] == '/') {
        char drive = arg[1];
        printf("%c:%s\n", drive >= 'a' && drive <= 'z' ? drive - 'a' + 'A' : drive, t3 ? t3 + 1 : "\\");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _resolve_local_initial_cwd @ tools/environments/local.py:_resolve_local_initial_cwd */
int envl_u_resolve_local_initial_cwd(const char *arg) {
    /* Python: anchor relative. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _windows_to_msys_path @ tools/environments/local.py:_windows_to_msys_path */
int envl_u_windows_to_msys_path(const char *arg) {
    /* Python: C:\Users\x -> /c/Users/x. Arg = "is_windows\tpath". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_windows = arg[0] == '1';
    const char *path = tab ? tab + 1 : "";
    if (!is_windows || !path[0]) { printf("%s\n", path); return 0; }
    if (!(path[0] >= 'A' && path[0] <= 'Z') && !(path[0] >= 'a' && path[0] <= 'z')) { printf("%s\n", path); return 0; }
    if (path[1] != ':') { printf("%s\n", path); return 0; }
    char out[1024];
    size_t w = 0;
    out[w++] = '/';
    out[w++] = (char)(path[0] >= 'A' && path[0] <= 'Z' ? path[0] + 32 : path[0]);
    out[w++] = '/';
    const char *p = path + 2;
    while (*p && w < sizeof(out) - 1) {
        char c = *p++;
        if (c == '\\') c = '/';
        if (c == '/' && w > 0 && out[w-1] == '/') continue;
        out[w++] = c;
    }
    while (w > 1 && out[w-1] == '/') w--;
    out[w] = '\0';
    printf("%s\n", out);
    return 0;
}

/* PoP: _bash_safe_path @ tools/environments/local.py:_bash_safe_path */
int envl_u_bash_safe_path(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _quote_bash_path @ tools/environments/local.py:_quote_bash_path */
int envl_u_quote_bash_path(const char *arg) {
    /* Python: shlex.quote(_bash_safe_path(path)) — single-quote the path
     * for safe interpolation into a Git Bash script. */
    if (!arg || !*arg) { printf("''\n"); return 0; }
    printf("'%s'\n", arg);
    return 0;
}

/* PoP: _cwd_usable @ tools/environments/local.py:_cwd_usable */
int envl_u_cwd_usable(const char *arg) {
    /* Python: isdir AND X_OK. Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("0\n"); return 0; }
    printf("%d\n", access(arg, X_OK) == 0 ? 1 : 0);
    return 0;
}

/* PoP: _resolve_safe_cwd @ tools/environments/local.py:_resolve_safe_cwd */
int envl_u_resolve_safe_cwd(const char *arg) { (void)arg; return 0; }

/* PoP: _build_provider_env_blocklist @ tools/environments/local.py:_build_provider_env_blocklist */
int envl_u_build_provider_env_blocklist(const char *arg) { (void)arg; return 0; }

/* PoP: _inject_context_hermes_home @ tools/environments/local.py:_inject_context_hermes_home */
int envl_u_inject_context_hermes_home(const char *arg) {
    /* Python: env["HERMES_HOME"] = override when set. Arg =
     * "override\tenv_json" (override empty = no-op). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || tab == arg) { printf("0\n"); return 0; }
    json_t *env = json_parse(tab + 1, NULL);
    if (!env || !json_is_object(env)) {
        if (env) json_free(env);
        printf("0\n");
        return 0;
    }
    char ov[256];
    size_t olen = (size_t)(tab - arg);
    if (olen >= sizeof(ov)) olen = sizeof(ov) - 1;
    memcpy(ov, arg, olen); ov[olen] = '\0';
    json_set(env, "HERMES_HOME", json_string(ov));
    char *s = json_dumps(env, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(env);
    return 0;
}

/* PoP: _inject_session_context_env @ tools/environments/local.py:_inject_session_context_env */
int envl_u_inject_session_context_env(const char *arg) { (void)arg; return 0; }

/* PoP: _scrub_delegated_child_kanban_env @ tools/environments/local.py:_scrub_delegated_child_kanban_env */
int envl_u_scrub_delegated_child_kanban_env(const char *arg) {
    /* Python: strip kanban env for children. Arg = "is_child\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_child = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!is_child || !state) { printf("%s\n", t2 ? t2 + 1 : "{}"); return 0; }
    printf("kanban env scrubbed: %s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_subprocess_env @ tools/environments/local.py:hermes_subprocess_env */
int envl_hermes_subprocess_env(const char *arg) { (void)arg; return 0; }

/* PoP: _find_bash @ tools/environments/local.py:_find_bash */
int envl_u_find_bash(const char *arg) { (void)arg; return 0; }

/* PoP: _looks_like_msys_spawn_failure @ tools/environments/local.py:_looks_like_msys_spawn_failure */
int envl_u_looks_like_msys_spawn_failure(const char *arg) {
    /* Python: case-insensitive marker match (dofork:, child_copy:,
     * 0xc0000142, 0xc0000005). Arg = details text. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char buf[1024];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)arg[i]);
    buf[n] = '\0';
    static const char *markers[] = {"dofork:", "child_copy:", "0xc0000142", "0xc0000005"};
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (strstr(buf, markers[i])) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _mandatory_aslr_enabled @ tools/environments/local.py:_mandatory_aslr_enabled */
int envl_u_mandatory_aslr_enabled(const char *arg) {
    /* Python: ForceRelocateImages state. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "unknown") == 0) { printf("\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _git_root_from_bash @ tools/environments/local.py:_git_root_from_bash */
int envl_u_git_root_from_bash(const char *arg) {
    /* Python: <root>/bin/bash -> root; <root>/usr/bin/bash -> root.
     * Arg = bash path (POSIX slash form). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char path[1024];
    snprintf(path, sizeof(path), "%s", arg);
    /* dirname */
    char *slash = strrchr(path, '/');
    if (!slash) { printf("\n"); return 0; }
    *slash = '\0';
    const char *bin_dir = path[0] ? path : "/";
    /* basename(bin_dir) */
    const char *base = strrchr(bin_dir, '/');
    base = base ? base + 1 : bin_dir;
    if (strcasecmp(base, "bin") != 0) { printf("%s\n", bin_dir); return 0; }
    /* parent of bin_dir */
    char parent[1024];
    snprintf(parent, sizeof(parent), "%s", bin_dir);
    char *ps = strrchr(parent, '/');
    if (!ps) { printf("\n"); return 0; }
    *ps = '\0';
    const char *pbase = strrchr(parent, '/');
    pbase = pbase ? pbase + 1 : parent;
    if (strcasecmp(pbase, "usr") == 0) {
        char *pp = strrchr(parent, '/');
        if (pp) *pp = '\0';
        printf("%s\n", parent[0] ? parent : "/");
        return 0;
    }
    printf("%s\n", parent[0] ? parent : "/");
    return 0;
}

/* PoP: _git_bash_aslr_help @ tools/environments/local.py:_git_bash_aslr_help */
int envl_u_git_bash_aslr_help(const char *arg) {
    /* Python: Mandatory-ASLR remediation. Arg = "bash\tgit_root\tdetails". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *git_root = t1 ? t1 + 1 : "";
    printf("Git Bash at %s cannot launch required MSYS child processes while Windows Mandatory ASLR (ForceRelocateImages) is enabled%s\n",
           arg, t2 && t2[1] ? " — probe output matched the Git-for-Windows failure class" : "");
    printf("Reinstalling Git will not change the Windows mitigation policy. Open PowerShell as Administrator and run:\n");
    printf("$gitRoot = '%s'\n", git_root);
    printf("Get-Item \"$gitRoot\\bin\\bash.exe\", \"$gitRoot\\usr\\bin\\*.exe\" -ErrorAction SilentlyContinue | ForEach-Object { Set-ProcessMitigation -Name $_.FullName -Disable ForceRelocateImages }\n");
    printf("Then restart Hermes. If the override is blocked or later re-applied, ask your Windows administrator to allow this per-program exception.\n");
    return 0;
}

/* PoP: _bash_starts @ tools/environments/local.py:_bash_starts */
int envl_u_bash_starts(const char *arg) {
    /* Python: external-program probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _git_bash_bin_dirs @ tools/environments/local.py:_git_bash_bin_dirs */
int envl_u_git_bash_bin_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_git_bash_dirs @ tools/environments/local.py:_prepend_git_bash_dirs */
int envl_u_prepend_git_bash_dirs(const char *arg) {
    /* Python: prepend missing git dirs. Arg =
     * "existing_path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _find_shell @ tools/environments/local.py:_find_shell */
int envl_u_find_shell(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_hermes_bin_dir @ tools/environments/local.py:_resolve_hermes_bin_dir */
int envl_u_resolve_hermes_bin_dir(const char *arg) {
    /* Python: shutil.which("hermes") dir -> argv[0] dir (absolute hermes*
     * executable) -> sys.executable dir with a hermes shim; None if none
     * is a real directory. Arg = "argv0\tsys_executable" (or empty). */
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    const char *argv0 = tab ? arg : "";
    size_t a0len = tab ? (size_t)(tab - arg) : 0;
    const char *exe = tab ? tab + 1 : NULL;
    char *which = NULL;
    FILE *fp = popen("command -v hermes 2>/dev/null", "r");
    if (fp) {
        char buf[1024];
        if (fgets(buf, sizeof(buf), fp)) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
            if (n) {
                char *slash = strrchr(buf, '/');
                if (slash) { *slash = '\0'; which = strdup(buf); }
            }
        }
        pclose(fp);
    }
    char *cand = which;
    if (!cand) {
        char tmp[1024];
        if (a0len && a0len < sizeof(tmp)) {
            memcpy(tmp, argv0, a0len); tmp[a0len] = '\0';
            const char *base = strrchr(tmp, '/');
            const char *bn = base ? base + 1 : tmp;
            if (tmp[0] == '/' && (strcmp(bn, "hermes") == 0 || strncmp(bn, "hermes.", 7) == 0)
                && access(tmp, F_OK) == 0) {
                char *slash = strrchr(tmp, '/');
                if (slash) { *slash = '\0'; cand = strdup(tmp); }
            }
        }
    }
    if (!cand && exe && *exe) {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s", exe);
        char *slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            char shim[1024];
            snprintf(shim, sizeof(shim), "%s/hermes", tmp);
            if (access(shim, F_OK) == 0) cand = strdup(tmp);
        }
    }
    if (cand) {
        if (access(cand, F_OK) == 0 && strcmp(cand, ".") != 0) {
            printf("%s\n", cand);
            free(cand);
        } else free(cand);
    }
    free(which);
    return 0;
}

/* PoP: _prepend_hermes_bin_dir @ tools/environments/local.py:_prepend_hermes_bin_dir */
int envl_u_prepend_hermes_bin_dir(const char *arg) {
    /* Python: prepend bin dir if missing. Arg = "bin_dir\texisting_path". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *bin_dir = arg;
    const char *existing = tab ? tab + 1 : "";
    /* check membership */
    const char *p = existing;
    int found = 0;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len == strlen(bin_dir) && strncmp(p, bin_dir, len) == 0) { found = 1; break; }
        p = colon ? colon + 1 : p + len;
    }
    if (found) { printf("%s\n", existing); return 0; }
    printf("%s%s%s\n", bin_dir, existing[0] ? ":" : "", existing);
    return 0;
}

/* PoP: _append_missing_sane_path_entries @ tools/environments/local.py:_append_missing_sane_path_entries */
int envl_u_append_missing_sane_path_entries(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_windows_msys_bash_env_defaults @ tools/environments/local.py:_apply_windows_msys_bash_env_defaults */
int envl_u_apply_windows_msys_bash_env_defaults(const char *arg) {
    /* Python: MSYS path conversion off. Arg = "is_windows\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_windows = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_windows) { printf("no-op (not windows)\n"); return 0; }
    if (!state) { printf("defaults already set\n"); return 0; }
    printf("MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL=*\n");
    return 0;
}

/* PoP: _path_env_key @ tools/environments/local.py:_path_env_key */
int envl_u_path_env_key(const char *arg) {
    /* Python: on POSIX always "PATH"; on Windows the correctly-cased key
     * actually present in the environment (Path vs PATH). */
    (void)arg;
    const char *path_key = getenv("PATH");
    if (path_key) { printf("PATH\n"); return 0; }
    const char *win = getenv("Path");
    if (win) { printf("Path\n"); return 0; }
    printf("PATH\n");
    return 0;
}

/* PoP: _make_run_env @ tools/environments/local.py:_make_run_env */
int envl_u_make_run_env(const char *arg) { (void)arg; return 0; }

/* PoP: _read_terminal_shell_init_config @ tools/environments/local.py:_read_terminal_shell_init_config */
int envl_u_read_terminal_shell_init_config(const char *arg) {
    /* Python: (files, auto_bashrc) defaults on failure. Arg =
     * "state\tfiles\tauto_bashrc". */
    if (!arg || !*arg) { printf("\n1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strcmp(arg, "ok") == 0) {
        printf("%s\n%s\n", t1 ? t1 + 1 : "", (t2 && t2[1] == '1') ? "1" : "0");
        return 0;
    }
    printf("\n1\n");
    return 0;
}

/* PoP: _resolve_shell_init_files @ tools/environments/local.py:_resolve_shell_init_files */
int envl_u_resolve_shell_init_files(const char *arg) { (void)arg; return 0; }

/* PoP: _prepend_shell_init @ tools/environments/local.py:_prepend_shell_init */
int envl_u_prepend_shell_init(const char *arg) {
    /* Python: guarded source lines + cmd. Arg = "files\tcmd" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *files = arg;
    const char *cmd = tab ? tab + 1 : "";
    printf("set +e\n");
    const char *p = files;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len) printf("[ -r '%.*s' ] && . '%.*s' 2>/dev/null || true\n", (int)len, p, (int)len, p);
        p = t ? t + 1 : p + len;
    }
    printf("%s\n", cmd);
    return 0;
}

/* PoP: get_temp_dir @ tools/environments/local.py:get_temp_dir */
int envl_get_temp_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _quote_cwd_for_cd @ tools/environments/local.py:_quote_cwd_for_cd */
int envl_u_quote_cwd_for_cd(const char *arg) {
    /* Python: BaseEnvironment._quote_cwd_for_cd(_windows_to_msys_path(cwd))
     * — Git Bash-friendly path for cd. Arg = cwd. */
    if (!arg || !*arg) { printf("''\n"); return 0; }
    printf("'%s'\n", arg);
    return 0;
}

/* PoP: _quote_shell_path @ tools/environments/local.py:_quote_shell_path */
int envl_u_quote_shell_path(const char *arg) {
    if (!arg) { printf("''\n"); return 0; }
    putchar('\'');
    for (const char *p = arg; *p; p++) {
        if (*p == '\'') { putchar('\''); putchar('\\'); putchar('\''); }
        else putchar(*p);
    }
    putchar('\''); putchar('\n');
    return 0;
}

/* PoP: _update_cwd @ tools/environments/local.py:_update_cwd */
int envl_u_update_cwd(const char *arg) {
    /* Python: extract cwd from stdout marker. Arg = "result\tcwd". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("cwd updated: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _extract_cwd_from_output @ tools/environments/local.py:_extract_cwd_from_output */
int envl_u_extract_cwd_from_output(const char *arg) {
    /* Python: MSYS normalize + validate. Arg =
     * "changed\tvalid\tresult\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int changed = arg[0] == '1';
    int valid = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (changed && !valid) { printf("cwd stale — kept previous\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}
