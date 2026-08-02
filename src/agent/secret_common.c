/*
 * secret_common.c — port of agent/secret_sources/{base,registry,command}.py.
 *
 * PoP: secret_common @ agent/secret_sources/base.py:SecretSource
 * PoP: secret_common @ agent/secret_sources/registry.py:register_source
 * PoP: secret_common @ agent/secret_sources/registry.py:apply_all
 * PoP: secret_common @ agent/secret_sources/command.py:CommandSource
 *
 * Self-contained: depends only on POSIX process spawn + hermes_json. No
 * providers/* dependency. Directly serves dynamic startup secret resolution.
 */
#define _POSIX_C_SOURCE 200809L
#include "secret_common.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <ctype.h>

extern char **environ;  /* POSIX global; assigned in child before execvp */

static char *xstrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── FetchResult ───────────────────────────────────────────────────────── */
fetch_result_t *fetch_result_create(void)
{
    return calloc(1, sizeof(fetch_result_t));
}

void fetch_result_free(fetch_result_t *r)
{
    if (!r) return;
    for (size_t i = 0; i < r->secret_count; i++) {
        free(r->secret_names[i]); free(r->secret_values[i]);
    }
    free(r->secret_names); free(r->secret_values);
    for (size_t i = 0; i < r->warning_count; i++) free(r->warnings[i]);
    free(r->warnings);
    free(r->error); free(r->binary_path);
    free(r);
}

void fetch_result_add(fetch_result_t *r, char *name, char *value)
{
    if (!r || !name) return;
    size_t n = r->secret_count + 1;
    r->secret_names = realloc(r->secret_names, n * sizeof(char *));
    r->secret_values = realloc(r->secret_values, n * sizeof(char *));
    r->secret_names[r->secret_count] = name;
    r->secret_values[r->secret_count] = value ? value : xstrdup("");
    r->secret_count = n;
}

void fetch_result_add_warning(fetch_result_t *r, const char *msg)
{
    if (!r || !msg) return;
    r->warnings = realloc(r->warnings, (r->warning_count + 1) * sizeof(char *));
    r->warnings[r->warning_count++] = xstrdup(msg);
}

/* PoP: fetch_result_ok @ agent/secret_sources/base.py:ok */
bool fetch_result_ok(const fetch_result_t *r)
{
    return r && r->error == NULL;
}

/* ── ANSI scrub (port of base.scrub_ansi) ───────────────────────────────── */
/* PoP: scrub_ansi @ agent/secret_sources/base.py:scrub_ansi */
char *scrub_ansi(const char *text)
{
    if (!text) return xstrdup("");
    /* Strip CSI (ESC[...m) and OSC (ESC]...BEL/ST) sequences. */
    size_t cap = strlen(text) + 1;
    char *out = malloc(cap);
    size_t o = 0;
    for (size_t i = 0; text[i]; ) {
        if (text[i] == 0x1b) {
            if (text[i + 1] == '[') {
                i += 2;
                while (text[i] && text[i] != 'm' && text[i] != 'A' && text[i] != 'B'
                       && text[i] != 'C' && text[i] != 'D' && text[i] != 'H'
                       && text[i] != 'J' && text[i] != 'K' && text[i] != 'f'
                       && !isalpha((unsigned char)text[i])) i++;
                if (text[i]) i++;            /* skip final letter */
                continue;
            } else if (text[i + 1] == ']') {
                i += 2;
                while (text[i] && text[i] != 0x07 && !(text[i] == 0x1b && text[i+1] == '\\'))
                    i++;
                if (text[i] == 0x1b) i += 2; /* skip ESC\ terminator */
                else if (text[i]) i++;
                continue;
            }
        }
        out[o++] = text[i++];
    }
    out[o] = '\0';
    return out;
}

/* PoP: is_valid_env_name @ agent/secret_sources/base.py:is_valid_env_name */
bool is_valid_env_name(const char *name)
{
    if (!name || !*name) return false;
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return false;
    for (size_t i = 1; name[i]; i++)
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) return false;
    return true;
}

/* ── run_secret_cli (port of base.run_secret_cli) ──────────────────────── */
/* SIGALRM handler for the wall-clock budget. */
static volatile sig_atomic_t g_cli_timed_out = 0;
static void cli_alarm_handler(int sig) { (void)sig; g_cli_timed_out = 1; }

