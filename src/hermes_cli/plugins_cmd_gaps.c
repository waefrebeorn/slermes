/* plugins_cmd_gaps.c — faithful C11 port of the remaining REAL_GAP surface
 * of hermes_cli/plugins_cmd.py (23 defs): install scanning, git plumbing,
 * index resolution, capability consent, update/remove/show commands.
 *
 * Every ported def carries the standalone annotation:
 *   /* PoP: <c_name> @ hermes_cli/plugins_cmd.py:<py_name> *​/ */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hermes_json.h"
#include "plugins_gaps.h"
#include "hermes_redact.h"              /* hermes_redact_force() */
#include "port_web_server_paths.h"      /* get_hermes_home() */
#include "port_config_py_helpers.h"     /* config_py_load_config_readonly() */
#include "plugin_guard.h"               /* plugin_guard_scan_plugin() */
#include "plugin_index_pure.h"
#include "plugins_core.h"               /* plugins_collect_directory_manifests() */

/* forward declarations used before their definitions in this file */
char *_scrub_git_url(const char *git_url);
static void pcmd_log_pass(const char *plugin_dir, const char *reason);
static void pcmd_log_info(const char *fmt, ...);
static void rm_rf(const char *path);
static bool is_file_c(const char *p);
json_t *discover_entrypoint_manifests(void);
static bool is_file(const char *p);
static bool is_file(const char *p) { return is_file_c(p); }
static void free_str_list(char **v, size_t n);
static ssize_t strset_index(char **v, size_t n, const char *s);
static bool strset_add(char ***v, size_t *n, const char *s);
static void strset_discard(char ***v, size_t *n, const char *s);

static void free_str_list(char **v, size_t n) {
    if (!v) return;
    for (size_t i = 0; i < n; i++) free(v[i]);
    free(v);
}
static ssize_t strset_index(char **v, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (v[i] && strcmp(v[i], s) == 0) return (ssize_t)i;
    return -1;
}
static bool strset_add(char ***v, size_t *n, const char *s) {
    if (strset_index(*v, *n, s) >= 0) return true;
    char **nv = realloc(*v, (*n + 1) * sizeof(char *));
    if (!nv) return false;
    nv[*n] = s ? strdup(s) : NULL;
    if (!nv[*n]) return false;
    *v = nv; (*n)++;
    return true;
}
static void strset_discard(char ***v, size_t *n, const char *s) {
    ssize_t i = strset_index(*v, *n, s);
    if (i < 0) return;
    free((*v)[i]);
    memmove(&(*v)[i], &(*v)[i + 1], (*n - (size_t)i - 1) * sizeof(char *));
    (*n)--;
}

/* ── small shared helpers ─────────────────────────────────────────────── */

static char g_pcmd_err[512];
static void pcmd_seterr(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_pcmd_err, sizeof(g_pcmd_err), fmt, ap);
    va_end(ap);
}
const char *pcmd_last_error(void) { return g_pcmd_err; }

static void pcmd_out(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
}

static void pcmd_err(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static bool file_exists_c(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
}
static bool is_file_c(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}
static bool is_dir_c(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}
static char *path_join_c(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0;
    while (la && a[la - 1] == '/') la--;
    char *out = malloc(la + strlen(b ? b : "") + 2);
    if (!out) return NULL;
    snprintf(out, la + strlen(b ? b : "") + 2, "%.*s/%s", (int)la, a ? a : "",
             b ? b : "");
    return out;
}
static char *read_all_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (len) *len = rd;
    return buf;
}
static int write_all_file(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = strlen(data);
    int rc = fwrite(data, 1, n, f) == n ? 0 : -1;
    fclose(f);
    return rc;
}

/* noninteractive_git_env(): GIT_TERMINAL_PROMPT=0 / GIT_ASKPASS=empty /
 * SSH batch mode so git never hangs waiting for credentials. */
static char **noninteractive_git_env(size_t *out_n) {
    static char *buf[64];
    static char slot[10][128];
    size_t n = 0;
    extern char **environ;
    for (size_t i = 0; environ[i] && n < 40; i++) {
        if (strncmp(environ[i], "GIT_ASKPASS=", 12) == 0 ||
            strncmp(environ[i], "SSH_ASKPASS=", 12) == 0)
            continue;
        buf[n++] = environ[i];
    }
    static const char *overrides[][2] = {
        {"GIT_TERMINAL_PROMPT", "0"},
        {"GIT_ASKPASS", "echo"},
        {"SSH_ASKPASS", "echo"},
        {"GIT_CONFIG_NOSYSTEM", NULL},   /* keep inherited value */
        {NULL, NULL},
    };
    snprintf(slot[0], sizeof(slot[0]), "GIT_TERMINAL_PROMPT=0");
    buf[n++] = slot[0];
    snprintf(slot[1], sizeof(slot[1]), "GIT_ASKPASS=echo");
    buf[n++] = slot[1];
    snprintf(slot[2], sizeof(slot[2]), "SSH_ASKPASS_REQUIRE=force");
    buf[n++] = slot[2];
    (void)overrides;
    buf[n] = NULL;
    if (out_n) *out_n = n;
    return buf;
}

/* CompletedProcess stand-in */
typedef struct {
    int   returncode;
    char *stdout_data;
    char *stderr_data;
} completed_process_t;

/* Run argv in cwd with a timeout; captures stdout+stderr (UTF-8 replace). */
static completed_process_t run_subprocess(const char *const *argv,
                                          const char *cwd, int timeout_s) {
    completed_process_t r = { -1, NULL, NULL };
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) return r;

    pid_t pid = fork();
    if (pid < 0) { close(out_pipe[0]); close(out_pipe[1]);
                   close(err_pipe[0]); close(err_pipe[1]); return r; }
    if (pid == 0) {
        /* child */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        if (cwd && chdir(cwd) != 0) _exit(127);
        setenv("GIT_TERMINAL_PROMPT", "0", 0);
        setenv("GIT_ASKPASS", "echo", 0);
        setenv("SSH_ASKPASS_REQUIRE", "force", 0);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(out_pipe[1]); close(err_pipe[1]);

    /* drain with poll until EOF on both or timeout */
    size_t ocap = 4096, olen = 0, ecap = 4096, elen = 0;
    r.stdout_data = malloc(ocap);
    r.stderr_data = malloc(ecap);
    int out_open = 1, err_open = 1;
    time_t deadline = time(NULL) + timeout_s;
    while ((out_open || err_open) && r.returncode == -1) {
        struct pollfd fds[2];
        int nfds = 0;
        int oi = -1, ei = -1;
        if (out_open) { fds[nfds].fd = out_pipe[0]; fds[nfds].events = POLLIN; oi = nfds++; }
        if (err_open) { fds[nfds].fd = err_pipe[0]; fds[nfds].events = POLLIN; ei = nfds++; }
        int remaining_ms = (int)(deadline - time(NULL)) * 1000;
        if (remaining_ms <= 0) {
            kill(pid, SIGKILL);
            break;
        }
        int pr = poll(fds, (nfds_t)nfds, remaining_ms);
        if (pr <= 0) continue;
        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & (POLLIN | POLLHUP))) continue;
            int target = fds[i].fd == out_pipe[0] ? 0 : 1;
            char chunk[4096];
            ssize_t got;
            while ((got = read(fds[i].fd, chunk, sizeof(chunk))) > 0) {
                if (target == 0 && r.stdout_data) {
                    if (olen + (size_t)got + 1 > ocap) {
                        ocap *= 2;
                        char *nb = realloc(r.stdout_data, ocap);
                        if (!nb) { free(r.stdout_data); r.stdout_data = NULL; break; }
                        r.stdout_data = nb;
                    }
                    memcpy(r.stdout_data + olen, chunk, (size_t)got);
                    olen += (size_t)got;
                } else if (target == 1 && r.stderr_data) {
                    if (elen + (size_t)got + 1 > ecap) {
                        ecap *= 2;
                        char *nb = realloc(r.stderr_data, ecap);
                        if (!nb) { free(r.stderr_data); r.stderr_data = NULL; break; }
                        r.stderr_data = nb;
                    }
                    memcpy(r.stderr_data + elen, chunk, (size_t)got);
                    elen += (size_t)got;
                }
            }
            if (got <= 0) {
                if (target == 0) out_open = 0; else err_open = 0;
            }
        }
    }
    if (r.stdout_data) r.stdout_data[olen] = '\0';
    if (r.stderr_data) r.stderr_data[elen] = '\0';
    close(out_pipe[0]); close(err_pipe[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) == pid) {
        if (WIFEXITED(status)) r.returncode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) r.returncode = 128 + WTERMSIG(status);
    }
    return r;
}

