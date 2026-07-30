/*
 * port_cron_scheduler_script.c — faithful C11 port of cron/scheduler.py's
 * script-execution surface:
 *   _get_script_timeout            (module/env/config resolution chain)
 *   _read_windows_pyvenv_cfg       (pyvenv.cfg parser — portable)
 *   _run_job_script                (traversal-guarded runner + redaction)
 *   _run_job_script_with_claim_heartbeat (owned one-shot claim keepalive)
 *   _parse_wake_gate               (nanoclaw wakeAgent convention)
 * plus the _sanitize_subprocess_env core from tools/environments/local.py
 * (name blocklist + dynamic-secret predicate + venv marker strip).
 *
 * Opaque plumbing reused: slermes_home(), hermes_redact(),
 * config_py_load_config_impl(), cronjobs_heartbeat_run_claim().
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hermes_json.h"
#include "slermes_home.h"
#include "cron_jobs.h"                 /* cronjobs_heartbeat_run_claim */
#include "port_config_py_helpers.h"    /* config_py_load_config_impl */

extern char **environ;
extern char *hermes_redact(const char *input);   /* agent/redact.c */

/* ================================================================
 * Script timeout resolution
 * ================================================================ */

#define DEFAULT_SCRIPT_TIMEOUT 3600
#define RUN_CLAIM_HEARTBEAT_SECONDS 60

static int g_script_timeout_override = DEFAULT_SCRIPT_TIMEOUT;

void scheduler_set_script_timeout_override(int seconds)
{
    g_script_timeout_override = seconds;
}

/* int(float(s)) with the Python failure contract: 0 on failure. */
static int parse_timeout(const char *s)
{
    if (!s || !s[0]) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    while (end && (*end == ' ' || *end == '\t')) end++;
    if (end && *end) return 0;
    if (v <= 0) return 0;
    return (int)v;
}

/* PoP: scheduler_get_script_timeout @ cron/scheduler.py:_get_script_timeout */
int scheduler_get_script_timeout(void)
{
    /* 1. Patched module value (tests / emergency monkeypatch). */
    if (g_script_timeout_override != DEFAULT_SCRIPT_TIMEOUT &&
        g_script_timeout_override > 0)
        return g_script_timeout_override;

    /* 2. Env override. */
    const char *env = getenv("HERMES_CRON_SCRIPT_TIMEOUT");
    if (env) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", env);
        /* strip */
        char *s = buf;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        if (l) {
            int t = parse_timeout(s);
            if (t > 0) return t;
        }
    }

    /* 3. Config cron.script_timeout_seconds. */
    json_t *cfg = config_py_load_config_impl(0);
    if (cfg) {
        json_t *cron_cfg = json_object_get(cfg, "cron");
        if (json_node_is_object(cron_cfg)) {
            json_t *configured =
                json_object_get(cron_cfg, "script_timeout_seconds");
            if (configured) {
                int t = 0;
                if (json_node_is_number(configured)) {
                    double d = json_number_value(configured);
                    if (d > 0) t = (int)d;
                } else if (json_node_is_string(configured)) {
                    t = parse_timeout(json_string_value(configured));
                }
                if (t > 0) { json_free(cfg); return t; }
            }
        }
        json_free(cfg);
    }

    return DEFAULT_SCRIPT_TIMEOUT;
}

/* ================================================================
 * pyvenv.cfg parser
 * ================================================================ */

/* PoP: scheduler_read_pyvenv_cfg @ cron/scheduler.py:_read_windows_pyvenv_cfg */
json_t *scheduler_read_pyvenv_cfg(const char *venv_dir)
{
    json_t *parsed = json_object();
    if (!parsed) return NULL;
    if (!venv_dir || !venv_dir[0]) return parsed;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/pyvenv.cfg", venv_dir);
    FILE *f = fopen(path, "r");
    if (!f) return parsed;   /* OSError -> {} */

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        /* strip key, lowercase */
        while (*key == ' ' || *key == '\t') key++;
        size_t kl = strlen(key);
        while (kl && (key[kl-1] == ' ' || key[kl-1] == '\t' ||
                      key[kl-1] == '\r' || key[kl-1] == '\n')) key[--kl] = '\0';
        for (size_t i = 0; i < kl; i++)
            key[i] = (char)tolower((unsigned char)key[i]);
        /* strip val */
        while (*val == ' ' || *val == '\t') val++;
        size_t vl = strlen(val);
        while (vl && (val[vl-1] == ' ' || val[vl-1] == '\t' ||
                      val[vl-1] == '\r' || val[vl-1] == '\n')) val[--vl] = '\0';
        if (kl) json_set(parsed, key, json_string(val));
    }
    fclose(f);
    return parsed;
}

