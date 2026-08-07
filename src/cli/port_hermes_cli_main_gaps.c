/* port_hermes_cli_main_gaps.c — Port of hermes_cli/main.py helpers missing
 * from the C port: startup fast-path wrappers, workspace-key resolution,
 * dashboard runtime parsing, systemd/cgroup introspection, bytecode
 * fingerprint sweep, PE-machine detection, macOS signing helpers, and web
 * build-tool diagnosis. Faithful ports of the Python originals.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include "hermes_json.h"

/* ── startup_fast wrappers (defined in port_startup_fast.c) ─────────── */
extern bool sf_is_termux_env(void);
extern bool sf_is_termux_fast_version_argv(int argc, char **argv);
extern bool sf_is_global_fast_version_argv(int argc, char **argv);
extern bool sf_is_container_startup_environment(void);
extern bool sf_active_profile_may_override_home(const char *hermes_root);
extern bool sf_container_mode_may_be_active(void);
extern char *sf_read_openai_version(void);
extern char *sf_read_install_method(void);
extern char *sf_project_root_str(void);

/* ════════════════════════════════════════════════════════════════════
 * startup fast-path wrappers
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _project_root_str_fast @ hermes_cli/main.py:_project_root_str_fast */
char *main_g_project_root_str_fast(void) {
    return sf_project_root_str();
}

/* PoP: _ensure_project_root_on_path_fast @ hermes_cli/main.py:_ensure_project_root_on_path_fast */
void main_g_ensure_project_root_on_path_fast(void) {
    /* Python: put PROJECT_ROOT on sys.path. The C port is a single binary,
     * so this is a no-op by construction (the project root is baked in). */
    (void)0;
}

/* PoP: _is_global_fast_version_argv @ hermes_cli/main.py:_is_global_fast_version_argv */
bool main_g_is_global_fast_version_argv(int argc, char **argv) {
    return sf_is_global_fast_version_argv(argc, argv);
}

/* PoP: _is_container_startup_environment_fast @ hermes_cli/main.py:_is_container_startup_environment_fast */
bool main_g_is_container_startup_environment_fast(void) {
    return sf_is_container_startup_environment();
}

/* PoP: _active_profile_may_override_home_fast @ hermes_cli/main.py:_active_profile_may_override_home_fast */
bool main_g_active_profile_may_override_home_fast(const char *hermes_root) {
    return sf_active_profile_may_override_home(hermes_root);
}

/* PoP: _container_mode_may_be_active_fast @ hermes_cli/main.py:_container_mode_may_be_active_fast */
bool main_g_container_mode_may_be_active_fast(void) {
    return sf_container_mode_may_be_active();
}

/* PoP: _try_ultrafast_version @ hermes_cli/main.py:_try_ultrafast_version */
bool main_g_try_ultrafast_version(int argc, char **argv) {
    /* Python: _startup_fast.try_fast_version() — handle --version/-V before
     * the heavy import wall. Termux keeps its historical contract (also
     * accepts the version subcommand); everywhere else only --version/-V,
     * and never when container mode may route the command into the
     * container. Prints fast version info and returns True when handled. */
    if (sf_is_termux_env()) {
        if (getenv("HERMES_TERMUX_DISABLE_FAST_CLI") &&
            strcmp(getenv("HERMES_TERMUX_DISABLE_FAST_CLI"), "1") == 0)
            return false;
        if (!sf_is_termux_fast_version_argv(argc, argv)) return false;
    } else {
        if (!sf_is_global_fast_version_argv(argc, argv)) return false;
        if (sf_container_mode_may_be_active()) return false;
    }
    /* print_fast_version_info() */
#ifndef HERMES_VERSION
#define HERMES_VERSION "0.20.0-slermes"
#endif
#ifndef HERMES_RELEASE_DATE
#define HERMES_RELEASE_DATE "2026.8.3"
#endif
    printf("Hermes Agent v%s (%s)\n", HERMES_VERSION, HERMES_RELEASE_DATE);
    char *root = sf_project_root_str();
    if (root) {
        printf("Install directory: %s\n", root);
        free(root);
    }
    extern char *sf_read_install_method(void);
    char *method = sf_read_install_method();
    if (method) {
        printf("Install method: %s\n", method);
        free(method);
    }
    char *oa = sf_read_openai_version();
    if (oa) {
        printf("OpenAI SDK: %s\n", oa);
        free(oa);
    } else {
        printf("OpenAI SDK: Not installed\n");
    }
    printf("Run 'hermes version' for update status.\n");
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 * _resolve_workspace_key
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _resolve_workspace_key @ hermes_cli/main.py:_resolve_workspace_key */
char *main_g_resolve_workspace_key(void) {
    /* Python: git repo root when CWD is inside a repo, else CWD itself;
     * None when neither can be determined. */
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    /* Run: git rev-parse --show-toplevel */
    char cmd[4200];
    snprintf(cmd, sizeof(cmd), "git rev-parse --show-toplevel 2>/dev/null");
    FILE *p = popen(cmd, "r");
    if (p) {
        char out[4096];
        if (fgets(out, sizeof(out), p)) {
            size_t len = strlen(out);
            while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) out[--len] = '\0';
            if (len > 0) {
                pclose(p);
                /* abspath: use as-is (git returns absolute). */
                return strdup(out);
            }
        }
        pclose(p);
    }
    return strdup(cwd);
}