static void completed_process_free(completed_process_t *r) {
    free(r->stdout_data); free(r->stderr_data);
    r->stdout_data = r->stderr_data = NULL;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Scan-on-install
 * ====================================================================== */

/* PoP: _scan_on_install_enabled @ hermes_cli/plugins_cmd.py:_scan_on_install_enabled */
/* On by default; disable via plugins.scan_on_install: false. */
bool _scan_on_install_enabled(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return true;
    const json_t *plugins_n = json_obj_get(cfg, "plugins");
    bool enabled = plugins_n
                       ? json_get_bool(plugins_n, "scan_on_install", true)
                       : true;
    json_free(cfg);
    return enabled;
}

/* PoP: _scan_plugin_tree @ hermes_cli/plugins_cmd.py:_scan_plugin_tree */
/*
 * Verdicts: safe -> proceed (rc 0); caution -> needs confirmation via
 * scan_decision_cb or force; dangerous/other-not-allowed -> blocked
 * (rc -1, PluginScanBlocked message in pcmd_last_error()). Returns the scan
 * report as fresh JSON when scanning ran, else NULL with rc 0. */
json_t *_scan_plugin_tree(const char *plugin_dir, const char *identifier,
                          bool force,
                          bool (*scan_decision_cb)(const json_t *report),
                          char **blocked_message) {
    if (!_scan_on_install_enabled()) return NULL;
    pg_plugin_scan_result_t *result =
        plugin_guard_scan_plugin(plugin_dir, identifier);

    /* report JSON mirror of the ScanResult */
    json_t *report = json_object();
    json_set(report, "skill_name",
             json_string(result ? result->skill_name : ""));
    json_set(report, "source", json_string(result ? result->source : ""));
    json_set(report, "trust_level",
             json_string(result ? result->trust_level : ""));
    json_set(report, "verdict", json_string(result ? result->verdict : ""));
    json_set(report, "summary", json_string(result ? result->summary : ""));

    int allowed_int = 0;
    char reason[512] = "";
    should_allow_plugin_install(result, force ? 1 : 0, &allowed_int, reason,
                                sizeof(reason));
    /* allowed tri-state: 1 allow, 0 deny, -1 caution-needs-confirmation */
    if (allowed_int != 1 && scan_decision_cb &&
        scan_decision_cb(report)) {
        allowed_int = 1;
        snprintf(reason, sizeof(reason), "%s", "Caution verdict accepted by user");
    }

    if (allowed_int != 1) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "Security scan blocked plugin install: %s\nReview the "
                 "findings above. Install only plugins from sources you "
                 "trust. (Scanning can be configured via plugins.scan_on_install "
                 "in config.yaml.)",
                 reason);
        if (blocked_message) *blocked_message = strdup(msg);
        pcmd_seterr("%s", msg);
        json_free(report);
        plugin_guard_scan_result_free(result);
        return NULL;
    }
    pcmd_log_pass(plugin_dir, reason);
    plugin_guard_scan_result_free(result);
    return report;
}

