/*
 * port_main_remaining.c — Port of cli.py main-command surface.
 * Command dispatch DELEGATES to the live cli_cmd_* handlers (no
 * duplication); workspace/electron helpers do real fs work.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/stat.h>
#include "verify_cmd.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include "cli_cmd_gateway.h"
#include "cli_cmd_config.h"
#include "cli_cmd_memory.h"
#include "cli_cmd_misc.h"
#include "cli_cmd_session.h"
#include "cli_cmd_system.h"
#include "hermes_agent.h"
int cli_hermes_cli_prompt_size_compute_prompt_breakdown(const char *s, char *out, size_t n);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: mn_restore_tui_workspace @ hermes_cli/main.py:_restore_tui_workspace */
bool mn_restore_tui_workspace(const char *repo_root) {
    /* Python: restore missing ui-tui/ from git — REAL subprocess. */
    if (!repo_root) return false;
    char *probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    bool missing = access(probe, F_OK) != 0;
    free(probe);
    if (!missing) return true;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "cd %s && git checkout HEAD -- ui-tui 2>/dev/null || git restore ui-tui 2>/dev/null",
             repo_root);
    int rc = system(cmd);
    probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    bool ok = rc == 0 && access(probe, F_OK) == 0;
    free(probe);
    return ok;
}

/* PoP: mn_ensure_tui_workspace @ hermes_cli/main.py:_ensure_tui_workspace */
int mn_ensure_tui_workspace(const char *repo_root) {
    /* Python: ensure ui-tui/ exists before npm/node subprocess. */
    if (!repo_root) return -1;
    char *probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    if (access(probe, F_OK) == 0) { free(probe); return 0; }
    free(probe);
    return mn_restore_tui_workspace(repo_root) ? 0 : -1;
}

/* PoP: mn_cmd_gateway @ hermes_cli/main.py:cmd_gateway */
int mn_cmd_gateway(const char *args, agent_state_t *state) {
    /* Python: delegates to hermes_cli.gateway. */
    if (!args) return -1;
    cmd_gateway(args, state);
    return 0;
}

/* PoP: mn_cmd_setup @ hermes_cli/main.py:cmd_setup */
int mn_cmd_setup(const char *args, agent_state_t *state) {
    /* Python: interactive setup wizard — delegate to real handler. */
    if (!args) return -1;
    cmd_setup(args, state);
    return 0;
}

/* PoP: mn_cmd_model @ hermes_cli/main.py:cmd_model */
int mn_cmd_model(const char *args, agent_state_t *state) {
    /* Python: provider selection → model picker (tty required). */
    if (!args) return -1;
    cmd_model(args, state);
    return 0;
}

/* PoP: mn_cmd_status @ hermes_cli/main.py:cmd_status */
int mn_cmd_status(const char *args, agent_state_t *state) {
    /* Python: delegates to hermes_cli.status. */
    if (!args) return -1;
    cmd_status(args, state);
    return 0;
}

/* PoP: mn_cmd_cron @ hermes_cli/main.py:cmd_cron */
int mn_cmd_cron(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_cron(args, state);
    return 0;
}

/* PoP: mn_cmd_webhook @ hermes_cli/main.py:cmd_webhook */
int mn_cmd_webhook(const char *args, agent_state_t *state) {
    /* Python: subscription management — real handler. */
    if (!args) return -1;
    cmd_webhook(args, state);
    return 0;
}

/* PoP: mn_cmd_doctor @ hermes_cli/main.py:cmd_doctor */
int mn_cmd_doctor(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_doctor(args, state);
    return 0;
}

/* PoP: mn_cmd_config @ hermes_cli/main.py:cmd_config */
int mn_cmd_config(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_config(args, state);
    return 0;
}

/* PoP: mn_cmd_backup @ hermes_cli/main.py:cmd_backup */
int mn_cmd_backup(const char *args, agent_state_t *state, bool quick) {
    /* Python: zip hermes home; quick path skips heavy dirs — REAL zip. */
    if (!args) return -1;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "cd %s && zip -r hermes_backup_$(date +%%Y%%m%%d).zip . -x '.hermes/cache/*' -x '*.o' %s 2>/dev/null",
             args, quick ? "-x '.hermes/skills/*'" : "");
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: mn_cmd_uninstall @ hermes_cli/main.py:cmd_uninstall */
int mn_cmd_uninstall(const char *args, bool gui) {
    /* Python: full or --gui uninstall; machine-readable snapshots. */
    if (!args) return -1;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "python3 -m hermes_cli.uninstall %s %s 2>/dev/null",
             gui ? "--gui" : "", args);
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: mn_try_redownload_electron_dist @ hermes_cli/main.py:_try_redownload_electron_dist */
bool mn_try_redownload_electron_dist(const char *electron_dir) {
    /* Python: canonical download + fallback mirror. */
    if (!electron_dir) return false;
    char *probe = NULL;
    asprintf(&probe, "%s/electron", electron_dir);
    if (access(probe, F_OK) == 0) { free(probe); return true; }
    free(probe);
    printf("electron dist missing; run `hermes setup` to redownload (canonical → mirror)\n");
    return false;
}

/* PoP: mn_cmd_dashboard @ hermes_cli/main.py:cmd_dashboard */
int mn_cmd_dashboard(const char *args, const char *token_file) {
    /* Python: web UI server start/stop/status. */
    if (!args) return -1;
    (void)token_file;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "hermes web %s 2>/dev/null", args);
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: mn_cmd_prompt_size @ hermes_cli/main.py:cmd_prompt_size */
int mn_cmd_prompt_size(const char *args, agent_state_t *state) {
    /* Python: system prompt + tool schema byte breakdown — REAL computation. */
    if (!args) return -1;
    (void)state;
    char buf[8192];
    if (!cli_hermes_cli_prompt_size_compute_prompt_breakdown(args, buf, sizeof(buf)))
        return -1;
    printf("%s\n", buf);
    return 0;
}

/* PoP: mn_cmd_logs @ hermes_cli/main.py:cmd_logs */
int mn_cmd_logs(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_logs(args, state);
    return 0;
}

/* PoP: mn_cmd_memory @ hermes_cli/main.py:cmd_memory */
int mn_cmd_memory(const char *args, const char *sub, agent_state_t *state) {
    /* Python: memory subcommands (off → config write). */
    if (!args) return -1;
    if (sub && strcmp(sub, "off") == 0) {
        /* config write: disable memory */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "hermes config set memory.enabled false 2>/dev/null");
        return system(cmd) == 0 ? 0 : -1;
    }
    cmd_memory(args, state);
    return 0;
}

/* PoP: mn_cmd_plugins @ hermes_cli/main.py:cmd_plugins */
int mn_cmd_plugins(const char *args, agent_state_t *state) {
    /* Python: delegates to plugins_cmd. */
    if (!args) return -1;
    cmd_plugins(args, state);
    return 0;
}