/* ════════════════════════════════════════════════════════════════════
 * _parse_dashboard_runtime / _dashboard_probe_host
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _parse_dashboard_runtime @ hermes_cli/main.py:_parse_dashboard_runtime */
int main_g_parse_dashboard_runtime(const char *command, char *mode_out, size_t mode_sz,
                                   char *host_out, size_t host_sz, int *port_out) {
    /* Python: detect dashboard/serve mode, --port, --host from a cmdline.
     * Returns 1 on success (mode_out/host_out/port_out set), 0 when no
     * mode matches. */
    if (!command) return 0;
    const char *mode = NULL;
    if (strstr(command, "hermes dashboard") ||
        strstr(command, "hermes_cli.main dashboard") ||
        strstr(command, "hermes_cli/main.py dashboard"))
        mode = "dashboard";
    else if (strstr(command, "hermes serve") ||
             strstr(command, "hermes_cli.main serve") ||
             strstr(command, "hermes_cli/main.py serve"))
        mode = "serve";
    if (!mode) return 0;
    if (mode_out && mode_sz) snprintf(mode_out, mode_sz, "%s", mode);
    int port = 9119;
    const char *host = "127.0.0.1";
    /* --port=NNN or --port NNN */
    const char *pp = strstr(command, "--port");
    if (pp) {
        pp += 6;
        while (*pp == ' ' || *pp == '\t') pp++;
        if (*pp == '=') pp++;
        while (*pp == ' ' || *pp == '\t') pp++;
        if (*pp >= '0' && *pp <= '9') {
            int v = 0;
            while (*pp >= '0' && *pp <= '9') { v = v * 10 + (*pp - '0'); pp++; }
            if (v > 0 && v <= 65535) port = v;
            else return 0;
        }
    }
    /* --host=... or --host ... (quoted or bare token) */
    const char *hp = strstr(command, "--host");
    if (hp) {
        hp += 6;
        while (*hp == ' ' || *hp == '\t') hp++;
        if (*hp == '=') hp++;
        while (*hp == ' ' || *hp == '\t') hp++;
        char hbuf[256] = "";
        size_t hi = 0;
        if (*hp == '"' || *hp == '\'') {
            char q = *hp++;
            while (*hp && *hp != q && hi < sizeof(hbuf) - 1) hbuf[hi++] = *hp++;
        } else {
            while (*hp && *hp != ' ' && *hp != '\t' && hi < sizeof(hbuf) - 1)
                hbuf[hi++] = *hp++;
        }
        hbuf[hi] = '\0';
        if (hbuf[0]) host = hbuf;
    }
    if (host_out && host_sz) snprintf(host_out, host_sz, "%s", host);
    if (port_out) *port_out = port;
    return 1;
}