/* ================================================================
 * Subprocess env sanitizer
 * (core of tools/environments/local.py:_sanitize_subprocess_env)
 * ================================================================ */

/* Static name blocklist — the union of the registry-derived names and the
 * hard-coded set in _build_provider_env_blocklist. The C tree has no live
 * provider registry import; this mirrors the full effective set. */
static const char *ENV_BLOCKLIST[] = {
    "OPENAI_BASE_URL", "OPENAI_API_KEY", "OPENAI_API_BASE", "OPENAI_ORG_ID",
    "OPENAI_ORGANIZATION", "OPENROUTER_API_KEY", "ANTHROPIC_BASE_URL",
    "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN", "LLM_MODEL", "GOOGLE_API_KEY",
    "VERTEX_CREDENTIALS_PATH", "GOOGLE_APPLICATION_CREDENTIALS",
    "DEEPSEEK_API_KEY", "MISTRAL_API_KEY", "GROQ_API_KEY",
    "TOGETHER_API_KEY", "PERPLEXITY_API_KEY", "COHERE_API_KEY",
    "FIREWORKS_API_KEY", "XAI_API_KEY", "HELICONE_API_KEY",
    "PARALLEL_API_KEY", "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL",
    "TELEGRAM_HOME_CHANNEL", "TELEGRAM_HOME_CHANNEL_NAME",
    "DISCORD_HOME_CHANNEL", "DISCORD_HOME_CHANNEL_NAME",
    "DISCORD_REQUIRE_MENTION", "DISCORD_FREE_RESPONSE_CHANNELS",
    "DISCORD_AUTO_THREAD", "SLACK_HOME_CHANNEL", "SLACK_HOME_CHANNEL_NAME",
    "SLACK_ALLOWED_USERS", "WHATSAPP_ENABLED", "WHATSAPP_MODE",
    "WHATSAPP_ALLOWED_USERS", "SIGNAL_HTTP_URL", "SIGNAL_ACCOUNT",
    "SIGNAL_ALLOWED_USERS", "SIGNAL_GROUP_ALLOWED_USERS",
    "SIGNAL_HOME_CHANNEL", "SIGNAL_HOME_CHANNEL_NAME",
    "SIGNAL_IGNORE_STORIES", "HASS_TOKEN", "HASS_URL", "EMAIL_ADDRESS",
    "EMAIL_PASSWORD", "EMAIL_IMAP_HOST", "EMAIL_SMTP_HOST",
    "EMAIL_HOME_ADDRESS", "EMAIL_HOME_ADDRESS_NAME",
    "HERMES_DASHBOARD_SESSION_TOKEN", "GATEWAY_ALLOWED_USERS",
    "GH_TOKEN", "GITHUB_APP_ID", "GITHUB_APP_PRIVATE_KEY_PATH",
    "GITHUB_APP_INSTALLATION_ID", "MODAL_TOKEN_ID", "MODAL_TOKEN_SECRET",
    "DAYTONA_API_KEY", "GATEWAY_RELAY_ID", "GATEWAY_RELAY_SECRET",
    "GATEWAY_RELAY_DELIVERY_KEY", "AWS_BEARER_TOKEN_BEDROCK",
    /* messaging bot tokens (always-strip tier) */
    "TELEGRAM_BOT_TOKEN", "DISCORD_BOT_TOKEN", "SLACK_BOT_TOKEN",
    "SLACK_APP_TOKEN", "NOUS_API_KEY",
    NULL
};

/* Active-venv markers that must not leak (#23473). */
static const char *VENV_MARKERS[] = { "VIRTUAL_ENV", "CONDA_PREFIX", NULL };