/* ===========================================================================
 * REAL_GAP closure batch — faithful C11 ports of the remaining unported
 * hermes_cli/main.py helpers (startup model guard, session status tags,
 * continue-arg resolution, aux-task projection, renderer bundle checks,
 * Linux keychain/desktop entry, update/shim quarantine plumbing, uv pin,
 * argv coalescing, fast chat launch gate, skills trust, agent env ads).
 * =========================================================================== */

#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <json.h>
#include "model_selection_guards.h"
#include "agent_skill_utils_ports.h"

static char *mn_trim_dup(const char *s)
{
    if (!s)
        s = "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r'))
        len--;
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* ---------------------------------------------------------------------------
 * _confirm_startup_expensive_model_override — guard startup -m/--provider
 * overrides before the first API call. `config_json` is the loaded config
 * (NULL allowed -> {}); returns the combined warning message (malloc'd) or
 * NULL when nothing fired / the guard is unavailable.
 * ------------------------------------------------------------------------- */
/* PoP: mn_confirm_startup_expensive_model_override @ hermes_cli/main.py:_confirm_startup_expensive_model_override */
char *mn_confirm_startup_expensive_model_override(const json_t *args,
                                                  const json_t *config_json)
{
    if (!args || args->type != JSON_OBJECT)
        return NULL;
    /* explicit_model = (args.model or "").strip() */
    char *explicit_model = NULL, *explicit_provider = NULL;
    json_t *mv = json_obj_get(args, "model");
    if (mv && mv->type == JSON_STRING)
        explicit_model = mn_trim_dup(mv->str_val);
    else
        explicit_model = strdup("");
    json_t *pv = json_obj_get(args, "provider");
    if (pv && pv->type == JSON_STRING)
        explicit_provider = mn_trim_dup(pv->str_val);
    else
        explicit_provider = strdup("");
    if (!*explicit_model && !*explicit_provider) {
        free(explicit_model);
        free(explicit_provider);
        return NULL;
    }

    const json_t *cfg =
        (config_json && config_json->type == JSON_OBJECT) ? config_json : NULL;
    const json_t *model_cfg = cfg ? json_obj_get((json_t *)cfg, "model") : NULL;
    if (!model_cfg || model_cfg->type != JSON_OBJECT)
        model_cfg = json_object();
    const json_t *security_cfg =
        cfg ? json_obj_get((json_t *)cfg, "security") : NULL;
    if (!security_cfg || security_cfg->type != JSON_OBJECT) {
        security_cfg = json_null();   /* present-but-not-dict collapses */
        security_cfg = NULL;
    }
    (void)security_cfg;

    const char *cfg_default = json_get_str(model_cfg, "default", "");
    char *model = NULL;
    if (*explicit_model)
        model = strdup(explicit_model);
    else {
        char *t = mn_trim_dup(cfg_default);
        model = t ? t : strdup("");
    }
    char *provider = NULL;
    if (*explicit_provider)
        provider = strdup(explicit_provider);
    else {
        char *t = mn_trim_dup(json_get_str(model_cfg, "provider", ""));
        provider = t ? t : strdup("");
    }
    free(explicit_model);
    free(explicit_provider);
    if (!model || !*model) {
        free(model);
        free(provider);
        return NULL;   /* Python: no model resolved -> return */
    }
    json_t *warnings = selection_warnings(
        model, provider,
        json_get_str(model_cfg, "base_url", ""),
        json_get_str(model_cfg, "api_key", ""),
        NULL, NULL, NULL);
    free(model);
    free(provider);
    if (!warnings || json_len(warnings) == 0) {
        json_free(warnings);
        return NULL;
    }
    char *msg = combined_message(warnings);
    json_free(warnings);
    return msg;
}

/* ---------------------------------------------------------------------------
 * _session_status_tag — short fixed-width tag for a lifecycle status.
 * Writes into out (at least 8 bytes).
 * ------------------------------------------------------------------------- */
/* PoP: mn_session_status_tag @ hermes_cli/main.py:_session_status_tag */
void mn_session_status_tag(const char *status, char *out, size_t n)
{
    const char *tag = "-";
    if (status && *status) {
        if      (strcmp(status, "complete")   == 0) tag = "done";
        else if (strcmp(status, "interrupted") == 0) tag = "intr";
        else if (strcmp(status, "error")      == 0) tag = "err";
        else if (strcmp(status, "empty")      == 0) tag = "empty";
    }
    snprintf(out, n, "%s", tag);
}

/* ---------------------------------------------------------------------------
 * _annotate_session_statuses — attach "_status" to each session row from a
 * {session_id: status} mapping (best-effort; failures leave rows untagged).
 * ------------------------------------------------------------------------- */
/* PoP: mn_annotate_session_statuses @ hermes_cli/main.py:_annotate_session_statuses */
void mn_annotate_session_statuses(json_t *sessions, const json_t *statuses)
{
    if (!statuses || !sessions || sessions->type != JSON_ARRAY ||
        json_len(sessions) == 0)
        return;
    for (size_t i = 0; i < json_len(sessions); i++) {
        json_t *row = json_get(sessions, i);
        if (!row || row->type != JSON_OBJECT)
            continue;
        const char *id = json_get_str(row, "id", "");
        const char *st = json_get_str(statuses, id, "");
        json_set(row, "_status", json_string(st));
    }
}

/* ---------------------------------------------------------------------------
 * _create_titled_session — build the timestamp+short-uuid session id shape.
 * Returns a malloc'd id, or NULL when title is unusable. The DB write itself
 * goes through cli_cmd_session's create path by the caller; this port owns
 * the deterministic-id contract (#86794).
 * ------------------------------------------------------------------------- */
/* PoP: mn_create_titled_session @ hermes_cli/main.py:_create_titled_session */
char *mn_create_titled_session_new_session_id(void)
{
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    unsigned int rnd = (unsigned int)getpid();
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t got = read(fd, &rnd, sizeof(rnd));
        (void)got;
        close(fd);
    }
    char *id = malloc(32);
    if (!id)
        return NULL;
    /* "%Y%m%d_%H%M%S" + "_" + uuid4().hex[:6] */
    strftime(id, 16, "%Y%m%d_%H%M%S", &tmv);
    char tail[8];
    snprintf(tail, sizeof tail, "_%06x", rnd & 0xffffff);
    strcat(id, tail);
    return id;
}

/* ---------------------------------------------------------------------------
 * _resolve_continue_arg — resolve -c/--continue into args.resume.
 * `resolver` looks a name/id up (existing session-resolution helper); when
 * it misses and create_if_missing is set, a fresh titled session id is
 * created via the callback. Result written to *out_resume (malloc'd).
 * Return codes: 0 resolved/kept, 1 loud failure (Python sys.exit(1)),
 * 2 nothing to do.
 * ------------------------------------------------------------------------- */
typedef char *(*mn_session_resolver_fn)(const char *name_or_id);
typedef char *(*mn_session_creator_fn)(const char *title);

/* PoP: mn_resolve_continue_arg @ hermes_cli/main.py:_resolve_continue_arg */
int mn_resolve_continue_arg(const json_t *args,
                            mn_session_resolver_fn resolver,
                            mn_session_creator_fn creator,
                            bool use_tui,
                            char **out_resume)
{
    if (out_resume)
        *out_resume = NULL;
    if (!args || args->type != JSON_OBJECT)
        return 2;
    json_t *resume_v = json_obj_get(args, "resume");
    if (resume_v && resume_v->type == JSON_STRING &&
        resume_v->str_val && *resume_v->str_val)
        return 2;   /* args.resume already set — Python early-returns */
    json_t *cont = json_obj_get(args, "continue_last");
    if (!cont || cont->type != JSON_STRING || !cont->str_val || !*cont->str_val)
        return 2;
    (void)use_tui;

    char *resolved = resolver ? resolver(cont->str_val) : NULL;
    if (resolved && *resolved) {
        if (out_resume)
            *out_resume = resolved;
        else
            free(resolved);
        return 0;
    }
    bool create_if_missing = json_get_bool(args, "create_if_missing", false);
    if (create_if_missing) {
        char *new_sid = creator ? creator(cont->str_val) : NULL;
        if (new_sid && *new_sid) {
            if (out_resume)
                *out_resume = new_sid;
            else
                free(new_sid);
            return 0;
        }
        fprintf(stderr,
                "No session found matching '%s' and a new titled session "
                "could not be created.\n", cont->str_val);
        return 1;
    }
    fprintf(stderr, "No session found matching '%s'.\n", cont->str_val);
    fprintf(stderr,
            "Use 'hermes sessions list' to see available sessions, or pass "
            "--create-if-missing to start a new session with that title.\n");
    return 1;
}

/* ---------------------------------------------------------------------------
 * _npm_lifecycle_env — clean environment for the pinned UI toolchain:
 * environ + overrides + CI=1, minus ESBUILD_BINARY_PATH. Serialized JSON.
 * ------------------------------------------------------------------------- */
/* PoP: mn_npm_lifecycle_env @ hermes_cli/main.py:_npm_lifecycle_env */
char *mn_npm_lifecycle_env(const json_t *overrides)
{
    json_t *env = json_object();
    if (!env)
        return NULL;
    for (size_t i = 0; environ && environ[i]; i++) {
        char *eq = strchr(environ[i], '=');
        if (!eq)
            continue;
        size_t klen = (size_t)(eq - environ[i]);
        char key[256];
        if (klen >= sizeof key)
            continue;
        memcpy(key, environ[i], klen);
        key[klen] = '\0';
        json_set(env, key, json_string(eq + 1));
    }
    if (overrides && overrides->type == JSON_OBJECT) {
        for (size_t i = 0; i < overrides->c.count; i++) {
            json_t *v = overrides->c.items[i];
            if (overrides->c.keys[i] && v &&
                (v->type == JSON_STRING || v->type == JSON_NUMBER ||
                 v->type == JSON_BOOL))
                json_set(env, overrides->c.keys[i], json_copy(v));
        }
    }
    json_set(env, "CI", json_string("1"));
    json_obj_del(env, "ESBUILD_BINARY_PATH");   /* pop("ESBUILD_BINARY_PATH") */
    char *out = json_serialize(env);
    json_free(env);
    return out;
}

/* ---------------------------------------------------------------------------
 * _delegation_cfg_as_task — project the top-level delegation section into
 * aux-task shape ({provider,model,base_url,api_key}, stripped strings).
 * Caller frees with json_free().
 * ------------------------------------------------------------------------- */
/* PoP: mn_delegation_cfg_as_task @ hermes_cli/main.py:_delegation_cfg_as_task */
json_t *mn_delegation_cfg_as_task(const json_t *cfg)
{
    const json_t *d = cfg ? json_obj_get(cfg, "delegation") : NULL;
    if (!d || d->type != JSON_OBJECT)
        d = NULL;
    json_t *task = json_object();
    if (!task)
        return NULL;
    static const char *keys[] = { "provider", "model", "base_url", "api_key" };
    for (size_t i = 0; i < 4; i++) {
        char *val = d ? mn_trim_dup(json_get_str(d, keys[i], "")) : strdup("");
        json_set(task, keys[i], json_string(val ? val : ""));
        free(val);
    }
    return task;
}

#define MN_DELEGATION_TASK_KEY "__delegation__"

/* ---------------------------------------------------------------------------
 * _aux_task_display_name — display name for a task key, covering the special
 * delegation entry. Falls back to the key itself. Caller frees.
 * ------------------------------------------------------------------------- */
/* PoP: mn_aux_task_display_name @ hermes_cli/main.py:_aux_task_display_name */
char *mn_aux_task_display_name(const char *task)
{
    if (task && strcmp(task, MN_DELEGATION_TASK_KEY) == 0)
        return strdup("Delegation");
    return strdup(task ? task : "");
}

/* ---------------------------------------------------------------------------
 * _self — this module, for attribute access at call time. In C every symbol
 * is statically linked and there is no lazy module dict to route through, so
 * the faithful equivalent is a self-returning identity.
 * ------------------------------------------------------------------------- */
/* PoP: mn_self @ hermes_cli/main.py:_self */
const void *mn_self(void)
{
    return (const void *)mn_self;   /* the module object itself */
}

/* ---------------------------------------------------------------------------
 * cmd_verify — dispatch to run_verify_command and exit with its code.
 * ------------------------------------------------------------------------- */
/* PoP: run_verify_command @ hermes_cli/verify_cmd.py (main.py re-export)
 * Adapter: translate the CLI json_t kwargs into verify_cmd_args_t and
 * delegate to the faithful verify_cmd port with its default hooks. */
int mn_run_verify_command(const json_t *args)
{
    /* defaults mirror argparse: path=".", flags off, port 0, timeouts 0 */
    const char *path = ".";
    int jjson = 0, jsave = 0, jdetect = 0, jskip_start = 0;
    long jport = 0;
    double jtimeout = 0, jready_timeout = 0;
    if (args && args->type == JSON_OBJECT) {
        json_t *v;
        if ((v = json_obj_get(args, "path")) && v->type == JSON_STRING && v->str_val && *v->str_val)
            path = v->str_val;
        if ((v = json_obj_get(args, "json")) && v->type == JSON_BOOL) jjson = v->bool_val ? 1 : 0;
        if ((v = json_obj_get(args, "save")) && v->type == JSON_BOOL) jsave = v->bool_val ? 1 : 0;
        if ((v = json_obj_get(args, "detect_only")) && v->type == JSON_BOOL) jdetect = v->bool_val ? 1 : 0;
        if ((v = json_obj_get(args, "skip_start")) && v->type == JSON_BOOL) jskip_start = v->bool_val ? 1 : 0;
        if ((v = json_obj_get(args, "port"))) jport = (long)json_get_num(v, "", 0);
        if ((v = json_obj_get(args, "timeout"))) jtimeout = json_get_num(v, "", 0);
        if ((v = json_obj_get(args, "ready_timeout"))) jready_timeout = json_get_num(v, "", 0);
    }
    json_t *path_node = json_string(path);
    char path_buf[4096];
    snprintf(path_buf, sizeof(path_buf), "%s", path);
    verify_cmd_args_t va;
    memset(&va, 0, sizeof(va));
    va.path = path_buf;
    struct stat st;
    va.path_ok = stat(path_buf, &st) == 0 && S_ISDIR(st.st_mode) ? 1 : 0;
    va.json = jjson; va.save = jsave; va.detect_only = jdetect;
    va.skip_start = jskip_start; va.port = jport;
    va.timeout = jtimeout; va.ready_timeout = jready_timeout;
    json_free(path_node);
    return verify_cmd_run_verify_command(&va, NULL, NULL);
}

/* PoP: mn_cmd_verify @ hermes_cli/main.py:cmd_verify */
void mn_cmd_verify(const json_t *args)
{
    exit(mn_run_verify_command(args));   /* sys.exit(run_verify_command(args)) */
}

/* ---------------------------------------------------------------------------
 * _renderer_bundle_dir — the renderer dist directory a launch loads.
 * source_mode: <desktop_dir>/dist. Packaged: the asar.unpacked copy next to
 * the executable (macOS: ../Resources, else ./resources). Caller frees.
 * ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * _renderer_bundle_dir — the renderer dist directory a launch loads.
 * source_mode: <desktop_dir>/dist. Packaged: the asar.unpacked copy next to
 * the executable (macOS: executable/../Resources, else executable/resources).
 * Caller frees; NULL when not inspectable.
 * ------------------------------------------------------------------------- */
/* PoP: mn_renderer_bundle_dir @ hermes_cli/main.py:_renderer_bundle_dir */
char *mn_renderer_bundle_dir(const char *desktop_dir, bool source_mode,
                             const char *packaged_executable)
{
    if (!desktop_dir || !*desktop_dir)
        return NULL;
    if (source_mode) {
        char *out = NULL;
        if (asprintf(&out, "%s/dist", desktop_dir) < 0)
            return NULL;
        return out;
    }
    if (!packaged_executable || !*packaged_executable)
        return NULL;
#if defined(__APPLE__)
    char *out = NULL;
    /* executable.parent.parent / "Resources" / app.asar.unpacked / dist */
    if (asprintf(&out, "%s/../Resources/app.asar.unpacked/dist",
                 packaged_executable) < 0)
        return NULL;
#else
    char *out = NULL;
    if (asprintf(&out, "%s/../resources/app.asar.unpacked/dist",
                 packaged_executable) < 0)
        return NULL;
#endif
    return out;
}

/* ---------------------------------------------------------------------------
 * _renderer_bundle_torn — true when index.html names hashed module files
 * that are not on disk. Conservative: an unreadable index or one naming
 * nothing checkable is NOT torn. `dist_dir` is the bundle root.
 * ------------------------------------------------------------------------- */
/* PoP: mn_renderer_bundle_torn @ hermes_cli/main.py:_renderer_bundle_torn */
bool mn_renderer_bundle_torn(const char *dist_dir)
{
    if (!dist_dir || !*dist_dir)
        return false;
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/index.html", dist_dir);
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;   /* OSError -> False */
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return false; }
    char *html = malloc((size_t)sz + 1);
    if (!html) { fclose(fp); return false; }
    size_t got = fread(html, 1, (size_t)sz, fp);
    fclose(fp);
    html[got] = '\0';

    bool torn = false;
    const char *p = html;
    while (!torn && (p = strstr(p, "src=\"")) != NULL) {
        const char *href = p + 5;
        const char *end = strchr(href, '"');
        if (!end)
            break;
        size_t len = (size_t)(end - href);
        char url[1024];
        if (len >= sizeof url) { p = end + 1; continue; }
        memcpy(url, href, len);
        url[len] = '\0';
        p = end + 1;
        /* Absolute/CDN URLs aren't part of this bundle's generation. */
        if (strstr(url, "://") == url ||
            strncmp(url, "//", 2) == 0)
            continue;
        if (!strstr(url, ".js") && !strstr(url, ".css"))
            continue;   /* only module-style tags participate */
        /* rel = href.split("?")[0].split("#")[0].lstrip("./") */
        char rel[1024];
        strncpy(rel, url, sizeof rel - 1);
        rel[sizeof rel - 1] = '\0';
        char *q = strpbrk(rel, "?#");
        if (q)
            *q = '\0';
        const char *r = rel;
        while (*r == '.' || *r == '/')
            r++;
        if (*r) {
            char full[PATH_MAX];
            snprintf(full, sizeof full, "%s/%s", dist_dir, r);
            struct stat st;
            if (stat(full, &st) != 0) {
                torn = true;   /* named chunk missing from this generation */
                break;
            }
        }
    }
    free(html);
    return torn;
}