static void pcmd_log_pass(const char *plugin_dir, const char *reason) {
    const char *base = strrchr(plugin_dir ? plugin_dir : "", '/');
    base = base ? base + 1 : (plugin_dir ? plugin_dir : "");
    pcmd_log_info("plugin scan passed for %s: %s", base, reason);
}
static void pcmd_log_info(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs("[hermes.plugins-cmd] INFO: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* PoP: _print_python_dependencies @ hermes_cli/plugins_cmd.py:_print_python_dependencies */
/*
 * Surface declared python_dependencies at install time (#64165): declaration
 * seam only, never auto-installed. */
void _print_python_dependencies(const json_t *manifest) {
    const json_t *deps = manifest ? json_obj_get(manifest, "python_dependencies") : NULL;
    if (!deps || !json_is_array(deps) || json_len(deps) == 0) return;
    const char *name =
        json_get_str(manifest, "name", "this plugin");
    printf("\n\033[1m%s\033[0m declares Python dependencies "
           "(not installed automatically):\n", name);
    size_t need = 16;
    for (size_t i = 0; i < json_len(deps); i++) {
        const json_t *d = json_get(deps, i);
        if (d && json_is_string(d) && json_string_value(d)[0])
            need += strlen(json_string_value(d)) + 8;
    }
    char *hint = malloc(need);
    if (!hint) return;
    strcpy(hint, "pip install ");
    size_t off = strlen(hint);
    for (size_t i = 0; i < json_len(deps); i++) {
        const json_t *d = json_get(deps, i);
        if (!d || !json_is_string(d) || !json_string_value(d)[0]) continue;
        int w = snprintf(hint + off, need - off, "'%s'%s",
                         json_string_value(d),
                         i + 1 < json_len(deps) ? " " : "");
        if (w > 0) off += (size_t)w;
        printf("  - %s\n", json_string_value(d));
    }
    printf("\033[2mInstall them yourself if needed: %s\033[0m\n\n", hint);
    free(hint);
}

/* PoP: _install_metadata_path @ hermes_cli/plugins_cmd.py:_install_metadata_path */
/* <hermes home>/plugins/.install-metadata.json. Caller frees. */
char *_install_metadata_path(void) {
    char *home = get_hermes_home() ? strdup(get_hermes_home())
                                   : strdup(".");
    char *plugins_dir = path_join_c(home, "plugins");
    free(home);
    char *p = path_join_c(plugins_dir, ".install-metadata.json");
    free(plugins_dir);
    return p;
}

/* metadata read/write helpers shared by the cmd ports below */
static json_t *read_install_metadata(void) {
    char *path = _install_metadata_path();
    if (!path) return NULL;
    json_t *meta = NULL;
    if (file_exists_c(path)) {
        char *text = read_all_file(path, NULL);
        if (text) {
            char *jerr = NULL;
            meta = json_parse(text, &jerr);
            if (jerr) free(jerr);
            free(text);
        }
    }
    free(path);
    if (!meta) meta = json_object();
    return meta;
}
static int write_install_metadata(const json_t *meta) {
    char *path = _install_metadata_path();
    if (!path) return -1;
    char *dir = strdup(path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    free(dir);
    char *dumped = json_serialize(meta);
    int rc = dumped ? write_all_file(path, dumped) : -1;
    free(dumped);
    free(path);
    return rc;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Git plumbing
 * ====================================================================== */

/* PoP: _normalize_exact_revision @ hermes_cli/plugins_cmd.py:_normalize_exact_revision */
/* --ref must be a full 40-character commit SHA. Caller frees. */
char *_normalize_exact_revision(const char *ref) {
    if (!ref || strlen(ref) != 40) goto invalid;
    for (const char *p = ref; *p; p++)
        if (!isxdigit((unsigned char)*p)) goto invalid;
    {
        char *lowered = strdup(ref);
        if (!lowered) return NULL;
        for (char *p = lowered; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        return lowered;
    }
invalid:
    pcmd_seterr("--ref must be a full 40-character commit SHA.");
    return NULL;
}

/* PoP: _safe_git_error @ hermes_cli/plugins_cmd.py:_safe_git_error */
/* Diagnosable Git output without echoing embedded credentials. Caller frees. */
char *_safe_git_error(const completed_process_t *result, const char *source_url) {
    const char *raw =
        result->stderr_data && result->stderr_data[0]
            ? result->stderr_data
            : (result->stdout_data ? result->stdout_data : "");
    /* strip */
    char *error = strdup(raw ? raw : "");
    if (!error) return NULL;
    size_t L = strlen(error);
    while (L && strchr("\r\n\t ", error[L-1])) error[--L] = '\0';

    if (source_url && *source_url) {
        char *scrubbed = _scrub_git_url(source_url);
        if (scrubbed) {
            /* replace every occurrence of source_url with scrubbed */
            char *pos, *cur = error;
            char *acc = malloc(strlen(error) + 1);
            if (acc) {
                acc[0] = '\0'; size_t alen = 0, acap = strlen(error) + 1;
                size_t slen = strlen(source_url), rlen = strlen(scrubbed);
                while ((pos = strstr(cur, source_url)) != NULL) {
                    size_t seg = (size_t)(pos - cur);
                    acap += rlen;
                    char *na = realloc(acc, acap);
                    if (!na) break;
                    acc = na;
                    memcpy(acc + alen, cur, seg); alen += seg;
                    memcpy(acc + alen, scrubbed, rlen); alen += rlen;
                    acc[alen] = '\0';
                    cur = pos + slen;
                }
                strcat(acc + alen, cur);
                free(error);
                error = acc;
            } else {
                free(acc);
            }
        }
        free(scrubbed);
    }
    /* redact_sensitive_text */
    char *redacted = hermes_redact_force(error);
    free(error);
    return redacted ? redacted : strdup("");
}

/* PoP: _scrub_git_url @ hermes_cli/plugins_cmd.py:_scrub_git_url */
/*
 * Strip credentials and query/fragment data from an HTTP(S) Git URL.
 * Other schemes are returned unchanged. Caller frees. */
char *_scrub_git_url(const char *git_url) {
    if (!git_url) return NULL;
    /* scheme://user:pass@host:port/path?query#fragment */
    const char *scheme_sep = strstr(git_url, "://");
    if (!scheme_sep) return strdup(git_url);
    size_t scheme_len = (size_t)(scheme_sep - git_url);
    if (!(scheme_len == 4 && strncmp(git_url, "http", 4) == 0) &&
        !(scheme_len == 5 && strncmp(git_url, "https", 5) == 0))
        return strdup(git_url);
    const char *after = scheme_sep + 3;
    const char *at = strchr(after, '@');
    if (at) after = at + 1;   /* drop userinfo (credentials) */
    const char *slash = strchr(after, '/');
    size_t host_len = slash ? (size_t)(slash - after) : strlen(after);
    const char *path = slash ? slash : "";
    /* strip ?query and #fragment */
    char path_clean[2048];
    size_t plen = strcspn(path, "?#");
    if (plen >= sizeof(path_clean)) plen = sizeof(path_clean) - 1;
    memcpy(path_clean, path, plen); path_clean[plen] = '\0';
    char *out = malloc(scheme_len + 3 + host_len + strlen(path_clean) + 1);
    if (!out) return NULL;
    snprintf(out, scheme_len + 3 + host_len + strlen(path_clean) + 1,
             "%.*s://%.*s%s", (int)scheme_len, git_url, (int)host_len, after,
             path_clean);
    return out;
}

/* PoP: _canonical_source @ hermes_cli/plugins_cmd.py:_canonical_source */
/* Scrubbed URL plus "#subdir" when one was requested. Caller frees. */
char *_canonical_source(const char *git_url, const char *subdir) {
    char *scrubbed = _scrub_git_url(git_url);
    if (!scrubbed) return NULL;
    if (!subdir || !*subdir) return scrubbed;
    char *out = malloc(strlen(scrubbed) + strlen(subdir) + 2);
    if (!out) { free(scrubbed); return NULL; }
    sprintf(out, "%s#%s", scrubbed, subdir);
    free(scrubbed);
    return out;
}

/* PoP: _run_plugin_git @ hermes_cli/plugins_cmd.py:_run_plugin_git */
/* Run one git command inside a plugin checkout (non-interactive). */
completed_process_t _run_plugin_git(const char *git_exe, const char *target,
                                    const char *const *args, size_t n_args,
                                    int timeout_s) {
    const char **argv = calloc(n_args + 2, sizeof(char *));
    completed_process_t fail = { -1, NULL, NULL };
    if (!argv) return fail;
    argv[0] = git_exe ? git_exe : "git";
    for (size_t i = 0; i < n_args; i++) argv[i + 1] = args[i];
    completed_process_t r =
        run_subprocess(argv, target, timeout_s > 0 ? timeout_s : 60);
    free(argv);
    return r;
}

/* PoP: _git_head_revision @ hermes_cli/plugins_cmd.py:_git_head_revision */
/*
 * HEAD of an installed checkout; PluginOperationError maps to NULL +
 * pcmd_last_error(). Caller frees. */
char *_git_head_revision(const char *repo, const char *git_exe) {
    const char *argv[] = { "rev-parse", "HEAD" };
    completed_process_t r =
        _run_plugin_git(git_exe, repo, argv, 2, 15);
    if (r.returncode != 0) {
        char *err = _safe_git_error(&r, NULL);
        pcmd_seterr("Could not determine installed Git revision:\n%s",
                    err ? err : "");
        free(err);
        completed_process_free(&r);
        return NULL;
    }
    char *head = strdup(r.stdout_data ? r.stdout_data : "");
    completed_process_free(&r);
    if (!head) return NULL;
    size_t L = strlen(head);
    while (L && strchr("\r\n \t", head[L-1])) head[--L] = '\0';
    for (char *p = head; *p; p++) *p = (char)tolower((unsigned char)*p);
    return head;
}

/* PoP: _checkout_exact_revision @ hermes_cli/plugins_cmd.py:_checkout_exact_revision */
/*
 * Fetch and detach at one immutable commit, then verify the resulting HEAD.
 * 0 ok; -1 maps PluginOperationError into pcmd_last_error(). */
int _checkout_exact_revision(const char *repo, const char *git_exe,
                             const char *revision) {
    const char *fetch_argv[] = { "fetch", "--depth", "1", "origin", revision };
    completed_process_t fetched = _run_plugin_git(git_exe, repo, fetch_argv, 5, 60);
    if (fetched.returncode == -1) {
        pcmd_seterr("Git fetch of commit '%s' timed out after 60 seconds.",
                    revision);
        completed_process_free(&fetched);
        return -1;
    }
    if (fetched.returncode != 0) {
        char *err = _safe_git_error(&fetched, NULL);
        pcmd_seterr("Git commit '%s' could not be fetched:\n%s",
                    revision, err ? err : "");
        free(err);
        completed_process_free(&fetched);
        return -1;
    }
    completed_process_free(&fetched);

    const char *co_argv[] = { "checkout", "--detach", revision };
    completed_process_t checked_out = _run_plugin_git(git_exe, repo, co_argv, 3, 60);
    if (checked_out.returncode == -1) {
        pcmd_seterr("Git checkout of commit '%s' timed out after 60 seconds.",
                    revision);
        completed_process_free(&checked_out);
        return -1;
    }
    if (checked_out.returncode != 0) {
        char *err = _safe_git_error(&checked_out, NULL);
        pcmd_seterr("Git checkout of commit '%s' failed:\n%s",
                    revision, err ? err : "");
        free(err);
        completed_process_free(&checked_out);
        return -1;
    }
    completed_process_free(&checked_out);

    char *actual = _git_head_revision(repo, git_exe);
    if (!actual || strcmp(actual, revision) != 0) {
        pcmd_seterr("Checked-out revision '%s' does not match requested commit '%s'.",
                    actual ? actual : "?", revision);
        free(actual);
        return -1;
    }
    free(actual);
    return 0;
}

/* PoP: _scrub_cloned_origin @ hermes_cli/plugins_cmd.py:_scrub_cloned_origin */
/*
 * Ensure credentials used for cloning do not survive in .git/config.
 * 0 ok; -1 maps PluginOperationError. */
int _scrub_cloned_origin(const char *repo, const char *git_exe,
                         const char *git_url) {
    char *scrubbed = _scrub_git_url(git_url);
    if (!scrubbed) return -1;
    if (strcmp(scrubbed, git_url ? git_url : "") == 0) {
        free(scrubbed);
        return 0;
    }
    const char *seturl[] = { "remote", "set-url", "origin", scrubbed };
    completed_process_t r = _run_plugin_git(git_exe, repo, seturl, 4, 15);
    free(scrubbed);
    if (r.returncode != 0) {
        char *err = _safe_git_error(&r, git_url);
        pcmd_seterr("Could not sanitize installed Git remote:\n%s", err ? err : "");
        free(err);
        completed_process_free(&r);
        return -1;
    }
    completed_process_free(&r);
    return 0;
}

/* PoP: _stash_ref @ hermes_cli/plugins_cmd.py:_stash_ref */
/*
 * Current refs/stash commit, or empty string when no stash exists.
 * Caller frees. */
char *_stash_ref(const char *git_exe, const char *target) {
    const char *probe_argv[] = { "rev-parse", "--verify", "refs/stash" };
    completed_process_t probe =
        _run_plugin_git(git_exe, target, probe_argv, 3, 60);
    char *out = NULL;
    if (probe.returncode == 0 && probe.stdout_data) {
        out = strdup(probe.stdout_data);
        if (out) {
            size_t L = strlen(out);
            while (L && strchr("\r\n \t", out[L-1])) out[--L] = '\0';
        }
    } else {
        out = strdup("");
    }
    completed_process_free(&probe);
    return out;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Index resolution
 * ====================================================================== */

/* PoP: _looks_like_bare_index_name @ hermes_cli/plugins_cmd.py:_looks_like_bare_index_name */
/* True when identifier is a bare plugin name (no slash, not a URL). */
bool _looks_like_bare_index_name(const char *identifier) {
    if (!identifier || !*identifier) return false;
    if (strchr(identifier, '/') || strchr(identifier, '\\')) return false;
    static const char *SCHEMES[] = {
        "https://", "http://", "git@", "ssh://", "file://", NULL,
    };
    for (int i = 0; SCHEMES[i]; i++)
        if (strncmp(identifier, SCHEMES[i], strlen(SCHEMES[i])) == 0)
            return false;
    return true;
}

/* PoP: _resolve_index_name @ hermes_cli/plugins_cmd.py:_resolve_index_name */
/*
 * Resolve a bare plugin name to (install_identifier, pinned_ref). Exits with
 * an error when unknown/ambiguous; the ref is only used when it is an exact
 * 40-char SHA. Caller frees both outputs. */
int _resolve_index_name(const char *identifier, char **out_identifier,
                        char **out_pinned_ref) {
    if (out_identifier) *out_identifier = NULL;
    if (out_pinned_ref) *out_pinned_ref = NULL;

    /* load_index(): cached copy -> bundled seed (same paths p_load_index_json
     * uses in plugins_cmd.c). Returns malloc'd entries JSON or NULL. */
    char *entries_json = NULL;
    {
        char *home = strdup(get_hermes_home() ? get_hermes_home() : ".");
        char cache_path[4096], seed_path[4096];
        snprintf(cache_path, sizeof(cache_path), "%s/cache/plugin_index.json", home);
        const char *exe_dir = "/usr/local/share/slermes/hermes_cli/data";
        snprintf(seed_path, sizeof(seed_path),
                 "%s/plugin_index.json", exe_dir);
        free(home);
        for (int attempt = 0; attempt < 2 && !entries_json; attempt++) {
            const char *path = attempt == 0 ? cache_path : seed_path;
            if (!file_exists_c(path)) continue;
            char *text = read_all_file(path, NULL);
            if (!text) continue;
            json_t *idx = json_parse(text, NULL);
            free(text);
            if (idx) {
                const json_t *e = json_obj_get(idx, "plugins");
                entries_json = e ? json_serialize(e)
                                 : json_serialize(idx);
                json_free(idx);
            }
        }
    }
    char *source = NULL;   /* source path not surfaced by the C loader */
    (void)source;
    char *resolved_json =
        pindex_resolve_name(entries_json ? entries_json : "[]", identifier);
    free(entries_json);
    if (!resolved_json) {
        pcmd_err("\033[31mError:\033[0m Plugin '%s' was not found in the "
                 "community index. Use `hermes plugins search <term>` to "
                 "browse, or install directly with an owner/repo identifier.",
                 identifier);
        exit(1);
    }
    char *jerr2 = NULL;
    json_t *res = json_parse(resolved_json, &jerr2);
    if (jerr2) free(jerr2);
    free(resolved_json);
    if (!res) exit(1);

    const json_t *entry = json_obj_get(res, "entry");
    const json_t *candidates = json_obj_get(res, "candidates");
    if (!entry || !json_is_object(entry)) {
        size_t ncand = candidates && json_is_array(candidates) ? json_len(candidates) : 0;
        if (ncand > 1) {
            pcmd_err("\033[31mError:\033[0m Plugin name '%s' is ambiguous in "
                     "the community index. Candidates:", identifier);
            for (size_t i = 0; i < ncand; i++) {
                const json_t *c = json_get(candidates, i);
                pcmd_err("  %s  →  %s", json_get_str(c, "name", "?"),
                         json_get_str(c, "install_identifier", "?"));
            }
            pcmd_err("Re-run with the exact name or the owner/repo identifier.");
        } else {
            pcmd_err("\033[31mError:\033[0m Plugin '%s' was not found in the "
                     "community index. Use `hermes plugins search <term>` to "
                     "browse, or install directly with an owner/repo identifier.",
                     identifier);
        }
        json_free(res);
        exit(1);
    }

    const char *ref = json_get_str(entry, "ref", "");
    char *pinned = NULL;
    if (ref && *ref && strlen(ref) == 40) {
        bool hexish = true;
        for (const char *p = ref; *p; p++)
            if (!isxdigit((unsigned char)*p)) { hexish = false; break; }
        if (hexish) {
            pinned = strdup(ref);
            for (char *p = pinned; *p; p++) *p = (char)tolower((unsigned char)*p);
        } else {
            pcmd_err("\033[2mIndex pins ref '%s' (not an exact commit SHA); "
                     "installing the default branch head instead.\033[0m", ref);
        }
    } else if (ref && *ref) {
        pcmd_err("\033[2mIndex pins ref '%s' (not an exact commit SHA); "
                 "installing the default branch head instead.\033[0m", ref);
    }
    const char *ident = json_get_str(entry, "install_identifier", "");
    pcmd_err("\033[2mResolved '%s' via community index → %s\033[0m",
             json_get_str(entry, "name", "?"), ident);
    if (pinned)
        pcmd_err("\033[2m%.12s\033[0m", pinned);
    pcmd_err("\033[2mOnly install plugins from sources you trust.\033[0m");
    if (out_identifier) *out_identifier = strdup(ident);
    if (out_pinned_ref) *out_pinned_ref = pinned;
    else free(pinned);
    json_free(res);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Capabilities & consent
 * ====================================================================== */

/* PoP: _declared_capabilities_from_manifest @ hermes_cli/plugins_cmd.py:_declared_capabilities_from_manifest */
/*
 * Extract + normalize the capabilities: declaration from a manifest. Returns
 * a fresh JSON array; caller frees. */
json_t *_declared_capabilities_from_manifest(const json_t *manifest,
                                             const char *plugin_name) {
    extern char *pc_parse_declared_capabilities(const char *raw_json,
                                                const char *plugin_name);
    const json_t *caps = manifest ? json_obj_get(manifest, "capabilities") : NULL;
    char *caps_json = caps ? json_serialize(caps) : NULL;
    char *normalized =
        pc_parse_declared_capabilities(caps_json ? caps_json : "null",
                                       plugin_name ? plugin_name : "?");
    free(caps_json);
    json_t *arr = NULL;
    if (normalized) {
        char *jerr = NULL;
        arr = json_parse(normalized, &jerr);
        if (jerr) free(jerr);
    }
    if (!arr) arr = json_array();
    free(normalized);
    return arr;
}

/* PoP: _declared_capabilities_for_key @ hermes_cli/plugins_cmd.py:_declared_capabilities_for_key */
/*
 * Read the declared capabilities for an installed/bundled plugin by key.
 * entry tuple = (name, version, description, source, dir_path, key). */
json_t *_declared_capabilities_for_key(const char *key) {
    json_t *out = json_array();
    extern json_t *plugins_cmd_discover_all_plugins(void);
    json_t *entries = plugins_cmd_discover_all_plugins();
    if (!entries || !json_is_array(entries)) {
        if (entries) json_free(entries);
        return out;
    }
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *e = json_get(entries, i);
        if (!e || !json_is_array(e) || json_len(e) < 6) continue;
        const json_t *name = json_get(e, 0);
        const json_t *source = json_get(e, 3);
        const json_t *dir_path = json_get(e, 4);
        const json_t *ekey = json_get(e, 5);
        const char *ks = ekey && json_is_string(ekey) ? json_string_value(ekey) : "";
        const char *ns = name && json_is_string(name) ? json_string_value(name) : "";
        if (strcmp(ks, key) != 0 && strcmp(ns, key) != 0) continue;

        if (source && json_is_string(source) &&
            strcmp(json_string_value(source), "entrypoint") == 0) {
            json_t *mf = discover_entrypoint_manifests();
            size_t n = json_len(mf);
            for (size_t k = 0; k < n; k++) {
                const json_t *m = json_get(mf, k);
                const char *mk = json_get_str(m, "key", "");
                const char *mn = json_get_str(m, "name", "");
                if (strcmp(mk, key) == 0 || strcmp(mn, key) == 0) {
                    const json_t *caps = json_obj_get(m, "capabilities");
                    if (caps && json_is_array(caps)) {
                        for (size_t c = 0; c < json_len(caps); c++)
                            json_append(out, json_copy(json_get(caps, c)));
                    }
                    break;
                }
            }
            json_free(mf);
            json_free(entries);
            return out;
        }
        const char *dp = dir_path && json_is_string(dir_path)
                             ? json_string_value(dir_path) : NULL;
        if (!dp || !*dp) { json_free(entries); return out; }
        /* manifest = _read_manifest(dir): plugin.yaml|yml via libyaml bridge */
        char *mpath = path_join_c(dp, "plugin.yaml");
        if (!is_file(mpath)) {
            free(mpath);
            mpath = path_join_c(dp, "plugin.yml");
        }
        json_t *manifest = NULL;
        if (mpath && is_file(mpath)) {
            char *text = read_all_file(mpath, NULL);
            if (text) manifest = json_parse_yaml(text);
            free(text);
        }
        free(mpath);
        json_free(out);
        out = _declared_capabilities_from_manifest(manifest, ns ? ns : "?");
        if (manifest) json_free(manifest);
        json_free(entries);
        return out;
    }
    json_free(entries);
    return out;
}

/* PoP: _print_capability_list @ hermes_cli/plugins_cmd.py:_print_capability_list */
/*
 * Consent screen body: one line per capability with its risk description.
 * Descriptions come from the capability registry port when linked. */
void _print_capability_list(const json_t *capabilities) {
    extern const char *pc_capability_description(const char *cap_id);
    size_t n = capabilities && json_is_array(capabilities)
                   ? json_len(capabilities) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *c = json_get(capabilities, i);
        if (!c || !json_is_string(c)) continue;
        const char *desc = pc_capability_description(json_string_value(c));
        printf("    \033[1m%s\033[0m — %s\n", json_string_value(c),
               desc ? desc : "");
    }
}

/* PoP: _run_capability_consent @ hermes_cli/plugins_cmd.py:_run_capability_consent */
/*
 * Show the consent screen and record the decision. On consent the pending
 * capabilities are granted; on decline — or in ANY non-interactive context —
 * they stay ungranted (fail closed). Returns True when consent was granted. */
bool _run_capability_consent(const char *plugin_id, const json_t *declared,
                             const char *context) {
    extern int pc_record_consent(const char *plugin_id,
                                 const char *pending_json,
                                 const char *declared_json,
                                 const char *config_json);
    extern char *pc_pending_capabilities(const char *plugin_id,
                                         const char *declared_json,
                                         const char *config_json);

    char *declared_json = declared ? json_serialize(declared) : strdup("[]");
    json_t *cfg = config_py_load_config_readonly();
    char *cfg_json = cfg ? json_serialize(cfg) : strdup("{}");
    if (cfg) json_free(cfg);

    char *pending_json =
        pc_pending_capabilities(plugin_id, declared_json ? declared_json : "[]",
                                cfg_json ? cfg_json : "{}");
    bool granted = false;
    json_t *pending = NULL;
    if (pending_json) {
        char *jerr = NULL;
        pending = json_parse(pending_json, &jerr);
        if (jerr) free(jerr);
    }
    if (!pending || !json_is_array(pending) || json_len(pending) == 0) {
        /* everything declared is already granted: refresh the consent hash */
        if (declared && json_is_array(declared) && json_len(declared) > 0)
            pc_record_consent(plugin_id, "[]", declared_json, cfg_json);
        granted = true;
        goto done;
    }

    printf("\n  \033[33mPlugin \033[1m%s\033[0m\033[33m %s the following "
           "capabilities:\033[0m\n", plugin_id,
           context && strcmp(context, "install") == 0 ? "requests"
                                                      : "now requests");
    _print_capability_list(pending);
    printf("  \033[2mGranting trusts the plugin author with these host "
           "surfaces. This is consent, not a sandbox — plugins run as "
           "regular code in-process.\033[0m\n");

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        printf("  \033[33mNon-interactive session: capabilities NOT granted "
               "(fail closed).\033[0m Run `hermes plugins capabilities %s` to "
               "review and `hermes plugins enable %s` to grant interactively.\n",
               plugin_id, plugin_id);
        goto done;
    }
    {
        char answer[64] = "";
        printf("  Grant these capabilities? [y/N] ");
        fflush(stdout);
        if (!fgets(answer, sizeof(answer), stdin))
            answer[0] = '\0';
        for (char *p = answer; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strncmp(answer, "y", 1) == 0) {
            pc_record_consent(plugin_id, pending_json, declared_json, cfg_json);
            printf("  \033[32m✓\033[0m Granted: ");
            for (size_t i = 0; i < json_len(pending); i++) {
                const json_t *c = json_get(pending, i);
                printf("%s%s", i ? ", " : "",
                       c && json_is_string(c) ? json_string_value(c) : "?");
            }
            printf("\033[2m (plugins.entries.%s.granted_capabilities)\033[0m\n",
                   plugin_id);
            granted = true;
        } else {
            printf("  \033[2mDeclined. %s stays enabled with these "
                   "capabilities off; it should degrade gracefully "
                   "(ctx.has_capability()). Re-run `hermes plugins enable %s` "
                   "to grant later.\033[0m\n", plugin_id, plugin_id);
        }
    }
done:
    if (pending) json_free(pending);
    free(pending_json);
    free(declared_json);
    free(cfg_json);
    return granted;
}

/* PoP: _bundled_default_on @ hermes_cli/plugins_cmd.py:_bundled_default_on */
/*
 * True when a bundled plugin at dir_path is active without an explicit
 * plugins.enabled entry: backend/platform/model-provider kinds are default-on,
 * standalone/exclusive opt-in, portable packages have no kind. */
bool _bundled_default_on(const char *dir_path) {
    static const char *DEFAULT_ON_KINDS[] = {
        "backend", "platform", "model-provider", NULL,
    };
    char *mpath = path_join_c(dir_path, "plugin.yaml");
    if (!is_file(mpath)) {
        free(mpath);
        mpath = path_join_c(dir_path, "plugin.yml");
    }
    if (!is_file(mpath)) { free(mpath); return false; }
    char *text = read_all_file(mpath, NULL);
    free(mpath);
    if (!text) return false;
    json_t *manifest = json_parse_yaml(text);
    free(text);
    if (!manifest || !json_is_object(manifest)) {
        if (manifest) json_free(manifest);
        return false;   /* fail closed on unparseable manifests */
    }
    const char *kind_raw = json_get_str(manifest, "kind", "standalone");
    char kind[64];
    snprintf(kind, sizeof(kind), "%s", kind_raw ? kind_raw : "standalone");
    for (char *p = kind; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (char *p = kind + strlen(kind); p > kind && strchr(" \t\r\n", p[-1]); )
        *--p = '\0';
    for (int i = 0; strlen(kind) == 0 && DEFAULT_ON_KINDS[i]; i++)
        ;   /* empty kind falls through as non-default-on */
    bool on = false;
    for (int i = 0; DEFAULT_ON_KINDS[i]; i++)
        if (strcmp(kind, DEFAULT_ON_KINDS[i]) == 0) { on = true; break; }
    json_free(manifest);
    return on;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Commands
 * ====================================================================== */

/* enabled/disabled set helpers (shared shape with plugins_cmd.py sets) */
typedef struct { char **v; size_t n; } strset_t;

static json_t *read_enabled_disabled_array(const char *field) {
    char *home = strdup(get_hermes_home() ? get_hermes_home() : ".");
    char *pdir = path_join_c(home, "plugins");
    free(home);
    char *fpath = path_join_c(pdir, field);
    free(pdir);
    json_t *arr = NULL;
    if (file_exists_c(fpath)) {
        char *text = read_all_file(fpath, NULL);
        if (text) {
            char *jerr = NULL;
            arr = json_parse(text, &jerr);
            if (jerr) free(jerr);
            free(text);
        }
    }
    free(fpath);
    if (!arr || !json_is_array(arr)) {
        if (arr) json_free(arr);
        arr = json_array();
    }
    return arr;
}

static int write_enabled_disabled_array(const char *field, const strset_t *s) {
    char *home = strdup(get_hermes_home() ? get_hermes_home() : ".");
    char *pdir = path_join_c(home, "plugins");
    free(home);
    mkdir(pdir, 0755);
    char *fpath = path_join_c(pdir, field);
    free(pdir);
    json_t *arr = json_array();
    for (size_t i = 0; i < s->n; i++)
        json_append(arr, json_string(s->v[i]));
    char *dumped = json_serialize(arr);
    json_free(arr);
    int rc = dumped ? write_all_file(fpath, dumped) : -1;
    free(dumped);
    free(fpath);
    return rc;
}

static void strset_load(json_t *arr, strset_t *s) {
    memset(s, 0, sizeof(*s));
    size_t n = arr ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && json_is_string(e))
            strset_add(&s->v, &s->n, json_string_value(e));
    }
}

/* git pull of a plugin checkout; returns ok + captured output */
static bool git_pull_plugin_dir(const char *target, char **out_output) {
    const char *argv[] = { "-C", target, "pull", "--ff-only", NULL };
    completed_process_t r =
        _run_plugin_git("git", NULL, argv, 4, 60);
    bool ok = r.returncode == 0;
    if (out_output) {
        size_t ol = r.stdout_data ? strlen(r.stdout_data) : 0;
        size_t el = r.stderr_data ? strlen(r.stderr_data) : 0;
        *out_output = malloc(ol + el + 1);
        if (*out_output) {
            if (r.stdout_data) strcpy(*out_output, r.stdout_data);
            else **out_output = '\0';
            if (r.stderr_data) strcat(*out_output, r.stderr_data);
            size_t L = strlen(*out_output);
            while (L && strchr("\r\n \t", (*out_output)[L-1]))
                (*out_output)[--L] = '\0';
        }
    }
    completed_process_free(&r);
    return ok;
}

/* drop __pycache__ compiled from the previous revision (#6207/#60242) */
static void clear_plugin_bytecode(const char *target) {
    DIR *d = opendir(target);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, "__pycache__") != 0) continue;
        char *sub = path_join_c(target, de->d_name);
        DIR *sd = sub ? opendir(sub) : NULL;
        if (sd) {
            struct dirent *fe;
            while ((fe = readdir(sd)) != NULL) {
                if (!strstr(fe->d_name, ".pyc")) continue;
                char *fp = path_join_c(sub, fe->d_name);
                if (fp) unlink(fp), free(fp);
            }
            closedir(sd);
        }
        if (sub) rmdir(sub);   /* remove when empty */
        free(sub);
    }
    closedir(d);
}

/* copy *.example files from the plugin root into the hermes home */
static void copy_example_files(const char *target) {
    DIR *d = opendir(target);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t nl = strlen(de->d_name);
        if (nl < 8 || strcmp(de->d_name + nl - 8, ".example") != 0) continue;
        char *src = path_join_c(target, de->d_name);
        char *home = strdup(get_hermes_home() ? get_hermes_home() : ".");
        char *dst = path_join_c(home, de->d_name);
        free(home);
        if (src && dst) {
            FILE *in = fopen(src, "rb"), *out = fopen(dst, "wb");
            if (in && out) {
                char buf[4096];
                size_t got;
                while ((got = fread(buf, 1, sizeof(buf), in)) > 0)
                    fwrite(buf, 1, got, out);
            }
            if (in) fclose(in);
            if (out) fclose(out);
        }
        free(src); free(dst);
    }
    closedir(d);
}