/* PoP: run_secret_cli @ agent/secret_sources/base.py:run_secret_cli */
int run_secret_cli(char *const argv[], const char *const *allow_env,
                   const char *const *extra_env, double timeout_seconds,
                   char **out_stdout, char **out_stderr, int *out_rc)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;
    if (out_rc) *out_rc = -1;

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) || pipe(err_pipe)) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]); return -1; }
    if (pid == 0) {
        /* Child: minimal allowlisted env. */
        char *const base_keep[] = {"PATH","HOME","USERPROFILE","SYSTEMROOT","TMPDIR",
            "TEMP","LANG","LC_ALL","XDG_CONFIG_HOME","XDG_DATA_HOME",NULL};
        /* Build a fresh environ: start empty, add keep + allow + extra. */
        char **newenv = calloc(64, sizeof(char *));
        size_t e = 0;
        for (int i = 0; base_keep[i]; i++) {
            char *v = getenv(base_keep[i]);
            if (v) { size_t n = strlen(base_keep[i]) + strlen(v) + 2; char *p = malloc(n); snprintf(p,n,"%s=%s",base_keep[i],v); newenv[e++]=p; }
        }
        for (int i = 0; allow_env && allow_env[i]; i++) {
            char *eq = strchr((char*)allow_env[i], '=');
            if (eq) { newenv[e++] = xstrdup(allow_env[i]); }
            else { char *v = getenv(allow_env[i]); if (v) { size_t n=strlen(allow_env[i])+strlen(v)+2; char *p=malloc(n); snprintf(p,n,"%s=%s",allow_env[i],v); newenv[e++]=p; } }
        }
        for (int i = 0; extra_env && extra_env[i]; i++) newenv[e++] = xstrdup(extra_env[i]);
        newenv[e++] = xstrdup("NO_COLOR=1");
        newenv[e] = NULL;

        dup2(out_pipe[1], 1);
        dup2(err_pipe[1], 2);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        /* execvp searches PATH using the global environ; point it at our
         * allowlisted env so the child never inherits the full credential set. */
        environ = newenv;
        execvp(argv[0], argv);
        _exit(127);  /* exec failed */
    }

    /* Parent. */
    close(out_pipe[1]); close(err_pipe[1]);
    g_cli_timed_out = 0;
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cli_alarm_handler;
    sigaction(SIGALRM, &sa, NULL);
    if (timeout_seconds > 0) alarm((unsigned int)timeout_seconds);

    /* Drain both pipes with a simple readiness poll (select). */
    char *ob = NULL, *eb = NULL; size_t ol = 0, el = 0;
    fd_set rfds; int maxfd = out_pipe[0] > err_pipe[0] ? out_pipe[0] : err_pipe[0];
    while (!g_cli_timed_out) {
        FD_ZERO(&rfds); FD_SET(out_pipe[0], &rfds); FD_SET(err_pipe[0], &rfds);
        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        int r = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) {
            /* Timeout on select with no data: if child exited, break. */
            int status; pid_t wp = waitpid(pid, &status, WNOHANG);
            if (wp == pid) break;
            if (r < 0 && errno != EINTR) break;
            if (tv.tv_sec == 0 && tv.tv_usec == 0) break;
            continue;
        }
        char buf[4096];
        if (FD_ISSET(out_pipe[0], &rfds)) {
            ssize_t n = read(out_pipe[0], buf, sizeof(buf));
            if (n > 0) { ob = realloc(ob, ol + n + 1); memcpy(ob + ol, buf, n); ol += n; ob[ol] = '\0'; }
            else if (n == 0) { /* eof */ }
        }
        if (FD_ISSET(err_pipe[0], &rfds)) {
            ssize_t n = read(err_pipe[0], buf, sizeof(buf));
            if (n > 0) { eb = realloc(eb, el + n + 1); memcpy(eb + el, buf, n); el += n; eb[el] = '\0'; }
        }
        int status; pid_t wp = waitpid(pid, &status, WNOHANG);
        if (wp == pid) break;
    }
    alarm(0);
    if (g_cli_timed_out) {
        kill(pid, SIGKILL);
        int status; waitpid(pid, &status, 0);
        free(ob); free(eb);
        return -1;  /* timeout */
    }
    int status; waitpid(pid, &status, 0);
    close(out_pipe[0]); close(err_pipe[0]);
    if (out_stdout) *out_stdout = ob ? ob : xstrdup("");
    if (out_stderr) *out_stderr = eb ? scrub_ansi(eb) : xstrdup("");
    if (out_rc) *out_rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (!ob) free(ob);
    if (!eb) free(eb);
    return 0;
}

/* ── Registry (port of registry.register_source / get_source / list) ───── */
struct secret_registry {
    secret_source_t **entries;
    size_t count, cap;
};