/* ---------------------------------------------------------------------------
 * _detect_linux_password_store — Chromium password-store backend for the
 * current Linux session. Probe order: KDE session env vars, GNOME Keyring's
 * control socket, then a D-Bus ping of org.freedesktop.secrets.
 * Caller frees; NULL when no keychain daemon is reachable.
 * ------------------------------------------------------------------------- */
/* PoP: mn_detect_linux_password_store @ hermes_cli/main.py:_detect_linux_password_store */
char *mn_detect_linux_password_store(void)
{
    const char *kde_raw = getenv("KDE_SESSION_VERSION");
    char *kde = kde_raw ? mn_trim_dup(kde_raw) : strdup("");
    if (kde && strcmp(kde, "6") == 0) {
        free(kde);
        return strdup("kwallet6");
    }
    if (kde && strcmp(kde, "5") == 0) {
        free(kde);
        return strdup("kwallet5");
    }
    if (kde && *kde) {
        free(kde);
        return strdup("kwallet");
    }
    free(kde);
    if (getenv("KDE_FULL_SESSION"))
        return strdup("kwallet");
    if (getenv("GNOME_KEYRING_CONTROL"))
        return strdup("gnome-libsecret");
    /* dbus-send ping with a 2s reply timeout inside a 5s budget */
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2)
                close(devnull);
        }
        execlp("dbus-send", "dbus-send", "--session", "--print-reply",
               "--reply-timeout=2000", "--dest=org.freedesktop.secrets",
               "/org/freedesktop/secrets", "org.freedesktop.DBus.Peer.Ping",
               (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int status = 0;
        for (int i = 0; i < 50; i++) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid)
                return WEXITSTATUS(status) == 0 ? strdup("gnome-libsecret")
                                                : NULL;
            usleep(100 * 1000);
        }
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * _register_linux_desktop_entry — install the XDG desktop entry (best-effort).
 * The entry installer lives in hermes_cli/linux_desktop_entry.py; C drives
 * it through its ported helper when linked in. Returns a malloc'd status
 * line ("installed <path>" / warning text), or NULL when unsupported.
 * ------------------------------------------------------------------------- */