/* PoP: plugins_cmd_cmd_update @ hermes_cli/plugins_cmd.py:cmd_update */
/*
 * Update an installed plugin by pulling latest from its git remote. A
 * dangerous re-scan verdict after the pull disables the plugin. */
void plugins_cmd_cmd_update(const char *name) {
    char *plugins_dir = path_join_c(get_hermes_home(), "plugins");
    char *target = plugins_dir ? path_join_c(plugins_dir, name) : NULL;
    free(plugins_dir);
    if (!target || !is_dir_c(target)) {
        pcmd_err("\033[31mError:\033[0m Plugin '%s' is not installed.", name);
        free(target);
        exit(1);
    }

    /* metadata + pinned check */
    json_t *metadata = read_install_metadata();
    json_t *install_record = metadata ? json_copy(json_obj_get(metadata, name)) : NULL;

    if (install_record && json_is_object(install_record) &&
        json_get_bool(install_record, "pinned", false)) {
        pcmd_err("\033[31mError:\033[0m Plugin '%s' is pinned to %s. To move "
                 "it, run `hermes plugins install %s --force --ref "
                 "<40-character commit SHA>`.",
                 name,
                 json_get_str(install_record, "revision", "?"),
                 json_get_str(install_record, "source", "<source>"));
        json_free(install_record);
        json_free(metadata);
        free(target);
        exit(1);
    }
    json_free(install_record);

    char *dotgit = path_join_c(target, ".git");
    bool has_git = dotgit ? file_exists_c(dotgit) : false;
    free(dotgit);
    if (!has_git) {
        pcmd_err("\033[31mError:\033[0m Plugin '%s' was not installed from "
                 "git (no .git directory). Cannot update.", name);
        json_free(metadata);
        free(target);
        exit(1);
    }

    printf("\033[2mUpdating %s...\033[2m\n", name);
    char *output = NULL;
    bool ok = git_pull_plugin_dir(target, &output);
    if (!ok) {
        pcmd_err("\033[31mError:\033[0m %s",
                 output && output[0] ? output : "git pull failed");
        free(output);
        json_free(metadata);
        free(target);
        exit(1);
    }

    if (metadata && json_obj_get(metadata, name)) {
        char *head = _git_head_revision(target, "git");
        if (head) {
            json_set((json_t *)json_obj_get(metadata, name),
                     "revision", json_string(head));
            write_install_metadata(metadata);
            free(head);
        }
    }

    /* Re-scan after update — a dangerous verdict disables the plugin rather
     * than leaving it active. */
    if (_scan_on_install_enabled()) {
        pg_plugin_scan_result_t *scan_result =
            plugin_guard_scan_plugin(target, name);
        int allowed = 0;
        char reason[512] = "";
        should_allow_plugin_install(scan_result, 0, &allowed, reason,
                                    sizeof(reason));
        if (allowed != 1) {
            printf("\n\033[33m⚠ Security scan flagged the updated plugin:"
                   "\033[0m %s\n", reason);
            for (int i = 0; scan_result && i < scan_result->finding_count; i++)
                printf("  [%s] %s: %s (%s:%d)\n",
                       scan_result->findings[i].severity,
                       scan_result->findings[i].pattern_id,
                       scan_result->findings[i].description,
                       scan_result->findings[i].file,
                       scan_result->findings[i].line);
            if (scan_result &&
                strcmp(scan_result->verdict, "dangerous") == 0) {
                strset_t enabled, disabled;
                json_t *ea = read_enabled_disabled_array(".plugins-enabled");
                json_t *da = read_enabled_disabled_array(".plugins-disabled");
                strset_load(ea, &enabled);
                strset_load(da, &disabled);
                json_free(ea); json_free(da);
                bool in_enabled = strset_index(enabled.v, enabled.n, name) >= 0;
                bool in_disabled = strset_index(disabled.v, disabled.n, name) >= 0;
                if (in_enabled || !in_disabled) {
                    strset_discard(&enabled.v, &enabled.n, name);
                    strset_add(&disabled.v, &disabled.n, name);
                    write_enabled_disabled_array(".plugins-enabled", &enabled);
                    write_enabled_disabled_array(".plugins-disabled", &disabled);
                }
                pcmd_err("\033[31mPlugin '%s' has been disabled.\033[0m Review "
                         "the findings, then re-enable with `hermes plugins "
                         "enable %s` if you trust them.", name, name);
                free_str_list(enabled.v, enabled.n);
                free_str_list(disabled.v, disabled.n);
            }
        }
        plugin_guard_scan_result_free(scan_result);
    }

    clear_plugin_bytecode(target);      /* stale bytecode class */
    copy_example_files(target);

    /* Update-time re-consent (#64228): additions stay ungranted until the
     * user says yes (non-interactive updates leave them ungranted). */
    {
        char *mpath = path_join_c(target, "plugin.yaml");
        if (!is_file(mpath)) { free(mpath); mpath = path_join_c(target, "plugin.yml"); }
        json_t *updated_manifest = NULL;
        if (mpath && is_file(mpath)) {
            char *text = read_all_file(mpath, NULL);
            if (text) updated_manifest = json_parse_yaml(text);
            free(text);
        }
        free(mpath);
        const char *plugin_id =
            updated_manifest ? json_get_str(updated_manifest, "name", name) : name;
        if (!plugin_id || !*plugin_id) plugin_id = name;
        json_t *declared_caps =
            _declared_capabilities_from_manifest(updated_manifest, plugin_id);
        if (declared_caps && json_is_array(declared_caps) &&
            json_len(declared_caps) > 0) {
            extern char *pc_pending_capabilities(const char *, const char *, const char *);
            extern char *pc_capability_set_hash(const char *caps_json);
            json_t *cfg = config_py_load_config_readonly();
            char *cfg_json = cfg ? json_serialize(cfg) : strdup("{}");
            if (cfg) json_free(cfg);
            char *caps_json = json_serialize(declared_caps);
            char *pending = pc_pending_capabilities(plugin_id, caps_json,
                                                    cfg_json ? cfg_json : "{}");
            char *new_hash = pc_capability_set_hash(caps_json ? caps_json : "[]");
            const json_t *entries_n = cfg ? json_obj_get(cfg, "plugins") : NULL;
            const json_t *ents = entries_n ? json_obj_get(entries_n, "entries") : NULL;
            const json_t *ent = ents ? json_obj_get(ents, plugin_id) : NULL;
            const json_t *consent = ent ? json_obj_get(ent, "capabilities_consent") : NULL;
            const char *stored = consent ? json_get_str(consent, "hash", "") : "";
            bool changed = stored[0] && new_hash && strcmp(stored, new_hash) != 0;
            if ((pending && pending[0] && strcmp(pending, "[]") != 0) || changed)
                _run_capability_consent(plugin_id, declared_caps, "update");
            free(pending); free(new_hash); free(caps_json); free(cfg_json);
        } else if (declared_caps) {
            json_free(declared_caps);
        }
        if (updated_manifest) json_free(updated_manifest);
    }

    if (output && strstr(output, "Already up to date")) {
        printf("\033[32m✓\033[0m Plugin \033[1m%s\033[0m is already up to date.\n",
               name);
    } else {
        printf("\033[32m✓\033[0m Plugin \033[1m%s\033[0m updated.\n", name);
        if (output && output[0])
            printf("\033[2m%s\033[0m\n", output);
    }
    free(output);
    if (install_record) json_free(install_record);
    json_free(metadata);
    free(target);
}