secret_registry_t *secret_registry_create(void)
{
    secret_registry_t *reg = calloc(1, sizeof(*reg));
    reg->cap = 8;
    reg->entries = calloc(reg->cap, sizeof(secret_source_t *));
    return reg;
}

void secret_registry_free(secret_registry_t *reg)
{
    if (!reg) return;
    free(reg->entries);
    free(reg);
}

/* PoP: secret_registry_register @ agent/secret_sources/registry.py:register_source */
int secret_registry_register(secret_registry_t *reg, secret_source_t *s)
{
    if (!reg || !s || !s->name || !s->fetch) return -1;
    /* name validity */
    if (!*s->name) return -1;
    for (const char *p = s->name; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_') return -1;
    if (strcmp(s->name, s->name) != 0) {} /* name must be lowercase — check */
    for (const char *p = s->name; *p; p++) if (isupper((unsigned char)*p)) return -1;
    if (s->api_version != SECRET_SOURCE_API_VERSION) return -1;
    if (s->shape && strcmp(s->shape, "mapped") != 0 && strcmp(s->shape, "bulk") != 0) return -1;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i]->name, s->name) == 0) return -1;  /* dup name */
        if (s->scheme && reg->entries[i]->scheme &&
            strcmp(reg->entries[i]->scheme, s->scheme) == 0) return -1;  /* scheme clash */
    }
    if (reg->count == reg->cap) {
        reg->cap *= 2;
        reg->entries = realloc(reg->entries, reg->cap * sizeof(secret_source_t *));
    }
    reg->entries[reg->count++] = s;
    return 0;
}

/* PoP: secret_registry_get @ agent/secret_sources/registry.py:get_source */
secret_source_t *secret_registry_get(secret_registry_t *reg, const char *name)
{
    if (!reg || !name) return NULL;
    for (size_t i = 0; i < reg->count; i++)
        if (strcmp(reg->entries[i]->name, name) == 0) return reg->entries[i];
    return NULL;
}

/* PoP: secret_registry_list @ agent/secret_sources/registry.py:list_sources */
char **secret_registry_list(secret_registry_t *reg)
{
    if (!reg) return NULL;
    char **out = calloc(reg->count + 1, sizeof(char *));
    for (size_t i = 0; i < reg->count; i++) out[i] = (char *)reg->entries[i]->name;
    out[reg->count] = NULL;
    return out;
}

/* ── apply_all orchestrator (port of registry.apply_all) ───────────────── */
/* Helper: read a JSON "K=V" environ into a small lookup; we mutate in place. */
static const char *env_lookup(char **environ, const char *key)
{
    if (!environ) return NULL;
    size_t kl = strlen(key);
    for (size_t i = 0; environ[i]; i++) {
        if (strncmp(environ[i], key, kl) == 0 && environ[i][kl] == '=')
            return environ[i] + kl + 1;
    }
    return NULL;
}