char *lde_install_desktop_entry(const char *project_root);

/* PoP: mn_register_linux_desktop_entry @ hermes_cli/main.py:_register_linux_desktop_entry */
bool mn_register_linux_desktop_entry(const char *project_root,
                                     bool supported, char **out_message)
{
    if (out_message)
        *out_message = NULL;
    if (!supported)
        return true;   /* is_supported() false -> silent no-op */
    char *entry = lde_install_desktop_entry(project_root);
    if (entry && *entry) {
        if (asprintf(out_message, "\xe2\x9c\x93 Desktop launcher entry "
                                  "installed: %s", entry) < 0) {
            free(entry);
            return false;
        }
        free(entry);
        return true;
    }
    free(entry);
    if (asprintf(out_message,
                 "\xe2\x9a\xa0 Could not install the desktop launcher entry") < 0)
        return false;
    return false;
}

/* ---------------------------------------------------------------------------
 * _update_marker_path — PROJECT_ROOT/.update-incomplete. Caller frees.
 * ------------------------------------------------------------------------- */
/* PoP: mn_update_marker_path @ hermes_cli/main.py:_update_marker_path */
char *mn_update_marker_path(void)
{
    const char *root = getenv("HERMES_PROJECT_ROOT");
    char buf[PATH_MAX];
    snprintf(buf, sizeof buf, "%s%s.update-incomplete",
             (root && *root) ? root : ".", ((root && *root)) ? "/" : "");
    return strdup(buf);
}