/* PoP: scheduler_env_is_internal_secret @ tools/environments/local.py:_is_hermes_internal_secret */
static bool env_is_internal_secret(const char *key)
{
    char upper[256];
    size_t n = strlen(key);
    if (n >= sizeof(upper)) n = sizeof(upper) - 1;
    for (size_t i = 0; i < n; i++)
        upper[i] = (char)toupper((unsigned char)key[i]);
    upper[n] = '\0';

    #define ENDS_WITH(s, suf) \
        (strlen(s) >= strlen(suf) && \
         strcmp((s) + strlen(s) - strlen(suf), (suf)) == 0)
    if (strncmp(upper, "AUXILIARY_", 10) == 0 &&
        (ENDS_WITH(upper, "_API_KEY") || ENDS_WITH(upper, "_BASE_URL")))
        return true;
    if (strncmp(upper, "GATEWAY_RELAY_", 14) == 0 &&
        (ENDS_WITH(upper, "_SECRET") || ENDS_WITH(upper, "_KEY") ||
         ENDS_WITH(upper, "_TOKEN")))
        return true;
    #undef ENDS_WITH
    return false;
}

/* PoP: scheduler_sanitize_subprocess_env @ tools/environments/local.py:_sanitize_subprocess_env */
char **scheduler_sanitize_subprocess_env(char *const *base_env)
{
    if (!base_env) base_env = environ;
    size_t n = 0;
    while (base_env[n]) n++;

    char **out = calloc(n + 1, sizeof(char *));
    if (!out) return NULL;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const char *entry = base_env[i];
        const char *eq = strchr(entry, '=');
        size_t klen = eq ? (size_t)(eq - entry) : strlen(entry);
        char key[256];
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, entry, klen);
        key[klen] = '\0';

        bool drop = false;
        for (const char **b = ENV_BLOCKLIST; *b; b++)
            if (strcmp(key, *b) == 0) { drop = true; break; }
        if (!drop)
            for (const char **v = VENV_MARKERS; *v; v++)
                if (strcmp(key, *v) == 0) { drop = true; break; }
        if (!drop && env_is_internal_secret(key)) drop = true;
        if (drop) continue;

        out[m] = strdup(entry);
        if (out[m]) m++;
    }
    out[m] = NULL;
    return out;
}

static void free_env_vector(char **env)
{
    if (!env) return;
    for (size_t i = 0; env[i]; i++) free(env[i]);
    free(env);
}

/* ================================================================
 * Script runner
 * ================================================================ */

/* which(1): search PATH for an executable name. Returns malloc'd path. */
static char *which_prog(const char *name)
{
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *dup = strdup(path);
    if (!dup) return NULL;
    char *save = NULL;
    for (char *dir = strtok_r(dup, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        char cand[PATH_MAX];
        snprintf(cand, sizeof(cand), "%s/%s", dir, name);
        if (access(cand, X_OK) == 0) {
            char *out = strdup(cand);
            free(dup);
            return out;
        }
    }
    free(dup);
    return NULL;
}

/* mkdir -p */
static void script_mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* Read all of fd into a malloc'd string. */
static char *read_all_fd(int fd)
{
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, 4096);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) break;
        len += (size_t)r;
    }
    buf[len] = '\0';
    return buf;
}

/* strip in place, returns same pointer */
static char *strip_inplace(char *s)
{
    if (!s) return s;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t' ||
                   s[len-1] == '\n' || s[len-1] == '\r')) s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    return s;
}

/* Redact stdout/stderr with the fallback contract of _run_job_script. */
static char *redact_or_marker(const char *text)
{
    if (!text) return strdup("");
    char *red = hermes_redact(text);
    if (!red) return strdup("[REDACTED - redaction failed]");
    return red;
}