/* PoP: _dashboard_probe_host @ hermes_cli/main.py:_dashboard_probe_host */
void main_g_dashboard_probe_host(const char *host, char *out, size_t out_sz) {
    /* Python: map wildcard binds (0.0.0.0, ::, empty) to 127.0.0.1. */
    const char *normalized = host ? host : "127.0.0.1";
    while (*normalized == ' ' || *normalized == '\t') normalized++;
    size_t len = strlen(normalized);
    while (len > 0 && (normalized[len-1] == ' ' || normalized[len-1] == '\t')) len--;
    if (len >= 2 && normalized[0] == '[' && normalized[len-1] == ']') {
        normalized++;
        len -= 2;
    }
    char tmp[256];
    size_t tl = len < sizeof(tmp) - 1 ? len : sizeof(tmp) - 1;
    memcpy(tmp, normalized, tl);
    tmp[tl] = '\0';
    if (tl == 0 || strcmp(tmp, "0.0.0.0") == 0 || strcmp(tmp, "::") == 0)
        snprintf(out, out_sz, "127.0.0.1");
    else
        snprintf(out, out_sz, "%s", tmp);
}

/* ════════════════════════════════════════════════════════════════════
 * systemd / cgroup introspection
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _get_systemd_service_for_pid @ hermes_cli/main.py:_get_systemd_service_for_pid */
char *main_g_get_systemd_service_for_pid(long pid) {
    /* Python: read /proc/<pid>/cgroup, return the .service unit name. */
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/cgroup", pid);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *c = strchr(line, ':');
        if (!c) continue;
        char *cg = strchr(c + 1, ':');
        if (!cg) continue;
        cg++;  /* path after "0::" */
        size_t cl = strlen(cg);
        while (cl > 0 && (cg[cl-1] == '\n' || cg[cl-1] == '\r')) cg[--cl] = '\0';
        if (cl >= 8 && strcmp(cg + cl - 8, ".service") == 0) {
            char *slash = strrchr(cg, '/');
            const char *name = slash ? slash + 1 : cg;
            if (name[0]) { result = strdup(name); break; }
        }
    }
    fclose(f);
    return result;
}

/* PoP: _extract_scope_from_cgroup @ hermes_cli/main.py:_extract_scope_from_cgroup */
char *main_g_extract_scope_from_cgroup(const char *cgroup_entry) {
    /* Python: system.slice → "system", user.slice → "user". */
    if (!cgroup_entry) return NULL;
    if (strstr(cgroup_entry, "/system.slice/")) return strdup("system");
    if (strstr(cgroup_entry, "/user.slice/")) return strdup("user");
    return NULL;
}

/* PoP: _get_pid_cgroup_path @ hermes_cli/main.py:_get_pid_cgroup_path */
char *main_g_get_pid_cgroup_path(long pid) {
    /* Python: return the unified (0::) cgroup path from /proc/<pid>/cgroup. */
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/cgroup", pid);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[1024];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *c = strchr(line, ':');
        if (!c) continue;
        char *cg = strchr(c + 1, ':');
        if (!cg) continue;
        cg++;
        size_t cl = strlen(cg);
        while (cl > 0 && (cg[cl-1] == '\n' || cg[cl-1] == '\r')) cg[--cl] = '\0';
        result = strdup(cg);
        break;
    }
    fclose(f);
    return result;
}

/* PoP: _dashboard_cmdline_for_pid @ hermes_cli/main.py:_dashboard_cmdline_for_pid */
char *main_g_dashboard_cmdline_for_pid(long pid) {
    /* Python: read /proc/<pid>/cmdline (NUL-joined → spaces). */
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    for (size_t i = 0; i < n; i++)
        if (buf[i] == '\0') buf[i] = ' ';
    return strdup(buf);
}

/* PoP: _try_restart_systemd_service @ hermes_cli/main.py:_try_restart_systemd_service */
int main_g_try_restart_systemd_service(const char *unit) {
    /* Python: systemctl restart <unit>; returns 0 on success. */
    if (!unit || !unit[0]) return -1;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "systemctl restart %s 2>/dev/null", unit);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

/* PoP: _respawn_dashboard_processes @ hermes_cli/main.py:_respawn_dashboard_processes */
int main_g_respawn_dashboard_processes(const char *command_line) {
    /* Python: respawn the dashboard/serve command detached. */
    if (!command_line || !command_line[0]) return -1;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "nohup %s >/dev/null 2>&1 &", command_line);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════
 * bytecode fingerprint sweep
 * ════════════════════════════════════════════════════════════════════ */

static const char *BYTECODE_FINGERPRINT_FILE = ".bytecode-fingerprint";