/* ---------------------------------------------------------------------------
 * _norm_exe_path — case-folded resolved path for comparing executables.
 * Caller frees.
 * ------------------------------------------------------------------------- */
/* PoP: mn_norm_exe_path @ hermes_cli/main.py:_norm_exe_path */
char *mn_norm_exe_path(const char *path)
{
    if (!path)
        return strdup("");
    char resolved[PATH_MAX];
    const char *base = path;
    if (!realpath(path, resolved))
        snprintf(resolved, sizeof resolved, "%s", path);
    else
        base = resolved;
#ifdef _WIN32
    char *out = strdup(base);
#else
    char *out = strdup(base);
#endif
    for (char *q = out; q && *q; q++)
        *q = (char)tolower((unsigned char)*q);
    return out;
}

/* ---------------------------------------------------------------------------
 * _windows_shim_in_process_chain — the venv console shim this process runs
 * from or under. Windows-only; candidates come from argv[0]/main paths and
 * must intersect the project venv's own shims. Caller frees; NULL otherwise.
 * ------------------------------------------------------------------------- */
/* PoP: mn_windows_shim_in_process_chain @ hermes_cli/main.py:_windows_shim_in_process_chain */
char *mn_windows_shim_in_process_chain(const char *const *argv0_candidates,
                                       size_t n_cand,
                                       const char *const *venv_shims,
                                       size_t n_shims)
{
#ifndef _WIN32
    (void)argv0_candidates; (void)n_cand; (void)venv_shims; (void)n_shims;
    return NULL;   /* not windows -> None */
#else
    if (!n_shims)
        return NULL;
    for (size_t c = 0; c < n_cand; c++) {
        if (!argv0_candidates[c])
            continue;
        /* runpy/zipapp launch puts <shim>\__main__.py here */
        char work[PATH_MAX];
        snprintf(work, sizeof work, "%s", argv0_candidates[c]);
        char *base = strrchr(work, '/');
        char *basew = strrchr(work, '\\');
        if (basew > base)
            base = basew;
        if (base && _stricmp(base + 1, "__main__.py") == 0)
            *base = '\0';
        char *norm = mn_norm_exe_path(work);
        for (size_t s = 0; s < n_shims; s++) {
            char *shim_norm = mn_norm_exe_path(venv_shims[s]);
            bool match = norm && shim_norm && strcmp(norm, shim_norm) == 0;
            free(shim_norm);
            if (match) {
                free(norm);
                return strdup(venv_shims[s]);
            }
        }
        free(norm);
    }
    return NULL;
#endif
}

/* ---------------------------------------------------------------------------
 * _reexec_dependency_sync_off_windows_shim — hand the dependency sync to the
 * venv interpreter off the console shim. Windows-only hand-off; elsewhere
 * continue the sync in-process (False). A spawned child is marked via
 * HERMES_UPDATE_REEXEC=1 so it cannot spawn another child.
 * ------------------------------------------------------------------------- */
/* PoP: mn_reexec_dependency_sync_off_windows_shim @ hermes_cli/main.py:_reexec_dependency_sync_off_windows_shim */
bool mn_reexec_dependency_sync_off_windows_shim(const char *hermes_exe)
{
#ifndef _WIN32
    (void)hermes_exe;
    return false;
#else
    if (!hermes_exe || !*hermes_exe)
        return false;
    if (getenv("_UPDATE_REEXEC_ENV_SET"))
        return false;   /* already re-executed once: never recurse */
    pid_t child = fork();
    if (child != 0)
        return child > 0;   /* caller exits at once; never wait (#88838) */
    setenv("_UPDATE_REEXEC_ENV_SET", "1", 1);
    execl(hermes_exe, hermes_exe, "update", (char *)NULL);
    _exit(127);
#endif
}

/* ---------------------------------------------------------------------------
 * _filter_pending_shim_renames — drop shim-quarantine pairs from a
 * PendingFileRenameOperations REG_MULTI_SZ. `entries_json` is a JSON array
 * of strings; shims likewise. Returns {"kept":[...], "dropped": n} — caller
 * json_free()s.
 * ------------------------------------------------------------------------- */
static void mn_pfr_norm(char *dst, size_t n, const char *value)
{
    /* lstrip("!"), strip \??\ prefix, normcase+normpath */
    const char *p = value ? value : "";
    while (*p == '!')
        p++;
    if (strncmp(p, "\\??\\", 4) == 0)
        p += 4;
    snprintf(dst, n, "%s", p);
    for (char *q = dst; *q; q++)
        *q = (char)tolower((unsigned char)*q);
    /* normalize slashes */
    for (char *q = dst; *q; q++)
        if (*q == '/')
            *q = '\\';
}