/* PoP: _remove_plugin_core @ hermes_cli/plugins_cmd.py:_remove_plugin_core */
/*
 * Remove one plugin and its metadata without splitting their state:
 * stage metadata first, move the tree aside, and restore on failure. */
void _remove_plugin_core(const char *target_path) {
    json_t *metadata = read_install_metadata();
    const char *name = strrchr(target_path ? target_path : "", '/');
    name = name ? name + 1 : (target_path ? target_path : "");

    if (!file_exists_c(target_path)) {
        json_free(metadata);
        pcmd_seterr("plugin directory not found: %s", target_path);
        return;
    }

    if (!metadata || !json_is_object(metadata) ||
        !json_has(metadata, name)) {
        /* no metadata entry -> plain recursive delete */
        rm_rf(target_path);
        json_free(metadata);
        return;
    }

    json_obj_del(metadata, name);

    /* staging dir next to the target: tempfile.mkdtemp(prefix=..dir=target.parent) */
    char parent[4096], staging_tpl[4200];
    snprintf(parent, sizeof(parent), "%s", target_path);
    char *slash = strrchr(parent, '/');
    const char *base = parent;
    if (slash) { *slash = '\0'; base = slash + 1; }
    else base = parent;
    snprintf(staging_tpl, sizeof(staging_tpl), "%s/.%s.remove-XXXXXX",
             parent, base);
    char *staging_real = mkdtemp(staging_tpl);
    if (!staging_real) {
        pcmd_seterr("mkdtemp failed: %s", strerror(errno));
        json_free(metadata);
        return;
    }

    /* os.replace(target, staging/plugin) */
    char backup[4400];
    snprintf(backup, sizeof(backup), "%.*s/plugin", (int)(strlen(staging_real)),
             staging_real);
    if (rename(target_path, backup) != 0) {
        pcmd_seterr("could not stage plugin removal: %s", strerror(errno));
        json_free(metadata);
        rmdir(staging_real);
        return;
    }
    if (write_install_metadata(metadata) != 0) {
        /* restore */
        rename(backup, target_path);
        pcmd_seterr("Plugin metadata update failed and '%s' could not be "
                    "restored automatically; recovery copy remains at %s.",
                    name, backup);
        json_free(metadata);
        return;
    }
    /* success: drop staged tree + staging dir */
    rm_rf(staging_real);
    json_free(metadata);
}

