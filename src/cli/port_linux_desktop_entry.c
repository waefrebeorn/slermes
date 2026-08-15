/* linux_desktop_entry.c — Port of hermes_cli/linux_desktop_entry.py.
 * Install and remove the Linux XDG desktop entry (hermes.desktop).
 *
 * Faithful to Python: side-effect-free at import, best-effort cache refresh,
 * and the uninstaller / Electron main process both use this.
 */
#define _GNU_SOURCE
#include "linux_desktop_entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "hermes_json.h"
#include "hermes_logger.h"

#define LDE_ENTRY_NAME "hermes.desktop"

/* PoP: lde_is_supported @ hermes_cli/linux_desktop_entry.py:is_supported */
/* Python: sys.platform.startswith(("linux","freebsd","openbsd","netbsd")). */
bool lde_is_supported(void) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    return true;
#else
    return false;
#endif
}

/* PoP: lde_xdg_data_home @ hermes_cli/linux_desktop_entry.py:_xdg_data_home */
char *lde_xdg_data_home(void) {
    const char *raw = getenv("XDG_DATA_HOME");
    if (raw && raw[0] && raw[strspn(raw, " \t")]) {
        /* Python: Path(raw).expanduser(). */
        if (raw[0] == '~') {
            const char *home = getenv("HOME");
            char *out = NULL;
            asprintf(&out, "%s%s", home ? home : "", raw + 1);
            return out;
        }
        return strdup(raw);
    }
    const char *home = getenv("HOME");
    char *out = NULL;
    asprintf(&out, "%s%s", home ? home : "", "/.local/share");
    return out;
}

/* PoP: lde_desktop_entry_path @ hermes_cli/linux_desktop_entry.py:desktop_entry_path */
char *lde_desktop_entry_path(void) {
    char *xdg = lde_xdg_data_home();
    if (!xdg) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/applications/%s", xdg, LDE_ENTRY_NAME);
    free(xdg);
    return out;
}

/* PoP: lde_icon_path @ hermes_cli/linux_desktop_entry.py:icon_path */
char *lde_icon_path(const char *project_root) {
    if (!project_root) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/apps/desktop/assets/icon.png", project_root);
    return out;
}

/* PoP: lde_quote_exec_arg @ hermes_cli/linux_desktop_entry.py:_quote_exec_arg */
char *lde_quote_exec_arg(const char *arg) {
    if (!arg) return strdup("");
    /* Reserved characters: space tab newline " ' \ > < ~ | & ; $ * ? # ( ) ` */
    bool need_quote = false;
    for (const char *p = arg; *p; p++) {
        if (strchr(" \t\n\"'\\><~|&;$*?#()`", *p)) { need_quote = true; break; }
    }
    if (!need_quote) return strdup(arg);
    /* escaped = arg.replace("\\","\\\\").replace('"','\\"'); return f'"{escaped}"' */
    size_t cap = strlen(arg) * 2 + 4;
    char *out = malloc(cap);
    if (!out) return NULL;
    char *q = out;
    *q++ = '"';
    for (const char *p = arg; *p; p++) {
        if (*p == '\\' || *p == '"') { *q++ = '\\'; }
        *q++ = *p;
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

/* PoP: lde_render_desktop_entry @ hermes_cli/linux_desktop_entry.py:render_desktop_entry */
char *lde_render_desktop_entry(const char *exec_command, const char *icon) {
    if (!exec_command) exec_command = "";
    if (!icon) icon = "";
    char *out = NULL;
    asprintf(&out,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Hermes\n"
        "GenericName=Hermes Desktop\n"
        "Comment=Launch Hermes Desktop\n"
        "Exec=%s\n"
        "Icon=%s\n"
        "Terminal=false\n"
        "Categories=Utility;\n"
        "StartupNotify=true\n"
        "StartupWMClass=Hermes\n",
        exec_command, icon);
    return out;
}

/* PoP: lde_run_quiet @ hermes_cli/linux_desktop_entry.py:_run_quiet */
bool lde_run_quiet(const char *const *cmd, int cmd_count) {
    if (!cmd || cmd_count <= 0) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        /* child: stdout/stderr -> DEVNULL */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2) close(devnull);
        }
        char **argv = malloc((size_t)(cmd_count + 1) * sizeof(char *));
        if (!argv) _exit(127);
        for (int i = 0; i < cmd_count; i++) argv[i] = (char *)cmd[i];
        argv[cmd_count] = NULL;
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    /* Python timeout=60 — block with a bounded wait (no alarm to keep it simple). */
    if (waitpid(pid, &status, 0) == -1) return false;
    if (WIFEXITED(status)) return WEXITSTATUS(status) == 0;
    return false;
}