/* PoP: scheduler_run_job_script @ cron/scheduler.py:_run_job_script */
bool scheduler_run_job_script(const char *script_path, const char *workdir,
                              char **output_out)
{
    if (output_out) *output_out = NULL;
    if (!script_path || !script_path[0]) {
        if (output_out) *output_out = strdup("Script not found: (empty)");
        return false;
    }

    /* scripts_dir = HERMES_HOME/scripts (created, resolved) */
    const char *home = slermes_home();
    if (!home) home = "/tmp/.slermes";
    char scripts_dir[PATH_MAX];
    snprintf(scripts_dir, sizeof(scripts_dir), "%s/scripts", home);
    script_mkdir_p(scripts_dir);
    char scripts_resolved[PATH_MAX];
    if (!realpath(scripts_dir, scripts_resolved))
        snprintf(scripts_resolved, sizeof(scripts_resolved), "%s", scripts_dir);

    /* expanduser + resolve against scripts dir */
    char raw[PATH_MAX];
    if (script_path[0] == '~' &&
        (script_path[1] == '/' || script_path[1] == '\0')) {
        const char *uh = getenv("HOME");
        snprintf(raw, sizeof(raw), "%s%s", uh ? uh : "",
                 script_path + 1);
    } else {
        snprintf(raw, sizeof(raw), "%s", script_path);
    }

    char joined[PATH_MAX];
    if (raw[0] == '/')
        snprintf(joined, sizeof(joined), "%s", raw);
    else
        snprintf(joined, sizeof(joined), "%s/%s", scripts_resolved, raw);

    char resolved[PATH_MAX];
    if (!realpath(joined, resolved)) {
        /* Non-existent path: normalize textually enough for the guard,
         * then report not-found if inside. Python resolve() succeeds on
         * missing files; mimic with the parent's realpath. */
        snprintf(resolved, sizeof(resolved), "%s", joined);
    }

    /* Guard: must reside within scripts dir (traversal/symlink escape). */
    size_t sd_len = strlen(scripts_resolved);
    if (!(strncmp(resolved, scripts_resolved, sd_len) == 0 &&
          (resolved[sd_len] == '/' || resolved[sd_len] == '\0'))) {
        if (output_out) {
            size_t n = strlen(scripts_resolved) + strlen(script_path) + 128;
            char *msg = malloc(n);
            if (msg)
                snprintf(msg, n,
                    "Blocked: script path resolves outside the scripts "
                    "directory (%s): '%s'", scripts_resolved, script_path);
            *output_out = msg;
        }
        return false;
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        if (output_out) {
            size_t n = strlen(resolved) + 32;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "Script not found: %s", resolved);
            *output_out = msg;
        }
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        if (output_out) {
            size_t n = strlen(resolved) + 40;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "Script path is not a file: %s", resolved);
            *output_out = msg;
        }
        return false;
    }

    int script_timeout = scheduler_get_script_timeout();

    /* Interpreter by extension: bash for .sh/.bash, python3 otherwise. */
    const char *dot = strrchr(resolved, '.');
    char suffix[16] = "";
    if (dot) {
        snprintf(suffix, sizeof(suffix), "%s", dot);
        for (char *c = suffix; *c; c++)
            *c = (char)tolower((unsigned char)*c);
    }

    char *interp = NULL;
    if (strcmp(suffix, ".sh") == 0 || strcmp(suffix, ".bash") == 0) {
        interp = which_prog("bash");
        if (!interp && access("/bin/bash", X_OK) == 0)
            interp = strdup("/bin/bash");
        if (!interp) {
            if (output_out) {
                const char *base = strrchr(resolved, '/');
                base = base ? base + 1 : resolved;
                size_t n = strlen(base) + 256;
                char *msg = malloc(n);
                if (msg)
                    snprintf(msg, n,
                        "Cannot run .sh/.bash script '%s': bash not found "
                        "on PATH. On Windows, install Git for Windows "
                        "(which ships Git Bash) or rewrite the script as "
                        "Python (.py).", base);
                *output_out = msg;
            }
            return false;
        }
    } else {
        interp = which_prog("python3");
        if (!interp) interp = which_prog("python");
        if (!interp) {
            if (output_out)
                *output_out = strdup("Script execution failed: no python "
                                     "interpreter found on PATH");
            return false;
        }
    }

    /* cwd: job workdir, else the script's parent (back-compat). */
    char cwd[PATH_MAX];
    if (workdir && workdir[0]) {
        snprintf(cwd, sizeof(cwd), "%s", workdir);
    } else {
        snprintf(cwd, sizeof(cwd), "%s", resolved);
        char *slash = strrchr(cwd, '/');
        if (slash) *slash = '\0';
    }

    /* Sanitized env. */
    char **env = scheduler_sanitize_subprocess_env(environ);

    int outp[2], errp[2];
    if (pipe(outp) != 0 || pipe(errp) != 0) {
        free(interp);
        free_env_vector(env);
        if (output_out)
            *output_out = strdup("Script execution failed: pipe() failed");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
        free(interp);
        free_env_vector(env);
        if (output_out)
            *output_out = strdup("Script execution failed: fork() failed");
        return false;
    }

    if (pid == 0) {
        /* child */
        dup2(outp[1], STDOUT_FILENO);
        dup2(errp[1], STDERR_FILENO);
        close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
        if (chdir(cwd) != 0) _exit(127);
        char *argv[3] = { interp, resolved, NULL };
        execve(interp, argv, env ? env : environ);
        _exit(127);
    }

    close(outp[1]); close(errp[1]);
    free(interp);
    free_env_vector(env);

    /* Timeout supervision: poll with waitpid(WNOHANG); read pipes as they
     * fill to avoid a full-pipe deadlock. */
    time_t deadline = time(NULL) + script_timeout;
    size_t so_cap = 8192, so_len = 0, se_cap = 8192, se_len = 0;
    char *so = malloc(so_cap), *se = malloc(se_cap);
    bool timed_out = false;
    int status = 0;

    fcntl(outp[0], F_SETFL, O_NONBLOCK);
    fcntl(errp[0], F_SETFL, O_NONBLOCK);

    for (;;) {
        /* drain both pipes */
        for (int which = 0; which < 2; which++) {
            int fd = which == 0 ? outp[0] : errp[0];
            char **buf = which == 0 ? &so : &se;
            size_t *len = which == 0 ? &so_len : &se_len;
            size_t *cap = which == 0 ? &so_cap : &se_cap;
            for (;;) {
                if (*len + 4096 + 1 > *cap) {
                    *cap *= 2;
                    char *nb = realloc(*buf, *cap);
                    if (!nb) break;
                    *buf = nb;
                }
                ssize_t r = read(fd, *buf + *len, 4096);
                if (r > 0) { *len += (size_t)r; continue; }
                break;
            }
        }

        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) break;
        if (time(NULL) > deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timed_out = true;
            break;
        }
        struct timespec ts = { 0, 50 * 1000 * 1000 };  /* 50ms */
        nanosleep(&ts, NULL);
    }
    /* final drain (blocking reads after exit) */
    fcntl(outp[0], F_SETFL, 0);
    fcntl(errp[0], F_SETFL, 0);
    for (int which = 0; which < 2; which++) {
        int fd = which == 0 ? outp[0] : errp[0];
        char **buf = which == 0 ? &so : &se;
        size_t *len = which == 0 ? &so_len : &se_len;
        size_t *cap = which == 0 ? &so_cap : &se_cap;
        for (;;) {
            if (*len + 4096 + 1 > *cap) {
                *cap *= 2;
                char *nb = realloc(*buf, *cap);
                if (!nb) break;
                *buf = nb;
            }
            ssize_t r = read(fd, *buf + *len, 4096);
            if (r <= 0) break;
            *len += (size_t)r;
        }
    }
    close(outp[0]); close(errp[0]);
    if (so) so[so_len] = '\0';
    if (se) se[se_len] = '\0';

    if (timed_out) {
        free(so); free(se);
        if (output_out) {
            size_t n = strlen(resolved) + 64;
            char *msg = malloc(n);
            if (msg)
                snprintf(msg, n, "Script timed out after %ds: %s",
                         script_timeout, resolved);
            *output_out = msg;
        }
        return false;
    }

    /* strip + redact both streams before ANY return path */
    strip_inplace(so ? so : (char *)"");
    strip_inplace(se ? se : (char *)"");
    char *stdout_red = redact_or_marker(so ? so : "");
    char *stderr_red = redact_or_marker(se ? se : "");
    free(so); free(se);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    if (exit_code != 0) {
        size_t n = strlen(stdout_red) + strlen(stderr_red) + 128;
        char *msg = malloc(n);
        if (msg) {
            int off = snprintf(msg, n, "Script exited with code %d",
                               exit_code);
            if (stderr_red[0])
                off += snprintf(msg + off, n - (size_t)off,
                                "\nstderr:\n%s", stderr_red);
            if (stdout_red[0])
                snprintf(msg + off, n - (size_t)off,
                         "\nstdout:\n%s", stdout_red);
        }
        free(stdout_red); free(stderr_red);
        if (output_out) *output_out = msg;
        return false;
    }

    free(stderr_red);
    if (output_out) *output_out = stdout_red;
    else free(stdout_red);
    return true;
}