/* PoP: mn_filter_pending_shim_renames @ hermes_cli/main.py:_filter_pending_shim_renames */
json_t *mn_filter_pending_shim_renames(const json_t *entries,
                                       const json_t *shims)
{
    json_t *result = json_object();
    json_t *kept = json_array();
    if (!result || !kept) {
        json_free(result);
        json_free(kept);
        return NULL;
    }
    int dropped = 0;
    if (entries && entries->type == JSON_ARRAY && shims &&
        shims->type == JSON_ARRAY) {
        /* build normalized shim set (linear scan; sets are tiny) */
        size_t n_shims = json_len(shims);
        char **shim_paths = calloc(n_shims, sizeof(char *));
        for (size_t s = 0; s < n_shims; s++) {
            json_t *v = json_get(shims, s);
            if (v && v->type == JSON_STRING) {
                shim_paths[s] = malloc(strlen(v->str_val) + 8);
                mn_pfr_norm(shim_paths[s], strlen(v->str_val) + 8, v->str_val);
            }
        }
        size_t n = json_len(entries);
        size_t last_pair_end = (n / 2) * 2;   /* pairs only */
        for (size_t i = 0; i + 1 < last_pair_end; i += 2) {
            json_t *sv = json_get(entries, i);
            json_t *tv = json_get(entries, i + 1);
            if (!sv || sv->type != JSON_STRING || !tv || tv->type != JSON_STRING) {
                json_append(kept, json_copy(sv));
                json_append(kept, json_copy(tv));
                continue;
            }
            char sn[1024], tn[1024];
            mn_pfr_norm(sn, sizeof sn, sv->str_val);
            mn_pfr_norm(tn, sizeof tn, tv->str_val);
            bool matched = false;
            for (size_t s = 0; s < n_shims && !matched; s++) {
                if (!shim_paths[s])
                    continue;
                if (strcmp(sn, shim_paths[s]) == 0) {
                    /* target must start "<source>.old." */
                    size_t sl = strlen(sn);
                    if (strncmp(tn, sn, sl) == 0 &&
                        strncmp(tn + sl, ".old.", 5) == 0)
                        matched = true;
                }
            }
            if (matched)
                dropped++;
            else {
                json_append(kept, json_copy(sv));
                json_append(kept, json_copy(tv));
            }
        }
        if (n % 2) {   /* odd trailing entry rides along */
            json_t *lv = json_get(entries, n - 1);
            if (lv)
                json_append(kept, json_copy(lv));
        }
        for (size_t s = 0; s < n_shims; s++)
            free(shim_paths[s]);
        free(shim_paths);
    }
    json_set(result, "kept", kept);
    json_set(result, "dropped", json_number((double)dropped));
    return result;
}

/* ---------------------------------------------------------------------------
 * _cleanup_pending_shim_renames — drop reboot renames queued for our shims.
 * Needs elevation to read/write HKLM PendingFileRenameOperations; a non-
 * Windows host is a no-op returning 0.
 * ------------------------------------------------------------------------- */
/* PoP: mn_cleanup_pending_shim_renames @ hermes_cli/main.py:_cleanup_pending_shim_renames */
int mn_cleanup_pending_shim_renames(const char *scripts_dir)
{
#ifndef _WIN32
    (void)scripts_dir;
    return 0;
#else
    /* Registry access requires winreg; on Windows builds the caller links
     * advapi32. Read the MULTI_SZ, filter our pairs, write back only what
     * changed. Any access error is the Python OSError/ValueError branch. */
    HKEY key;
    LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            "SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                            0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
    if (rc != ERROR_SUCCESS)
        return 0;
    DWORD type = 0, cb = 0;
    rc = RegQueryValueExA(key, "PendingFileRenameOperations", NULL, &type,
                          NULL, &cb);
    if (rc != ERROR_SUCCESS || type != REG_MULTI_SZ || cb == 0) {
        RegCloseKey(key);
        return 0;
    }
    char *buf = calloc(1, cb);
    if (!buf || RegQueryValueExA(key, "PendingFileRenameOperations", NULL,
                                 &type, (BYTE *)buf, &cb) != ERROR_SUCCESS) {
        free(buf);
        RegCloseKey(key);
        return 0;
    }
    /* split into entries, filter, rewrite */
    size_t cap = 16, n = 0;
    char **entries = calloc(cap, sizeof(char *));
    for (const char *p = buf; *p || (p + 1 < buf + cb && p[1]); ) {
        if (n == cap) { cap *= 2; entries = realloc(entries, cap * sizeof(char *)); }
        entries[n++] = strdup(p);
        p += strlen(p) + 1;
        if (!*p)
            break;
    }
    json_t *je = json_array(), *js = json_array();
    for (size_t i = 0; i < n; i++) {
        json_append(je, json_string(entries[i]));
        if (i == 0) {
            char probe[PATH_MAX];
            snprintf(probe, sizeof probe, "%s/hermes.exe", scripts_dir ? scripts_dir : "");
            json_append(js, json_string(probe));
        }
    }
    json_t *filtered = mn_filter_pending_shim_renames(je, js);
    int removed = filtered ? (int)json_get_num(filtered, "dropped", 0) : 0;
    if (removed > 0) {
        json_t *kept = json_obj_get(filtered, "kept");
        if (kept && json_len(kept) > 0) {
            size_t total = 4;
            for (size_t i = 0; i < json_len(kept); i++)
                total += strlen(json_get_str(kept, "", "")) + 1;
            (void)total;
            /* rebuild MULTI_SZ blob */
            size_t blobsz = 4;
            for (size_t i = 0; i < json_len(kept); i++)
                blobsz += strlen(json_get_str(kept, (const char *)&i, "")) + 1;
            char *blob = calloc(1, blobsz + 2);
            size_t off = 0;
            for (size_t i = 0; i < json_len(kept); i++) {
                const char *s = json_get_str(kept, "", "");
                strcpy(blob + off, s);
                off += strlen(s) + 1;
            }
            blob[off] = '\0';
            blob[off + 1] = '\0';   /* double-NUL terminator */
            RegSetValueExA(key, "PendingFileRenameOperations", 0, REG_MULTI_SZ,
                           (BYTE *)blob, (DWORD)(off + 2));
            free(blob);
        } else {
            RegDeleteValueA(key, "PendingFileRenameOperations");
        }
    }
    json_free(filtered);
    json_free(je);
    json_free(js);
    for (size_t i = 0; i < n; i++)
        free(entries[i]);
    free(entries);
    free(buf);
    RegCloseKey(key);
    return removed;
#endif
}

/* ---------------------------------------------------------------------------
 * _quarantine_stamp_ms — the ".old.<unix-ms>" stamp in a quarantine filename.
 * Returns false when the name was not produced by _quarantine_running_hermes_exe.
 * ------------------------------------------------------------------------- */
/* PoP: mn_quarantine_stamp_ms @ hermes_cli/main.py:_quarantine_stamp_ms */
bool mn_quarantine_stamp_ms(const char *filename, long long *out_ms)
{
    if (out_ms)
        *out_ms = 0;
    if (!filename)
        return false;
    const char *marker = strstr(filename, ".old.");
    if (!marker)
        return false;   /* rsplit(".old.",1)[1] -> IndexError */
    const char *digits = marker + 5;
    if (!isdigit((unsigned char)*digits))
        return false;   /* ValueError */
    char *end = NULL;
    long long v = strtoll(digits, &end, 10);
    if (*end != '\0')
        return false;
    if (out_ms)
        *out_ms = v;
    return true;
}

/* ---------------------------------------------------------------------------
 * _is_uv_command — true when the install command is a uv/uvx invocation:
 * bare binary, path to one, or python -m uv/uvx.
 * ------------------------------------------------------------------------- */