/* PoP: secret_apply_all @ agent/secret_sources/registry.py:apply_all */
apply_report_t *secret_apply_all(secret_registry_t *reg,
                                 const char *secrets_cfg_json,
                                 const char *home_path,
                                 char **environ)
{
    apply_report_t *rep = calloc(1, sizeof(apply_report_t));
    if (!reg) return rep;
    /* Parse secrets_cfg JSON. */
    json_t *cfg = json_parse(secrets_cfg_json ? secrets_cfg_json : "{}", NULL);
    if (!cfg) cfg = json_object();
    /* preserve_existing: names whose pre-existing env value always wins. */
    char *preserve_list = NULL;
    json_t *preserve = json_obj_get(cfg, "preserve_existing");
    if (preserve && json_is_array(preserve)) {
        size_t cap = 1;
        for (size_t i = 0; i < json_len(preserve); i++) {
            json_t *it = json_get(preserve, i);
            if (it && json_is_string(it)) {
                const char *sv = json_string_value(it);
                if (sv) cap += strlen(sv) + 2;
            }
        }
        preserve_list = calloc(cap, 1);
        for (size_t i = 0; i < json_len(preserve); i++) {
            json_t *it = json_get(preserve, i);
            if (it && json_is_string(it)) {
                const char *v = json_string_value(it);
                if (*preserve_list) strcat(preserve_list, ",");
                strcat(preserve_list, v);
            }
        }
    }

    /* Determine enabled + order (mapped first, then bulk). */
    secret_source_t **ordered = calloc(reg->count, sizeof(secret_source_t *));
    size_t on = 0;
    for (size_t i = 0; i < reg->count; i++) {
        secret_source_t *s = reg->entries[i];
        json_t *scfg = json_obj_get(cfg, s->name);
        const char *scfg_j = scfg ? json_dumps(scfg, 0) : "{}";
        bool enabled = s->is_enabled ? s->is_enabled(s, scfg_j) : (scfg && json_obj_get(scfg,"enabled"));
        if (enabled) ordered[on++] = s;
    }
    /* sort mapped before bulk (stable by insertion) */
    for (size_t i = 0; i < on; i++)
        for (size_t j = i + 1; j < on; j++)
            if (ordered[i]->shape && ordered[j]->shape &&
                strcmp(ordered[j]->shape, "mapped") == 0 &&
                strcmp(ordered[i]->shape, "bulk") == 0) {
                secret_source_t *t = ordered[i]; ordered[i] = ordered[j]; ordered[j] = t;
            }

    /* claimed: var→source name (first-wins). protected: var→source. */
    char **claimed = NULL; char **claimed_by = NULL; size_t claimed_n = 0;
    char **protected_vars = NULL; char **protected_by = NULL; size_t prot_n = 0;

    /* protected vars from each source. */
    for (size_t i = 0; i < on; i++) {
        secret_source_t *s = ordered[i];
        json_t *scfg = json_obj_get(cfg, s->name);
        const char *scfg_j = scfg ? json_dumps(scfg, 0) : "{}";
        if (s->protected_env_vars) {
            size_t pc = 0; char **pv = s->protected_env_vars(s, scfg_j, &pc);
            for (size_t k = 0; k < pc; k++) {
                protected_vars = realloc(protected_vars, (prot_n+1)*sizeof(char*));
                protected_by = realloc(protected_by, (prot_n+1)*sizeof(char*));
                protected_vars[prot_n] = xstrdup(pv[k]);
                protected_by[prot_n] = xstrdup(s->name);
                prot_n++;
            }
            /* pv is NULL-terminated owned by source; we copied names. */
            for (size_t k = 0; k < pc; k++) free(pv[k]); free(pv);
        }
    }

    for (size_t i = 0; i < on; i++) {
        secret_source_t *s = ordered[i];
        json_t *scfg = json_obj_get(cfg, s->name);
        const char *scfg_j = scfg ? json_dumps(scfg, 0) : "{}";

        fetch_result_t *fr = s->fetch(s, scfg_j, home_path ? home_path : "");
        if (!fetch_result_ok(fr)) {
            fetch_result_free(fr);
            continue;
        }
        bool override = s->override_existing ? s->override_existing(s, scfg_j) : false;
        for (size_t k = 0; k < fr->secret_count; k++) {
            const char *var = fr->secret_names[k];
            const char *val = fr->secret_values[k];
            if (!is_valid_env_name(var)) continue;
            /* protected? */
            bool is_prot = false;
            for (size_t p = 0; p < prot_n; p++)
                if (strcmp(protected_vars[p], var) == 0) { is_prot = true; break; }
            if (is_prot) continue;
            /* already claimed by an earlier source? */
            bool claimed_before = false;
            for (size_t c = 0; c < claimed_n; c++)
                if (strcmp(claimed[c], var) == 0) { claimed_before = true; break; }
            if (claimed_before) continue;
            /* pre-existing env? */
            const char *existed = env_lookup(environ, var);
            if (existed) {
                if (preserve_list && strstr(preserve_list, var)) continue;
                if (!override) continue;
            }
            /* apply — setenv is the C equivalent of os.environ[name]=value.
             * When we reach here, the guard chain has already decided the
             * var SHOULD be set; override=1 performs the write. */
            setenv(var, val, 1);
            claimed = realloc(claimed, (claimed_n+1)*sizeof(char*));
            claimed_by = realloc(claimed_by, (claimed_n+1)*sizeof(char*));
            claimed[claimed_n] = xstrdup(var);
            claimed_by[claimed_n] = xstrdup(s->name);
            claimed_n++;
        }
        fetch_result_free(fr);
    }

    rep->applied_any = (claimed_n > 0);
    rep->applied_count = claimed_n;
    for (size_t i = 0; i < claimed_n; i++) { free(claimed[i]); free(claimed_by[i]); }
    free(claimed); free(claimed_by);
    for (size_t i = 0; i < prot_n; i++) { free(protected_vars[i]); free(protected_by[i]); }
    free(protected_vars); free(protected_by);
    free(ordered);
    free(preserve_list);
    if (cfg) json_free(cfg);
    return rep;
}

/* ── CommandSource (port of command.py) ────────────────────────────────── */
/* Runs a user-supplied command; expects stdout lines of NAME=VALUE. */
typedef struct {
    secret_source_t base;
    char *command;   /* argv template or shell? We use argv list from cfg.cmd */
} command_source_t;