/* Read the git revision fingerprint (HEAD + dirty markers) cheaply. */
static char *read_git_revision_fingerprint(const char *project_root) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/.git/HEAD", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char head[1024] = "";
    if (!fgets(head, sizeof(head), f)) { fclose(f); return NULL; }
    fclose(f);
    size_t hl = strlen(head);
    while (hl > 0 && (head[hl-1] == '\n' || head[hl-1] == '\r')) head[--hl] = '\0';
    /* Ref: "ref: refs/heads/main" → read the ref file. */
    char fp[4096];
    if (strncmp(head, "ref: ", 5) == 0) {
        snprintf(fp, sizeof(fp), "%s/.git/%s", project_root, head + 5);
        FILE *rf = fopen(fp, "r");
        if (rf) {
            char rev[128] = "";
            if (fgets(rev, sizeof(rev), rf)) {
                size_t rl = strlen(rev);
                while (rl > 0 && (rev[rl-1] == '\n' || rev[rl-1] == '\r')) rev[--rl] = '\0';
                char *out = NULL;
                asprintf(&out, "%s", rev);
                fclose(rf);
                return out;
            }
            fclose(rf);
        }
    }
    return strdup(head);
}

/* PoP: _record_bytecode_fingerprint @ hermes_cli/main.py:_record_bytecode_fingerprint */
int main_g_record_bytecode_fingerprint(const char *project_root) {
    /* Python: persist the checkout fingerprint atomically (tmp + rename).
     * Never raises; a failed write just means the next launch re-sweeps. */
    if (!project_root) return -1;
    char *fingerprint = read_git_revision_fingerprint(project_root);
    if (!fingerprint) return -1;
    char stamp[4200], tmp[4300];
    snprintf(stamp, sizeof(stamp), "%s/%s", project_root, BYTECODE_FINGERPRINT_FILE);
    snprintf(tmp, sizeof(tmp), "%s.tmp", stamp);
    FILE *f = fopen(tmp, "w");
    if (!f) { free(fingerprint); return -1; }
    fputs(fingerprint, f);
    fclose(f);
    rename(tmp, stamp);
    free(fingerprint);
    return 0;
}

/* Recursively clear __pycache__ dirs under root; returns count. */
static int clear_bytecode_cache(const char *root) {
    DIR *d = opendir(root);
    if (!d) return 0;
    int removed = 0;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (strcmp(ent->d_name, "__pycache__") == 0) {
                char cmd[4200];
                snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" 2>/dev/null", child);
                if (system(cmd) == 0) removed++;
            } else {
                removed += clear_bytecode_cache(child);
            }
        }
    }
    closedir(d);
    return removed;
}

/* PoP: _sweep_stale_bytecode_if_checkout_changed @ hermes_cli/main.py:_sweep_stale_bytecode_if_checkout_changed */
int main_g_sweep_stale_bytecode_if_checkout_changed(const char *project_root) {
    /* Python: compare the fingerprint against the stamp; sweep __pycache__
     * once when they diverge. Returns the number of cache dirs removed. */
    if (!project_root) return 0;
    char *fingerprint = read_git_revision_fingerprint(project_root);
    if (!fingerprint) return 0;
    char stamp[4200];
    snprintf(stamp, sizeof(stamp), "%s/%s", project_root, BYTECODE_FINGERPRINT_FILE);
    FILE *f = fopen(stamp, "r");
    char recorded[4096] = "";
    if (f) {
        size_t n = fread(recorded, 1, sizeof(recorded) - 1, f);
        recorded[n] = '\0';
        fclose(f);
        size_t rl = strlen(recorded);
        while (rl > 0 && (recorded[rl-1] == '\n' || recorded[rl-1] == '\r')) recorded[--rl] = '\0';
    }
    if (strcmp(recorded, fingerprint) == 0) { free(fingerprint); return 0; }
    int removed = clear_bytecode_cache(project_root);
    free(fingerprint);
    return removed;
}

/* ════════════════════════════════════════════════════════════════════
 * _pytest_owns_live_checkout / _missing_web_build_tool
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _pytest_owns_live_checkout @ hermes_cli/main.py:_pytest_owns_live_checkout */
bool main_g_pytest_owns_live_checkout(const char *root) {
    /* Python: PYTEST_CURRENT_TEST set AND root == this checkout itself. */
    if (!getenv("PYTEST_CURRENT_TEST")) return false;
    if (!root) return false;
    extern char *sf_project_root_str(void);
    char *proj = sf_project_root_str();
    if (!proj) return false;
    bool same = strcmp(root, proj) == 0;
    free(proj);
    return same;
}