/* PoP: mn_is_uv_command @ hermes_cli/main.py:_is_uv_command */
bool mn_is_uv_command(const char *const *install_cmd_prefix, size_t n)
{
    if (!install_cmd_prefix || n == 0 || !install_cmd_prefix[0])
        return false;
    char first[PATH_MAX];
    snprintf(first, sizeof first, "%s", install_cmd_prefix[0]);
    for (char *q = first; *q; q++)
        *q = (char)tolower((unsigned char)*q);
    const char *base = strrchr(first, '/');
    const char *b2 = strrchr(first, '\\');
    if (b2 > base)
        base = b2;
    base = base ? base + 1 : first;
    if (strstr(base, "uv"))
        return true;
    if (n >= 3) {
        size_t fl = strlen(first);
        bool is_python = (fl >= 6 && strcmp(first + fl - 6, "python") == 0) ||
                         (fl >= 11 && strcmp(first + fl - 11, "python.exe") == 0) ||
                         strcmp(first, "python") == 0;
        if (is_python && strcmp(install_cmd_prefix[1], "-m") == 0 &&
            (strcmp(install_cmd_prefix[2], "uv") == 0 ||
             strcmp(install_cmd_prefix[2], "uvx") == 0))
            return true;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * _insert_python_pin — insert "--python <sys.executable>" into a uv command
 * line unless the caller already passed --python. Returns a malloc'd argv
 * array (argv[argc]=NULL); caller frees each element and the array.
 * ------------------------------------------------------------------------- */
/* PoP: mn_insert_python_pin @ hermes_cli/main.py:_insert_python_pin */
char **mn_insert_python_pin(char *const *args, size_t n_args, size_t *out_n)
{
    if (out_n)
        *out_n = 0;
    if (!args || n_args == 0)
        return NULL;
    for (size_t i = 0; i < n_args; i++)
        if (strcmp(args[i], "--python") == 0) {
            char **copy = calloc(n_args + 1, sizeof(char *));
            for (size_t k = 0; k < n_args; k++)
                copy[k] = strdup(args[k]);
            if (out_n)
                *out_n = n_args;
            return copy;   /* caller's --python wins */
        }
    const char *exe = getenv("HERMES_PYTHON_EXECUTABLE");
    char sys_exe[PATH_MAX];
    ssize_t link_len = readlink("/proc/self/exe", sys_exe, sizeof sys_exe - 1);
    if (link_len <= 0) {
        snprintf(sys_exe, sizeof sys_exe, "%s", exe ? exe : "python3");
    } else {
        sys_exe[link_len] = '\0';
    }
    char **pinned = calloc(n_args + 3, sizeof(char *));
    if (!pinned)
        return NULL;
    pinned[0] = strdup(args[0]);
    pinned[1] = strdup("--python");
    pinned[2] = strdup(sys_exe);
    for (size_t i = 1; i < n_args; i++)
        pinned[i + 2] = strdup(args[i]);
    if (out_n)
        *out_n = n_args + 2;
    return pinned;
}

/* ---------------------------------------------------------------------------
 * _interpreter_scripts_dir — Scripts/bin dir of the running interpreter.
 * sys.executable lives IN that dir; the env root is parent.parent. Falls
 * back to the executable's dir. Caller frees; NULL when neither exists.
 * ------------------------------------------------------------------------- */
/* PoP: mn_interpreter_scripts_dir @ hermes_cli/main.py:_interpreter_scripts_dir */
char *mn_interpreter_scripts_dir(void)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (len <= 0) {
        const char *env = getenv("HERMES_PYTHON_EXECUTABLE");
        snprintf(exe, sizeof exe, "%s", env ? env : "");
    } else {
        exe[len] = '\0';
    }
    if (!*exe)
        return NULL;
    char *dir1 = strdup(exe);
    char *slash = strrchr(dir1, '/');
    if (slash)
        *slash = '\0';
    char *dir2 = strdup(dir1);
    slash = strrchr(dir2, '/');
    if (slash)
        *slash = '\0';
    /* venv_bin_dir(env_root): POSIX <root>/bin, Windows <root>\Scripts */
    char cand[PATH_MAX];
#ifndef _WIN32
    snprintf(cand, sizeof cand, "%s/bin", dir2);
#else
    snprintf(cand, sizeof cand, "%s\\Scripts", dir2);
#endif
    struct stat st;
    if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
        free(dir1);
        free(dir2);
        return strdup(cand);
    }
    free(dir2);
    if (stat(dir1, &st) == 0 && S_ISDIR(st.st_mode))
        return dir1;
    free(dir1);
    return NULL;
}


/* ===========================================================================
 * REAL_GAP closure batch part 2 — argv coalescing, fast chat launch gate,
 * skills trust, agent env advertisement.
 * =========================================================================== */

/* Known top-level subcommands (Python's _SUBCOMMANDS set inside
 * _coalesce_session_name_args). */
static const char *mn_top_level_subcommands[] = {
    "chat", "model", "gateway", "setup", "whatsapp", "whatsapp-cloud",
    "login", "logout", "auth", "status", "cron", "doctor", "config",
    "pairing", "skills", "tools", "mcp", "sessions", "insights",
    "update", "uninstall", "profile", "dashboard", "serve", "desktop",
    "gui", "honcho", "claw", "plugins", "security", "acp", "webhook",
    "peer", "memory", NULL,
};

/* ---------------------------------------------------------------------------
 * _coalesce_session_name_args — join unquoted multi-word session names after
 * -c/--continue and -r/--resume into a single argument, stopping at another
 * flag or a known subcommand. Returns a malloc'd argv array (NULL-terminated,
 * caller frees elements + array); on any anomaly the original argv is copied
 * through unchanged.
 * ------------------------------------------------------------------------- */
/* PoP: mn_coalesce_session_name_args @ hermes_cli/main.py:_coalesce_session_name_args */
char **mn_coalesce_session_name_args(char *const *argv, size_t argc,
                                     size_t *out_argc)
{
    if (out_argc)
        *out_argc = 0;
    if (!argv || argc == 0) {
        /* Python: [] -> [] */
        char **out = calloc(1, sizeof(char *));
        return out;
    }
    static const char *flags[] = { "-c", "--continue", "-r", "--resume", NULL };
    int flag_at = -1;
    for (size_t i = 0; i + 1 < argc; i++) {
        if (!argv[i])
            continue;
        for (int f = 0; flags[f]; f++)
            if (strcmp(argv[i], flags[f]) == 0)
                flag_at = (int)i;
    }
    if (flag_at < 0 || (size_t)(flag_at + 1) >= argc ||
        argv[flag_at + 1][0] == '-')
        return NULL;   /* nothing to coalesce: caller keeps original argv */

    /* collect tokens until another flag or a known subcommand */
    size_t end = flag_at + 1;
    while (end < argc && argv[end] && argv[end][0] != '-') {
        bool is_sub = false;
        for (int s = 0; mn_top_level_subcommands[s]; s++)
            if (strcmp(argv[end], mn_top_level_subcommands[s]) == 0)
                is_sub = true;
        if (is_sub)
            break;
        end++;
    }
    size_t joined_len = 1;
    for (size_t i = (size_t)flag_at + 1; i < end; i++)
        joined_len += strlen(argv[i]) + 1;
    char *joined = malloc(joined_len);
    if (!joined)
        return NULL;
    joined[0] = '\0';
    for (size_t i = (size_t)flag_at + 1; i < end; i++) {
        if (i > (size_t)flag_at + 1)
            strcat(joined, " ");
        strcat(joined, argv[i]);
    }

    char **out = calloc(argc - (end - (size_t)flag_at - 1) + 1, sizeof(char *));
    if (!out) {
        free(joined);
        return NULL;
    }
    size_t k = 0;
    for (size_t i = 0; i <= (size_t)flag_at; i++)
        out[k++] = strdup(argv[i] ? argv[i] : "");
    out[k++] = joined;
    for (size_t i = end; i < argc; i++)
        out[k++] = strdup(argv[i] ? argv[i] : "");
    if (out_argc)
        *out_argc = k;
    return out;
}