/* ================================================================
 * Claim-heartbeat wrapper
 * ================================================================ */

typedef struct {
    char job_id[128];
    char owner[300];
    pthread_mutex_t mu;
    pthread_cond_t cond;
    bool stop;
} heartbeat_ctx_t;

static void *heartbeat_loop(void *arg)
{
    heartbeat_ctx_t *ctx = arg;
    pthread_mutex_lock(&ctx->mu);
    for (;;) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += RUN_CLAIM_HEARTBEAT_SECONDS;
        int rc = 0;
        while (!ctx->stop && rc != ETIMEDOUT)
            rc = pthread_cond_timedwait(&ctx->cond, &ctx->mu, &ts);
        if (ctx->stop) break;
        pthread_mutex_unlock(&ctx->mu);
        cronjobs_heartbeat_run_claim(ctx->job_id, ctx->owner);
        pthread_mutex_lock(&ctx->mu);
    }
    pthread_mutex_unlock(&ctx->mu);
    return NULL;
}

/* PoP: scheduler_run_job_script_with_claim_heartbeat @ cron/scheduler.py:_run_job_script_with_claim_heartbeat */
bool scheduler_run_job_script_with_claim_heartbeat(const json_t *job,
                                                   const char *script_path,
                                                   const char *workdir,
                                                   char **output_out)
{
    /* Only owned one-shot claims heartbeat; everything else runs plain. */
    const json_t *schedule =
        job ? json_object_get((json_t *)job, "schedule") : NULL;
    const json_t *claim =
        job ? json_object_get((json_t *)job, "run_claim") : NULL;
    const char *kind = json_node_is_object(schedule)
        ? json_get_str((json_t *)schedule, "kind", "") : "";
    const char *owner = json_node_is_object(claim)
        ? json_get_str((json_t *)claim, "by", "") : "";
    if (strcmp(kind, "once") != 0 || !owner[0])
        return scheduler_run_job_script(script_path, workdir, output_out);

    heartbeat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.job_id, sizeof(ctx.job_id), "%s",
             json_get_str((json_t *)job, "id", ""));
    snprintf(ctx.owner, sizeof(ctx.owner), "%s", owner);
    pthread_mutex_init(&ctx.mu, NULL);
    pthread_cond_init(&ctx.cond, NULL);
    ctx.stop = false;

    pthread_t th;
    if (pthread_create(&th, NULL, heartbeat_loop, &ctx) != 0) {
        pthread_cond_destroy(&ctx.cond);
        pthread_mutex_destroy(&ctx.mu);
        return scheduler_run_job_script(script_path, workdir, output_out);
    }

    bool ok = scheduler_run_job_script(script_path, workdir, output_out);

    pthread_mutex_lock(&ctx.mu);
    ctx.stop = true;
    pthread_cond_broadcast(&ctx.cond);
    pthread_mutex_unlock(&ctx.mu);
    pthread_join(th, NULL);
    pthread_cond_destroy(&ctx.cond);
    pthread_mutex_destroy(&ctx.mu);
    return ok;
}