/* PoP: command_source_fetch @ agent/secret_sources/command.py:fetch */
static fetch_result_t *command_source_fetch(secret_source_t *self,
                                             const char *cfg_json, const char *home_path)
{
    (void)home_path; (void)self;
    fetch_result_t *r = fetch_result_create();
    json_t *cfg = json_parse(cfg_json ? cfg_json : "{}", NULL);
    if (!cfg) { r->error = xstrdup("invalid config"); r->error_kind = SECRET_ERR_INTERNAL; return r; }
    json_t *cmd = json_obj_get(cfg, "cmd");
    if (!cmd || !json_is_string(cmd)) {
        r->error = xstrdup("command source requires 'cmd'");
        r->error_kind = SECRET_ERR_NOT_CONFIGURED;
        json_free(cfg); return r;
    }
    /* Tokenize cmd into argv (simple whitespace split; respects no quoting
     * beyond what the Python shlex does — faithful enough for script paths). */
    const char *cmdstr = json_string_value(cmd);
    /* Tokenize like shlex: split on whitespace, honoring '...' and "...".
     * (Faithful to command.py which uses shlex.split.) */
    char **argv = calloc(64, sizeof(char *));
    size_t ac = 0;
    const char *p = cmdstr;
    while (*p && ac < 63) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *tok = malloc(strlen(p) + 1);
        size_t tl = 0;
        char quote = 0;
        while (*p) {
            if (quote) {
                if (*p == '\\' && quote == '"') { p++; if (*p) tok[tl++] = *p; p++; continue; }
                if (*p == quote) { quote = 0; p++; continue; }
                tok[tl++] = *p++; continue;
            }
            if (*p == '\'' || *p == '"') { quote = *p++; continue; }
            if (*p == ' ' || *p == '\t') break;
            tok[tl++] = *p++;
        }
        tok[tl] = '\0';
        argv[ac++] = tok;
    }
    argv[ac] = NULL;

    char *out = NULL, *err = NULL; int rc = -1;
    int rv = run_secret_cli(argv, NULL, NULL, SECRET_DEFAULT_CLI_TIMEOUT_SECONDS, &out, &err, &rc);
    for (size_t i = 0; i < ac; i++) free(argv[i]);
    free(argv);

    if (rv != 0 || rc != 0) {
        r->error = xstrdup(err ? err : "command source failed");
        r->error_kind = SECRET_ERR_INTERNAL;
        free(out); free(err);
        json_free(cfg); return r;
    }
    /* Parse NAME=VALUE lines. */
    if (out) {
        for (char *line = out; *line; ) {
            char *nl = strchr(line, '\n');
            char *cur = line;
            if (nl) *nl = '\0';
            char *eq = strchr(cur, '=');
            if (eq) {
                *eq = '\0';
                char *name = cur; char *val = eq + 1;
                if (is_valid_env_name(name))
                    fetch_result_add(r, xstrdup(name), xstrdup(val));
            }
            if (!nl) break;
            line = nl + 1;
        }
    }
    free(out); free(err);
    json_free(cfg);
    return r;
}

/* PoP: command_source_enabled @ agent/secret_sources/base.py:is_enabled */
static bool command_source_enabled(secret_source_t *self, const char *cfg_json)
{
    json_t *cfg = json_parse(cfg_json ? cfg_json : "{}", NULL);
    json_t *e = cfg ? json_obj_get(cfg, "enabled") : NULL;
    bool en = e && json_is_bool(e) && json_bool_value(e);
    if (cfg) json_free(cfg);
    return en;
}

secret_source_t *secret_command_source_create(void)
{
    command_source_t *cs = calloc(1, sizeof(*cs));
    secret_source_t *s = &cs->base;
    s->api_version = SECRET_SOURCE_API_VERSION;
    s->name = "command";
    s->label = "Command Source";
    s->shape = "mapped";
    s->scheme = NULL;
    s->fetch = command_source_fetch;
    s->is_enabled = command_source_enabled;
    return s;
}

/* ── Global registry ──────────────────────────────────────────────────── */
static secret_registry_t *g_secret_registry = NULL;
secret_registry_t *secret_registry_init(void)
{
    if (g_secret_registry) return g_secret_registry;
    g_secret_registry = secret_registry_create();
    secret_source_t *cmd = secret_command_source_create();
    secret_registry_register(g_secret_registry, cmd);
    return g_secret_registry;
}
void secret_registry_shutdown(void)
{
    if (!g_secret_registry) return;
    /* Sources are static/long-lived; registry just drops its array. */
    secret_registry_free(g_secret_registry);
    g_secret_registry = NULL;
}