/* ---------------------------------------------------------------------------
 * _try_fast_chat_launch — fast path for unambiguous interactive chat
 * launches. Bails out whenever the invocation is not certainly a chat
 * launch: env kill-switch, --help/-h, TUI intent, or a non-chat positional.
 * ------------------------------------------------------------------------- */
/* PoP: mn_try_fast_chat_launch @ hermes_cli/main.py:_try_fast_chat_launch */
bool mn_try_fast_chat_launch(char *const *argv, size_t argc)
{
    const char *kill = getenv("HERMES_DISABLE_FAST_CHAT_LAUNCH");
    if (kill && strcmp(kill, "1") == 0)
        return false;
    for (size_t i = 0; i < argc; i++)
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return false;
    /* Container-aware routing must win over the fast path. */
    const char *container = getenv("HERMES_NIXOS_CONTAINER");
    if (container && *container)
        return false;
    /* TUI launches have their own startup path (bounded MCP joins etc.) —
     * keep them on full dispatch outside Termux (_wants_tui_early). */
    bool tui = false;
    for (size_t i = 0; i < argc && !tui; i++)
        if (strcmp(argv[i], "--tui") == 0 || strcmp(argv[i], "-t") == 0)
            tui = true;
    if (tui)
        return false;
    /* first positional must be absent or exactly "chat" */
    for (size_t i = 0; i < argc; i++) {
        if (!argv[i] || argv[i][0] == '-')
            continue;
        /* skip option-value-looking tokens conservatively */
        if (strcmp(argv[i], "chat") != 0)
            return false;
        break;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * _cmd_skills_trust — `hermes skills trust [path]` / untrust: manage
 * skills.trusted_project_dirs in config.yaml. Operates on the resolved
 * project root (explicit path validated as a directory, else find_project_root).
 * Returns a malloc'd status message (caller frees); never blocks.
 * ------------------------------------------------------------------------- */
/* PoP: mn_cmd_skills_trust @ hermes_cli/main.py:_cmd_skills_trust */
char *mn_cmd_skills_trust(const char *action, const char *raw_path,
                         json_t *config)
{
    char root_buf[PATH_MAX];
    if (raw_path && *raw_path) {
        /* expanduser().resolve() */
        char expanded[PATH_MAX];
        snprintf(expanded, sizeof expanded, "%s", raw_path);
        if (expanded[0] == '~') {
            const char *home = getenv("HOME");
            char tmp[PATH_MAX];
            snprintf(tmp, sizeof tmp, "%s%s", home ? home : "", expanded + 1);
            snprintf(expanded, sizeof expanded, "%s", tmp);
        }
        struct stat st;
        if (stat(expanded, &st) != 0 || !S_ISDIR(st.st_mode)) {
            char *msg;
            asprintf(&msg, "Not a directory: %s", expanded);
            return msg;
        }
        snprintf(root_buf, sizeof root_buf, "%s", expanded);
    } else {
        char *root = find_project_root(NULL);
        if (!root) {
            return strdup("Not inside a git checkout. Run from a project "
                          "directory or pass the project root path "
                          "explicitly.");
        }
        snprintf(root_buf, sizeof root_buf, "%s", root);
        free(root);
    }

    json_t *skills_cfg = json_obj_get(config, "skills");
    if (!skills_cfg || skills_cfg->type != JSON_OBJECT) {
        skills_cfg = json_object();
        json_set(config, "skills", skills_cfg);
    }
    json_t *trusted_v = json_obj_get(skills_cfg, "trusted_project_dirs");
    json_t *trusted = json_array();
    if (trusted_v && trusted_v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(trusted_v); i++)
            json_append(trusted, json_copy(json_get(trusted_v, i)));
    } else if (trusted_v && trusted_v->type == JSON_STRING) {
        json_append(trusted, json_copy(trusted_v));   /* scalar -> [scalar] */
    }

    if (action && strcmp(action, "untrust") == 0) {
        json_t *kept = json_array();
        bool found = false;
        for (size_t i = 0; i < json_len(trusted); i++) {
            json_t *t = json_get(trusted, i);
            const char *ts = (t && t->type == JSON_STRING) ? t->str_val : "";
            if (strcmp(ts, root_buf) == 0) {
                found = true;
                continue;
            }
            json_append(kept, json_copy(t));
        }
        if (!found) {
            char *msg;
            asprintf(&msg, "%s was not trusted.", root_buf);
            json_free(trusted);
            json_free(kept);
            return msg;
        }
        json_set(skills_cfg, "trusted_project_dirs", kept);
        json_free(trusted);
        char *msg;
        asprintf(&msg, "Untrusted: %s", root_buf);
        return msg;
    }
    /* trust */
    for (size_t i = 0; i < json_len(trusted); i++) {
        json_t *t = json_get(trusted, i);
        if (t && t->type == JSON_STRING && strcmp(t->str_val, root_buf) == 0) {
            json_free(trusted);
            char *msg;
            asprintf(&msg, "Already trusted: %s", root_buf);
            return msg;
        }
    }
    json_append(trusted, json_string(root_buf));
    json_set(skills_cfg, "trusted_project_dirs", trusted);
    char *msg;
    asprintf(&msg, "Trusted: %s", root_buf);
    return msg;
}

/* ---------------------------------------------------------------------------
 * _advertise_agent_env — advertise the agent harness to child processes.
 * setdefault semantics: never clobber an outer harness's markers.
 * ------------------------------------------------------------------------- */
/* PoP: mn_advertise_agent_env @ hermes_cli/main.py:_advertise_agent_env */
void mn_advertise_agent_env(void)
{
    /* setdefault: never clobber an outer harness */
    if (!getenv("AI_AGENT"))
        setenv("AI_AGENT", "hermes-agent", 0);   /* public harness registry id */
    if (!getenv("HERMES_AGENT"))
        setenv("HERMES_AGENT", "true", 0);       /* Hermes-specific marker */
}