/* PoP: _missing_web_build_tool @ hermes_cli/main.py:_missing_web_build_tool */
char *main_g_missing_web_build_tool(const char *output) {
    /* Python: detect which build tool (tsc/vite) a failed npm run build
     * could not resolve, across shell wordings. */
    if (!output) return NULL;
    char lowered[8192];
    size_t li = 0;
    for (const char *p = output; *p && li < sizeof(lowered) - 1; p++)
        lowered[li++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
    lowered[li] = '\0';
    const char *tools[] = { "tsc", "vite" };
    for (int t = 0; t < 2; t++) {
        const char *tool = tools[t];
        char p1[128], p2[128], p3[128];
        snprintf(p1, sizeof(p1), "%s: not found", tool);
        snprintf(p2, sizeof(p2), "%s: command not found", tool);
        snprintf(p3, sizeof(p3), "'%s' is not recognized", tool);
        if (strstr(lowered, p1) || strstr(lowered, p2) || strstr(lowered, p3))
            return strdup(tool);
    }
    return NULL;
}

/* PoP: _run_npm_watching_for_engine_failure @ hermes_cli/main.py:_run_npm_watching_for_engine_failure */
int main_g_run_npm_watching_for_engine_failure(const char *web_dir) {
    /* Python: npm run build under a lock, serializing across processes;
     * serves existing dist when another process holds the lock. The C port
     * performs the same serialized build via a lockfile + npm. */
    if (!web_dir || !web_dir[0]) return -1;
    char lock[4200];
    snprintf(lock, sizeof(lock), "%s/.web_ui_build.lock", web_dir);
    /* Try exclusive non-blocking lock. */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "if (flock -n 9) 2>/dev/null; then "
             "  cd \"%s\" && npm run build >/dev/null 2>&1; "
             "  rc=$?; "
             "  flock -u 9 2>/dev/null; "
             "  exit $rc; "
             "else "
             "  # Another process is building — serve existing dist. "
             "  exit 0; "
             "fi 9>\"%s\"", web_dir, lock);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════
 * Windows PE machine helpers (pure mapping + env probe)
 * ════════════════════════════════════════════════════════════════════ */

static const char *pe_machine_name(unsigned short machine) {
    switch (machine) {
        case 0x014c: return "I386";
        case 0x8664: return "AMD64";
        case 0xAA64: return "ARM64";
        case 0x01c0: return "ARM";
        default: return NULL;
    }
}

/* PoP: _windows_native_machine_from_iswow64 @ hermes_cli/main.py:_windows_native_machine_from_iswow64 */
char *main_g_windows_native_machine_from_iswow64(const char *iswow64_native_machine) {
    /* Python: map the IsWow64Process2 native machine code. The C port takes
     * the machine code as a decimal string (the Windows API result) and
     * maps it; NULL on unparseable input. */
    if (!iswow64_native_machine) return NULL;
    char *end = NULL;
    long v = strtol(iswow64_native_machine, &end, 0);
    if (end == iswow64_native_machine) return NULL;
    const char *name = pe_machine_name((unsigned short)v);
    return name ? strdup(name) : NULL;
}

/* PoP: _windows_user_runnable_pe_machines @ hermes_cli/main.py:_windows_user_runnable_pe_machines */
char *main_g_windows_user_runnable_pe_machines(const char *runnable_csv) {
    /* Python: the set of PE machines the host can load. The C port takes
     * the comma-separated machine codes reported by GetMachineTypeAttributes
     * and normalizes to names; NULL when none runnable. */
    if (!runnable_csv || !runnable_csv[0]) return NULL;
    json_t *arr = json_array();
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", runnable_csv);
    char *tok = strtok(buf, ",");
    while (tok) {
        char *end = NULL;
        long v = strtol(tok, &end, 0);
        const char *name = (end != tok) ? pe_machine_name((unsigned short)v) : NULL;
        if (name) json_array_append(arr, json_string(name));
        tok = strtok(NULL, ",");
    }
    char *out = json_dumps(arr, 0);
    json_free(arr);
    if (out && strcmp(out, "[]") == 0) { free(out); return NULL; }
    return out;
}

/* PoP: _windows_native_machine @ hermes_cli/main.py:_windows_native_machine */
char *main_g_windows_native_machine(const char *env_arch, const char *env_archw6432) {
    /* Python: normalized native machine; prefers PROCESSOR_ARCHITEW6432
     * (the WOW64 truth), then PROCESSOR_ARCHITECTURE, then platform.machine.
     * The C port probes the same env vars the Python reads. */
    const char *v = env_archw6432 && env_archw6432[0] ? env_archw6432
                  : env_arch && env_arch[0] ? env_arch : NULL;
    if (!v) return NULL;
    /* Normalize upper. */
    char *out = strdup(v);
    for (char *p = out; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * macOS desktop signing helpers
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _desktop_macos_bundle_id @ hermes_cli/main.py:_desktop_macos_bundle_id */
char *main_g_desktop_macos_bundle_id(const char *bundle_path) {
    /* Python: read CFBundleIdentifier from Contents/Info.plist (or the
     * framework Variants/Resources variant). The C port shells to plutil
     * (the macOS-native plist reader) — faithful read of the same key. */
    if (!bundle_path) return NULL;
    char info[4096];
    snprintf(info, sizeof(info), "%s/Contents/Info.plist", bundle_path);
    struct stat st;
    if (stat(info, &st) != 0) {
        snprintf(info, sizeof(info), "%s/Resources/Info.plist", bundle_path);
        if (stat(info, &st) != 0) return NULL;
    }
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "plutil -extract CFBundleIdentifier raw -o - \"%s\" 2>/dev/null",
             info);
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    char out[1024] = "";
    if (fgets(out, sizeof(out), p)) {
        size_t len = strlen(out);
        while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) out[--len] = '\0';
    }
    pclose(p);
    return out[0] ? strdup(out) : NULL;
}

/* PoP: _desktop_macos_local_signing_identity @ hermes_cli/main.py:_desktop_macos_local_signing_identity */
char *main_g_desktop_macos_local_signing_identity(const char *config_identity) {
    /* Python: return the opt-in keychain identity from config; None when
     * empty/unset (keeps identifier-pinned ad-hoc signing). */
    if (!config_identity) return NULL;
    const char *p = config_identity;
    while (*p == ' ' || *p == '\t') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
    if (len == 0) return NULL;
    char *out = malloc(len + 1);
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

/* PoP: _desktop_macos_has_valid_real_signature @ hermes_cli/main.py:_desktop_macos_has_valid_real_signature */
bool main_g_desktop_macos_has_valid_real_signature(const char *app_path) {
    /* Python: codesign --verify a real (non-adhoc) signature. The C port
     * runs the same codesign verification and checks the authority line
     * for a real identity. */
    if (!app_path) return false;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "codesign --verify --deep \"%s\" 2>/dev/null && "
             "codesign -dv \"%s\" 2>&1 | grep -q 'Authority='", app_path, app_path);
    return system(cmd) == 0;
}

/* PoP: _desktop_macos_local_codesign @ hermes_cli/main.py:_desktop_macos_local_codesign */
int main_g_desktop_macos_local_codesign(const char *app_path, const char *identity) {
    /* Python: ad-hoc or identity-pinned codesign of the app bundle. */
    if (!app_path) return -1;
    char cmd[16384];
    if (identity && identity[0])
        snprintf(cmd, sizeof(cmd), "codesign --force --deep --sign \"%s\" \"%s\" 2>/dev/null",
                 identity, app_path);
    else
        snprintf(cmd, sizeof(cmd), "codesign --force --deep --sign - \"%s\" 2>/dev/null",
                 app_path);
    return system(cmd) == 0 ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════
 * _resolve_deferred_platform_cli_command
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _resolve_deferred_platform_cli_command @ hermes_cli/main.py:_resolve_deferred_platform_cli_command */
int main_g_resolve_deferred_platform_cli_command(const char *command_name) {
    /* Python: materialize the deferred platform whose top-level CLI command
     * matches (registry probe). The C port probes the platform registry and
     * returns 1 when the platform is known. */
    if (!command_name || !command_name[0]) return 0;
    extern char *prg_get(const char *name);
    char *entry = prg_get(command_name);
    if (entry) { free(entry); return 1; }
    return 0;
}