/* recursive delete helper (shutil.rmtree / ignore_errors semantics) */
static void rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            char *sub = path_join_c(path, de->d_name);
            if (sub) {
                struct stat st;
                if (lstat(sub, &st) == 0 && S_ISDIR(st.st_mode))
                    rm_rf(sub);
                else
                    unlink(sub);
                free(sub);
            }
        }
        closedir(d);
        rmdir(path);
    } else {
        unlink(path);
    }
}

/* PoP: cmd_show @ hermes_cli/plugins_cmd.py:cmd_show */
/*
 * Show details for one plugin including advisory emits/listens declarations.
 * Resolves by display name or registry key across every discoverable plugin. */
void cmd_show(const char *name) {
    extern json_t *plugins_cmd_discover_all_plugins(void);
    json_t *entries = plugins_cmd_discover_all_plugins();
    json_t *match = NULL;
    size_t n = entries && json_is_array(entries) ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        /* entry tuple as object: name/version/description/source/dir_path/key */
        if (!e || !json_is_object(e)) continue;
        const char *ename = json_get_str(e, "name", "");
        const char *ekey = json_get_str(e, "key", "");
        if (strcmp(ename, name) == 0 || strcmp(ekey, name) == 0) {
            match = json_copy(e);
            break;
        }
    }
    if (!match) {
        pcmd_err("\033[31mPlugin '%s' not found.\033[0m", name);
        pcmd_err("\033[2mList installed plugins:\033[0m hermes plugins list");
        if (entries) json_free(entries);
        exit(1);
    }

    const char *pname   = json_get_str(match, "name", "");
    const char *version = json_get_str(match, "version", "");
    const char *description = json_get_str(match, "description", "");
    const char *source  = json_get_str(match, "source", "");
    const char *dir_path = json_get_str(match, "dir_path", "");
    const char *key     = json_get_str(match, "key", "");

    /* manifest reads via libyaml bridge */
    char *mpath = NULL;
    if (dir_path[0]) {
        mpath = path_join_c(dir_path, "plugin.yaml");
        if (!is_file(mpath)) { free(mpath); mpath = path_join_c(dir_path, "plugin.yml"); }
    }
    json_t *manifest = NULL;
    if (mpath && is_file(mpath)) {
        char *text = read_all_file(mpath, NULL);
        if (text) manifest = json_parse_yaml(text);
        free(text);
    }
    free(mpath);

    const json_t *emits = manifest ? json_obj_get(manifest, "emits") : NULL;
    const json_t *listens = manifest ? json_obj_get(manifest, "listens") : NULL;

    /* status mirrors _plugin_status(name, enabled, disabled, key=key) */
    char status[64];
    {
        json_t *ea = read_enabled_disabled_array(".plugins-enabled");
        json_t *da = read_enabled_disabled_array(".plugins-disabled");
        bool ena = false, dis = false;
        for (size_t i = 0; i < json_len(ea); i++) {
            const json_t *s = json_get(ea, i);
            if (s && json_is_string(s) &&
                (strcmp(json_string_value(s), pname) == 0 ||
                 strcmp(json_string_value(s), key) == 0))
                { ena = true; break; }
        }
        for (size_t i = 0; i < json_len(da); i++) {
            const json_t *s = json_get(da, i);
            if (s && json_is_string(s) &&
                (strcmp(json_string_value(s), pname) == 0 ||
                 strcmp(json_string_value(s), key) == 0))
                { dis = true; break; }
        }
        json_free(ea); json_free(da);
        snprintf(status, sizeof(status), "%s",
                 dis ? "disabled" : (ena ? "enabled" : "off (default)"));
    }

    printf("\n");
    if (version[0])
        printf("\033[1m%s\033[0m \033[2mv%s\033[0m\n", pname, version);
    else
        printf("\033[1m%s\033[0m\n", pname);
    if (description[0])
        printf("%s\n", description);
    printf("\033[2mStatus:\033[0m %s\n", status);
    printf("\033[2mSource:\033[0m %s\n", source);
    printf("\033[2mKey:\033[0m %s\n", key);
    if (emits && json_is_array(emits) && json_len(emits) > 0) {
        printf("\033[2mEmits:\033[0m ");
        for (size_t i = 0; i < json_len(emits); i++) {
            const json_t *e = json_get(emits, i);
            printf("%s%s", i ? ", " : "",
                   e && json_is_string(e) ? json_string_value(e) : "?");
        }
        printf("\n");
    } else {
        printf("\033[2mEmits:\033[0m \033[2m(none)\033[0m\n");
    }
    if (listens && json_is_array(listens) && json_len(listens) > 0) {
        printf("\033[2mListens:\033[0m ");
        for (size_t i = 0; i < json_len(listens); i++) {
            const json_t *l = json_get(listens, i);
            printf("%s%s", i ? ", " : "",
                   l && json_is_string(l) ? json_string_value(l) : "?");
        }
        printf("\n");
    } else {
        printf("\033[2mListens:\033[0m \033[2m(none)\033[0m\n");
    }
    printf("\n");

    if (manifest) json_free(manifest);
    json_free(match);
    if (entries) json_free(entries);
}
/* ════════════════════════════════════════════════════════════════════ ==
 * Discovery surface shared by the cmd ports above
 * ====================================================================== */