/* PoP: lde_refresh_desktop_databases @ hermes_cli/linux_desktop_entry.py:refresh_desktop_databases */
char *lde_refresh_desktop_databases(const char *applications_dir, int *out_ran) {
    int ran = 0;
    json_t *arr = json_array();
    if (!applications_dir) applications_dir = "";

    /* update-desktop-database <applications_dir> */
    if (access("/usr/bin/update-desktop-database", X_OK) == 0 ||
        access("/usr/local/bin/update-desktop-database", X_OK) == 0) {
        const char *cmd[] = { "update-desktop-database", applications_dir };
        if (lde_run_quiet(cmd, 2)) {
            json_append(arr, json_string("update-desktop-database"));
            ran++;
        }
    }

    /* Plasma 6 first, then Plasma 5 — only one installed. */
    const char *kbuild_tools[] = { "kbuildsycoca6", "kbuildsycoca5" };
    for (size_t i = 0; i < 2; i++) {
        const char *tool = kbuild_tools[i];
        if (access("/usr/bin/kbuildsycoca6", X_OK) == 0 || access("/usr/bin/kbuildsycoca5", X_OK) == 0) {
            /* Only run the one that exists. */
            if (i == 1 && ran) break; /* both checked above; just run first found */
        }
        char bin[256];
        snprintf(bin, sizeof(bin), "/usr/bin/%s", tool);
        if (access(bin, X_OK) == 0) {
            const char *cmd[] = { tool, "--noincremental" };
            if (lde_run_quiet(cmd, 2)) {
                json_append(arr, json_string(tool));
                ran++;
            }
            break;
        }
    }

    char *ser = json_serialize(arr);
    json_free(arr);
    if (out_ran) *out_ran = ran;
    return ser ? ser : strdup("[]");
}

/* PoP: lde_resolve_exec_command @ hermes_cli/linux_desktop_entry.py:resolve_exec_command */
char *lde_resolve_exec_command(void) {
    /* Python: prefer the real hermes executable (argv[0] or PATH); else use
     * the current interpreter -m hermes_cli.main. In C, resolve our own
     * /proc/self/exe (the slermes binary), then append "desktop". */
    char self[4096];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char *q = lde_quote_exec_arg(self);
        char *out = NULL;
        asprintf(&out, "%s desktop", q);
        free(q);
        return out;
    }
    /* Fallback: bare "hermes desktop". */
    return strdup("hermes desktop");
}

/* PoP: lde_install_desktop_entry @ hermes_cli/linux_desktop_entry.py:install_desktop_entry */
char *lde_install_desktop_entry(const char *project_root) {
    if (!lde_is_supported()) return NULL;
    if (!project_root) return NULL;

    char *entry_path = lde_desktop_entry_path();
    if (!entry_path) return NULL;
    char *icon = lde_icon_path(project_root);
    /* icon_value = str(icon) if icon.is_file() else "hermes" */
    const char *icon_value = "hermes";
    if (icon && access(icon, R_OK) == 0) icon_value = icon;

    char *exec = lde_resolve_exec_command();
    char *contents = lde_render_desktop_entry(exec, icon_value);

    /* entry_path.parent.mkdir(parents=True, exist_ok=True) */
    char *dir = strdup(entry_path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        char *p = dir;
        while (*p) {
            char *next = strchr(p + 1, '/');
            char save = next ? *next : '\0';
            if (next) *next = '\0';
            mkdir(dir, 0755);
            if (next) *next = save;
            p = next ? next : p + strlen(p);
        }
        mkdir(dir, 0755);
    }

    /* Skip rewrite when unchanged (avoids churning menu caches). */
    bool unchanged = false;
    FILE *f = fopen(entry_path, "rb");
    if (f) {
        char buf[8192];
        size_t rd = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[rd] = '\0';
        unchanged = (strcmp(buf, contents) == 0);
    }
    if (unchanged) {
        free(dir); free(exec); free(contents); free(icon);
        return entry_path;
    }

    FILE *w = fopen(entry_path, "wb");
    if (!w) {
        free(dir); free(exec); free(contents); free(icon); free(entry_path);
        return NULL;
    }
    fputs(contents, w);
    fclose(w);
    chmod(entry_path, 0755);

    free(dir); free(exec); free(contents); free(icon);
    lde_refresh_desktop_databases(entry_path, NULL);
    return entry_path;
}