/* ================================================================
 * Wake gate
 * ================================================================ */

/* PoP: scheduler_parse_wake_gate @ cron/scheduler.py:_parse_wake_gate */
bool scheduler_parse_wake_gate(const char *script_output)
{
    if (!script_output || !script_output[0]) return true;

    /* last non-empty line */
    const char *last_start = NULL;
    size_t last_len = 0;
    const char *p = script_output;
    while (*p) {
        const char *line_start = p;
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        /* is the line non-blank? */
        bool blank = true;
        for (size_t i = 0; i < len; i++) {
            if (line_start[i] != ' ' && line_start[i] != '\t' &&
                line_start[i] != '\r') { blank = false; break; }
        }
        if (!blank) { last_start = line_start; last_len = len; }
        if (!nl) break;
        p = nl + 1;
    }
    if (!last_start) return true;

    /* strip the line */
    while (last_len && (last_start[0] == ' ' || last_start[0] == '\t' ||
                        last_start[0] == '\r')) { last_start++; last_len--; }
    while (last_len && (last_start[last_len-1] == ' ' ||
                        last_start[last_len-1] == '\t' ||
                        last_start[last_len-1] == '\r')) last_len--;
    char *line = malloc(last_len + 1);
    if (!line) return true;
    memcpy(line, last_start, last_len);
    line[last_len] = '\0';

    char *err = NULL;
    json_t *gate = json_parse(line, &err);
    free(line);
    free(err);
    if (!gate) return true;

    bool wake = true;
    if (json_node_is_object(gate)) {
        json_t *flag = json_object_get(gate, "wakeAgent");
        /* only an explicit boolean false gates the agent off */
        if (flag && flag->type == JSON_BOOL && !json_bool_value(flag))
            wake = false;
    }
    json_free(gate);
    return wake;
}