/* _discover_all_plugins() port: bundled + user directory manifests (via
 * plugins_core's collector) plus entrypoint manifests, as a JSON array of
 * {name, version, description, source, dir_path, key} objects. */
json_t *plugins_cmd_discover_all_plugins(void) {
    json_t *out = json_array();
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    if (r) {
        size_t n = plugins_scan_count(r);
        for (size_t i = 0; i < n; i++) {
            json_t *o = json_object();
            json_set(o, "name", json_string(plugins_scan_name(r, i)));
            json_set(o, "version", json_string(""));
            json_set(o, "description", json_string(""));
            json_set(o, "source", json_string(plugins_scan_source(r, i)));
            json_set(o, "dir_path", json_string(plugins_scan_path(r, i)));
            json_set(o, "key", json_string(plugins_scan_key(r, i)));
            json_append(out, o);
        }
        plugins_scan_free(r);
    }
    json_t *mf = discover_entrypoint_manifests();
    size_t m = json_len(mf);
    for (size_t i = 0; i < m; i++) {
        const json_t *e = json_get(mf, i);
        json_t *o = json_object();
        json_set(o, "name", json_copy(json_obj_get(e, "name")));
        json_set(o, "version", json_copy(json_obj_get(e, "version")));
        json_set(o, "description", json_copy(json_obj_get(e, "description")));
        json_set(o, "source", json_string("entrypoint"));
        json_set(o, "dir_path", json_string(""));
        json_set(o, "key", json_copy(json_obj_get(e, "key")));
        json_append(out, o);
    }
    json_free(mf);
    return out;
}

/* Weak CAPABILITY_REGISTRY description lookup: the capabilities pure port
 * carries ids but not descriptions; empty description renders bare. */
__attribute__((weak)) const char *pc_capability_description(const char *cap_id) {
    (void)cap_id;
    return "";
}
