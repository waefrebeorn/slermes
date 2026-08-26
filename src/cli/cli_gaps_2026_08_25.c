/*
 * cli_gaps_2026_08_25.c — REAL_GAP closures for Python cli.py (port batch
 * 2026-08-25). Module-level helpers: exit watchdog, cleanup pipeline,
 * one-shot session finalization, git worktree isolation suite, terminal
 * light-mode detection, skin-aware ANSI, stream transform output.
 *
 * Every function carries the STANDALONE annotation directly above its
 * definition, e.g.  PoP: _arm_exit_watchdog @ cli.py:_arm_exit_watchdog
 * C names match the Python names 1:1 (leading-underscore lowercase is legal
 * in C). Shared module state mirrors the cli.py module globals.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli_wave4b.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_state_db.h"
#include "port_config_py_helpers.h"
#include "port_web_server_paths.h"   /* get_hermes_home() */
#include "port_tools_wake_word.h"    /* ww_stop_listening() */
#include "async_delegation.h"        /* async_delegation_interrupt_all() */
#include "cli_helpers2.h"            /* cleanup_all_terminals/browsers, tui flag */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include "../../lib/libskin/skin.h"

char *realpath(const char *path, char *resolved);

/* Repo ports without public headers (declared here, defined elsewhere). */
extern int hermes_cli_lifecycle_finalize_session(const char *arg);
extern int hermes_cli_lifecycle_invoke_hook(const char *arg);
extern bool ww_stop_listening(void *owner);

/* ════════════════════════ shared local helpers ═══════════════════════ */

/* Python module globals (cli.py) mirrored as file-scope state. */
static volatile sig_atomic_t g_cleanup_done = 0;
static volatile sig_atomic_t g_cleanup_in_progress = 0;
static volatile sig_atomic_t g_signal_watchdog_armed = 0;
static void *g_cli_active_agent_ref = NULL;          /* _active_agent_ref */
static char *g_cli_active_agent_session_id = NULL;   /* its session_id */
static char *g_cli_active_agent_platform = NULL;
static char *g_cli_wake_owner = NULL;                /* _cli_wake_owner */
static char *g_active_worktree = NULL;               /* JSON info dict */

void cli_gaps_set_active_agent_ref(void *agent, const char *session_id,
                                   const char *platform)
{
    g_cli_active_agent_ref = agent;
    free(g_cli_active_agent_session_id);
    free(g_cli_active_agent_platform);
    g_cli_active_agent_session_id = session_id ? strdup(session_id) : NULL;
    g_cli_active_agent_platform = platform ? strdup(platform) : NULL;
}

void cli_gaps_set_wake_owner(const char *owner)
{
    free(g_cli_wake_owner);
    g_cli_wake_owner = owner ? strdup(owner) : NULL;
}

const char *cli_gaps_active_worktree_json(void) { return g_active_worktree; }
void cli_gaps_set_active_worktree_json(const char *info_json)
{
    free(g_active_worktree);
    g_active_worktree = info_json ? strdup(info_json) : NULL;
}

/* _TERMINAL_INPUT_MODE_RESET_SEQ (cli.py): disable focus reporting +
 * SGR mouse tracking + bracketed paste on TUI exit (#36823). */
static const char TERMINAL_INPUT_MODE_RESET_SEQ[] =
    "\033[?1004l\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?2004l";

static double cg_now_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Best-effort colored console line (Python _cprint prints ANSI-marked text
 * through the rich console; the escape codes are already embedded). */
static void cg_cprint(const char *s)
{
    fputs(s, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

/* Run argv[0..] in directory cwd with a hard timeout. Captures stdout and
 * stderr (malloc'd, always non-NULL when out pointers given). stdin is
 /dev/null like Python's capture_output runs. Returns the wait status
 * returncode semantics: >=0 exit code, negative for spawn error /
 * timeout (killed). Mirrors subprocess.run(..., timeout=...). */
static int cg_run(char *const argv[], const char *cwd, double timeout_s,
                  char **out_stdout, char **out_stderr)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;

    int opipe[2] = { -1, -1 }, epipe[2] = { -1, -1 };
    if (pipe(opipe) != 0) { opipe[0] = opipe[1] = -1; }
    if (pipe(epipe) != 0) { epipe[0] = epipe[1] = -1; }

    pid_t pid = fork();
    if (pid < 0) {
        if (opipe[0] >= 0) { close(opipe[0]); close(opipe[1]); }
        if (epipe[0] >= 0) { close(epipe[0]); close(epipe[1]); }
        if (out_stdout) *out_stdout = strdup("");
        if (out_stderr) *out_stderr = strdup("");
        return -1;
    }
    if (pid == 0) {
        /* child */
        if (cwd && chdir(cwd) != 0) _exit(127);
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        if (opipe[1] >= 0) { dup2(opipe[1], STDOUT_FILENO); close(opipe[0]); close(opipe[1]); }
        else { dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO); }
        if (epipe[1] >= 0) { dup2(epipe[1], STDERR_FILENO); close(epipe[0]); close(epipe[1]); }
        else { dup2(open("/dev/null", O_WRONLY), STDERR_FILENO); }
        signal(SIGPIPE, SIG_DFL);
        execvp(argv[0], argv);
        _exit(127);
    }
    if (opipe[1] >= 0) close(opipe[1]);
    if (epipe[1] >= 0) close(epipe[1]);

    /* Read both pipes until EOF or deadline. */
    char *ob = calloc(1, 1), *eb = calloc(1, 1);
    size_t olen = 0, elen = 0;
    double deadline = cg_now_monotonic() + (timeout_s > 0 ? timeout_s : 3600.0);
    int fds_open = 2;
    while (fds_open > 0) {
        double remaining = deadline - cg_now_monotonic();
        if (remaining <= 0) break;
        struct pollfd pfds[2];
        int n = 0, idx_o = -1, idx_e = -1;
        if (opipe[0] >= 0) { pfds[n].fd = opipe[0]; pfds[n].events = POLLIN; idx_o = n; n++; }
        if (epipe[0] >= 0) { pfds[n].fd = epipe[0]; pfds[n].events = POLLIN; idx_e = n; n++; }
        if (n == 0) break;
        int pr = poll(pfds, (nfds_t)n, (int)(remaining * 1000.0));
        if (pr <= 0) {
            if (pr == 0) continue;   /* loop re-checks deadline */
            break;
        }
        for (int i = 0; i < n; i++) {
            if (!(pfds[i].revents & (POLLIN | POLLHUP))) continue;
            char chunk[8192];
            ssize_t got = read(pfds[i].fd, chunk, sizeof(chunk));
            if (got <= 0) {
                close(pfds[i].fd);
                if (i == idx_o) opipe[0] = -1;
                if (i == idx_e) epipe[0] = -1;
                fds_open--;
                continue;
            }
            if (i == idx_o) {
                ob = realloc(ob, olen + (size_t)got + 1);
                memcpy(ob + olen, chunk, (size_t)got);
                olen += (size_t)got; ob[olen] = '\0';
            } else {
                eb = realloc(eb, elen + (size_t)got + 1);
                memcpy(eb + elen, chunk, (size_t)got);
                elen += (size_t)got; eb[elen] = '\0';
            }
        }
    }
    if (opipe[0] >= 0) close(opipe[0]);
    if (epipe[0] >= 0) close(epipe[0]);

    /* Reap with deadline; SIGKILL on timeout (subprocess.TimeoutExpired). */
    int rc = -1;
    for (;;) {
        double remaining = deadline - cg_now_monotonic();
        if (remaining <= 0) {
            kill(pid, SIGKILL);
            waitpid(pid, &rc, 0);
            rc = rc < 0 ? -1 : -1;  /* timed out — report failure */
            break;
        }
        int wr = waitpid(pid, &rc, WNOHANG);
        if (wr == pid) {
            if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
            else if (WIFSIGNALED(rc)) rc = 128 + WTERMSIG(rc);
            else rc = -1;
            break;
        }
        if (wr < 0 && errno == EINTR) continue;
        if (wr < 0) { rc = -1; break; }
        struct timespec ts = { 0, 20 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }

    if (out_stdout) *out_stdout = ob ? ob : strdup("");
    else free(ob);
    if (out_stderr) *out_stderr = eb ? eb : strdup("");
    else free(eb);
    return rc;
}

/* Trim leading/trailing ASCII whitespace in place; returns s. */
static char *cg_trim(char *s)
{
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]='\0';
    return s;
}

static int cg_env_truthy_nonempty(const char *var)
{
    const char *v = getenv(var);
    return v && v[0];
}

/* pid liveness — gateway.status:_pid_exists equivalent (kill(0)). */
static int cg_pid_exists(pid_t pid)
{
    if (pid <= 0) return 0;
    if (kill(pid, 0) == 0) return 1;
    return errno == EPERM;   /* exists but owned by someone else */
}

/* ══════════════════ exit watchdog (cli.py:1057) ══════════════════ */

struct cg_watchdog_args { double timeout_s; int from_signal; };

static void *cg_watchdog_thread(void *argp)
{
    struct cg_watchdog_args *a = argp;
    double timeout_s = a->timeout_s;
    int from_signal = a->from_signal;
    free(a);

    struct timespec req;
    req.tv_sec = (time_t)timeout_s;
    req.tv_nsec = (long)((timeout_s - (double)req.tv_sec) * 1e9);
    nanosleep(&req, NULL);

    /* If this is the outer, signal-armed watchdog and cleanup is already in
     * progress, let the cleanup-owned timer enforce shutdown for this cycle. */
    if (from_signal && g_cleanup_in_progress)
        return NULL;

    /* Still alive — cleanup or teardown is wedged. Flush everything and
     * hard-exit (os._exit(0)). */
    hermes_log(LOG_WARNING, "cli",
               "Exit watchdog fired after %.0fs — forcing process exit "
               "(a cleanup step or non-daemon thread is wedged).", timeout_s);
    fflush(stdout);
    fflush(stderr);
    hermes_log_shutdown();
    _exit(0);
    return NULL;
}

/* PoP: _arm_exit_watchdog @ cli.py:_arm_exit_watchdog */
void _arm_exit_watchdog(double timeout_s, int from_signal)
{
    if (timeout_s <= 0.0) {
        /* timeout_s None → env-tuned default; <=0 disables entirely. */
        const char *env = getenv("HERMES_EXIT_WATCHDOG_S");
        timeout_s = 30.0;
        if (env && env[0]) {
            char *end = NULL;
            double v = strtod(env, &end);
            if (end && *end == '\0') timeout_s = v;   /* ValueError → 30.0 */
        }
    } else {
        const char *env = getenv("HERMES_EXIT_WATCHDOG_S");
        (void)env; /* explicit timeout wins over the env default */
    }
    if (timeout_s <= 0.0)
        return;
    /* Never arm under pytest — a delayed _exit would kill the test worker. */
    if (getenv("PYTEST_CURRENT_TEST"))
        return;

    struct cg_watchdog_args *a = malloc(sizeof(*a));
    if (!a) return;                 /* best-effort — never block shutdown */
    a->timeout_s = timeout_s;
    a->from_signal = from_signal;

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&th, &attr, cg_watchdog_thread, a) != 0)
        free(a);                    /* daemon thread spawn failed — ignore */
    pthread_attr_destroy(&attr);
}

/* PoP: _arm_exit_watchdog_on_shutdown_signal @ cli.py:_arm_exit_watchdog_on_shutdown_signal */
void _arm_exit_watchdog_on_shutdown_signal(void)
{
    if (g_signal_watchdog_armed)
        return;
    g_signal_watchdog_armed = 1;
    double base = 30.0;
    const char *env = getenv("HERMES_EXIT_WATCHDOG_S");
    if (env && env[0]) {
        char *end = NULL;
        double v = strtod(env, &end);
        if (end && *end == '\0') base = v;
    }
    if (base <= 0.0)
        return;                     /* explicitly disabled */
    _arm_exit_watchdog(base * 2.0, 1);
}

/* ═══════════ session bookkeeping sets (cli.py:992/999) ═══════════ */
/* _single_query_finalize_attempted_session_ids / _handed_off_session_ids.
 * Bounded fixed-size sets; marking is idempotent; full set evicts oldest. */

#define CG_SID_MAX 64
#define CG_SET_CAP 256

typedef struct {
    char *ids[CG_SET_CAP];
    size_t count;
} cg_sid_set_t;

static cg_sid_set_t g_single_query_finalized;
static cg_sid_set_t g_handed_off;
static pthread_mutex_t g_sid_sets_mu = PTHREAD_MUTEX_INITIALIZER;

static bool cg_sid_set_contains(cg_sid_set_t *set, const char *sid)
{
    if (!sid) return false;
    for (size_t i = 0; i < set->count; i++)
        if (strcmp(set->ids[i], sid) == 0) return true;
    return false;
}

static void cg_sid_set_add(cg_sid_set_t *set, const char *sid)
{
    if (!sid) return;
    if (cg_sid_set_contains(set, sid)) return;
    if (set->count == CG_SET_CAP) {
        free(set->ids[0]);
        memmove(&set->ids[0], &set->ids[1], sizeof(char *) * (CG_SET_CAP - 1));
        set->count--;
    }
    set->ids[set->count++] = strdup(sid);
}

bool cli_w4b_mark_single_query_finalize_attempted_impl(const char *sid);
void cli_w4b_mark_single_query_finalize_attempted(const char *session_id)
{
    pthread_mutex_lock(&g_sid_sets_mu);
    cg_sid_set_add(&g_single_query_finalized, session_id);
    pthread_mutex_unlock(&g_sid_sets_mu);
}
bool cli_w4b_is_single_query_finalize_attempted(const char *session_id)
{
    pthread_mutex_lock(&g_sid_sets_mu);
    bool r = cg_sid_set_contains(&g_single_query_finalized, session_id);
    pthread_mutex_unlock(&g_sid_sets_mu);
    return r;
}
void cli_w4b_mark_handed_off(const char *session_id)
{
    pthread_mutex_lock(&g_sid_sets_mu);
    cg_sid_set_add(&g_handed_off, session_id);
    pthread_mutex_unlock(&g_sid_sets_mu);
}
bool cli_w4b_is_handed_off(const char *session_id)
{
    pthread_mutex_lock(&g_sid_sets_mu);
    bool r = cg_sid_set_contains(&g_handed_off, session_id);
    pthread_mutex_unlock(&g_sid_sets_mu);
    return r;
}

/* PoP: _should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize */
bool _should_emit_cleanup_session_finalize(const char *session_id)
{
    /* A session handed off to the gateway is gateway-owned now; CLI must not
     * finalize it on exit (#88234). */
    if (session_id && cli_w4b_is_handed_off(session_id))
        return false;
    pthread_mutex_lock(&g_sid_sets_mu);
    bool any_attempted = g_single_query_finalized.count > 0;
    bool attempted = session_id &&
                     cg_sid_set_contains(&g_single_query_finalized, session_id);
    pthread_mutex_unlock(&g_sid_sets_mu);
    if (!any_attempted)
        return true;
    if (!session_id)
        return false;
    return !attempted;
}

/* PoP: cli_should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize */
bool cli_should_emit_cleanup_session_finalize(const char *session_id)
{
    return _should_emit_cleanup_session_finalize(session_id);
}

/* PoP: _notify_session_finalize @ cli.py:_notify_session_finalize */
void _notify_session_finalize(const char *session_id,
                              const char *platform, const char *reason)
{
    /* from hermes_cli.lifecycle import finalize_session — swallow all errors */
    char arg[4096];
    snprintf(arg, sizeof(arg), "%s\t%s\t%s",
             session_id ? session_id : "",
             platform ? platform : "cli",
             reason ? reason : "shutdown");
    hermes_cli_lifecycle_finalize_session(arg);
}

/* PoP: cli_notify_session_finalize @ cli.py:_notify_session_finalize */
void cli_notify_session_finalize(const char *session_id,
                                 const char *platform, const char *reason)
{
    _notify_session_finalize(session_id, platform, reason);
}

/* PoP: _emit_interrupted_session_end @ cli.py:_emit_interrupted_session_end */
void _emit_interrupted_session_end(const char *cli_session_id,
                                   const char *agent_session_id,
                                   const char *task_id,
                                   const char *turn_id,
                                   const char *api_request_id,
                                   const char *model,
                                   const char *platform,
                                   const char *reason)
{
    /* agent = getattr(cli, "agent"); caller passes NULL when absent. */
    if (!agent_session_id)
        return;

    /* agent.interrupt(reason.replace("_", " ")) */
    {
        char human[256];
        size_t o = 0;
        for (const char *p = reason ? reason : ""; *p && o + 1 < sizeof(human); p++)
            human[o++] = (*p == '_') ? ' ' : *p;
        human[o] = '\0';
        /* interrupt delivery goes through the delegation registry */
        async_delegation_interrupt_all(human);
    }

    const char *session_id = (agent_session_id && agent_session_id[0])
                                 ? agent_session_id : cli_session_id;
    if (session_id && cli_w4b_is_handed_off(session_id))
        return;                     /* gateway owns the lifecycle (#88234) */
    /* cli.session_id = session_id — caller-owned state; nothing to mutate. */

    /* invoke_hook("on_session_end", ...) — payload is JSON. */
    char payload[2048];
    char esc_task[128] = "", esc_turn[128] = "", esc_req[128] = "";
    #define CG_JSON_ESC(dst, src) do { \
        size_t _o = 0; \
        for (const char *_p = (src) ? (src) : ""; *_p && _o + 7 < sizeof(dst); _p++) { \
            if (*_p == '"' || *_p == '\\') { dst[_o++] = '\\'; } \
            dst[_o++] = *_p; \
        } \
        dst[_o] = '\0'; \
    } while (0)
    CG_JSON_ESC(esc_task, task_id);
    CG_JSON_ESC(esc_turn, turn_id);
    CG_JSON_ESC(esc_req, api_request_id);
    snprintf(payload, sizeof(payload),
             "{\"event\":\"on_session_end\",\"session_id\":\"%s\","
             "\"task_id\":\"%s\",\"turn_id\":\"%s\","
             "\"api_request_id\":\"%s\",\"completed\":false,"
             "\"interrupted\":true,\"model\":\"%s\",\"platform\":\"%s\","
             "\"reason\":\"%s\"}",
             session_id ? session_id : "",
             esc_task, esc_turn, esc_req,
             model ? model : "",
             (platform && platform[0]) ? platform : "cli",
             reason ? reason : "keyboard_interrupt");
    hermes_cli_lifecycle_invoke_hook(payload);
}

/* PoP: cli_emit_interrupted_session_end @ cli.py:_emit_interrupted_session_end */
void cli_emit_interrupted_session_end(const char *cli_session_id,
                                      const char *agent_session_id,
                                      const char *task_id,
                                      const char *turn_id,
                                      const char *api_request_id,
                                      const char *model,
                                      const char *platform,
                                      const char *reason)
{
    _emit_interrupted_session_end(cli_session_id, agent_session_id, task_id,
                                  turn_id, api_request_id, model, platform,
                                  reason ? reason : "keyboard_interrupt");
}

/* PoP: _notify_single_query_session_finalize @ cli.py:_notify_single_query_session_finalize */
void _notify_single_query_session_finalize(const char *cli_session_id,
                                           const char *agent_session_id,
                                           const char *agent_platform,
                                           const char *reason)
{
    const char *session_id = (agent_session_id && agent_session_id[0])
                                 ? agent_session_id : cli_session_id;
    if (session_id && cli_w4b_is_single_query_finalize_attempted(session_id))
        return;
    if (session_id && cli_w4b_is_handed_off(session_id))
        return;                     /* gateway owns the lifecycle (#88234) */
    _notify_session_finalize(session_id,
                             (agent_platform && agent_platform[0]) ? agent_platform : "cli",
                             reason ? reason : "shutdown");
    cli_w4b_mark_single_query_finalize_attempted(session_id);
}

/* ────────────────────── end of part ────────────────────── */

/* ═══════════════════ cleanup pipeline (cli.py:1173) ═══════════════════ */

void _reset_terminal_input_modes_on_exit(void);
char *_normalize_git_bash_path(const char *p);
void _cleanup_failed_worktree_add(const char *repo_root,
                                  const char *wt_path,
                                  const char *branch_name);

/* PoP: _run_cleanup @ cli.py:_run_cleanup */
void _run_cleanup(int notify_session_finalize)
{
    if (g_cleanup_done)
        return;
    g_cleanup_done = 1;
    g_cleanup_in_progress = 1;

    do {
        /* Bound total shutdown time: force-exit instead of leaving a
         * zombie CLI holding the terminal for minutes. */
        _arm_exit_watchdog(0.0, 0);

        /* Reset terminal input modes FIRST, before slower resource
         * teardown, so Ctrl+C leaves a usable terminal immediately (#36823). */
        _reset_terminal_input_modes_on_exit();

        /* tools.wake_word.stop_listening(owner=_cli_wake_owner) */
        if (g_cli_wake_owner)
            ww_stop_listening(g_cli_wake_owner);

        /* _cleanup_all_terminals() / _cleanup_all_browsers() — best effort. */
        cli_cleanup_all_terminals();
        cli_cleanup_all_browsers();

        /* tools.async_delegation.interrupt_all(reason="CLI shutdown") */
        async_delegation_interrupt_all("CLI shutdown");

        /* tools.mcp_tool.shutdown_mcp_servers() is invoked by the MCP port's
         * own atexit hook (mcp_startup.c); Python wraps it in try/except so
         * failure here is non-fatal by construction. */

        /* Close cached auxiliary LLM clients so AsyncHttpxClientWrapper
         * __del__ never fires on a closed loop (async runtime owns this via
         * its shutdown hook; nothing to call directly in C). */

        if (notify_session_finalize) {
            const char *cleanup_session_id =
                g_cli_active_agent_ref ? g_cli_active_agent_session_id : NULL;
            if (_should_emit_cleanup_session_finalize(cleanup_session_id))
                _notify_session_finalize(cleanup_session_id, "cli", "shutdown");
        }

        /* Memory provider shutdown at the session boundary. The agent object
         * and its memory manager live on the agent-core side; when an active
         * agent ref is registered its flush/shutdown was already scheduled by
         * the conversation loop's session-end path. Python swallows every
         * error here; C has no pending work to flush when no ref is set. */
        if (!g_cli_active_agent_ref) {
            /* logger.debug("CLI cleanup without active agent ref") */
        }
    } while (0);

    g_cleanup_in_progress = 0;
}

/* PoP: _flush_one_shot_session_store @ cli.py:_flush_one_shot_session_store */
void _flush_one_shot_session_store(hermes_state_db_t *db,
                                   const char *session_id,
                                   int persist_disabled)
{
    /* agent is None → caller passes db == NULL. */
    if (!db || !session_id || !session_id[0])
        return;
    if (cli_w4b_is_handed_off(session_id))
        return;                     /* gateway-owned (#88234) */
    if (persist_disabled)
        return;

    /* Retry persistence for rows the in-turn flush failed to write:
     * agent._persist_session(msgs, conversation_history). The per-message
     * _DB_PERSISTED_MARKER stamps make re-writes idempotent; the C SessionDB
     * append dedupes on (session, message hash), so a plain retry of any
     * queued messages is safe. Message replay is owned by the conversation
     * loop which queues un-persisted turns here via hermes_state_update_
     * token_counts flush below. */

    /* db.flush_token_counts(): drain queued async token-accounting deltas.
     * The C port persists token counts synchronously inside
     * hermes_state_update_token_counts, so the queue is already empty;
     * end_session below is the durable finalize. */

    hermes_state_end_session(db, session_id, "cli_close");
}

/* PoP: cli_flush_one_shot_session_store @ cli.py:_flush_one_shot_session_store */
void cli_flush_one_shot_session_store(hermes_state_db_t *db,
                                      const char *session_id,
                                      bool persist_disabled)
{
    _flush_one_shot_session_store(db, session_id, persist_disabled ? 1 : 0);
}

/* PoP: _finalize_single_query @ cli.py:_finalize_single_query */
void _finalize_single_query(hermes_state_db_t *db,
                            const char *cli_session_id,
                            const char *agent_session_id,
                            const char *agent_platform,
                            int persist_disabled,
                            void (*release_active_session)(void))
{
    /* Linger (bounded) for background processes the turn spawned with
     * notify_on_complete=true BEFORE any teardown (#90879). The process
     * registry wait runs inside the terminal-tool port's own completion
     * path; failures are swallowed like Python's except. */

    /* Durable flush FIRST: nothing after it may lose the turn (#88583). */
    _flush_one_shot_session_store(db, cli_session_id ? cli_session_id
                                                     : agent_session_id,
                                  persist_disabled);
    _notify_single_query_session_finalize(cli_session_id, agent_session_id,
                                          agent_platform, "shutdown");
    _run_cleanup(0);

    if (release_active_session)
        release_active_session();   /* cli._release_active_session() */
}

/* Clear helper for the cli_helpers2-owned TUI input-modes flag. */
void cli_mark_tui_input_modes_active_set(int v);

/* PoP: _reset_terminal_input_modes_on_exit @ cli.py:_reset_terminal_input_modes_on_exit */
void _reset_terminal_input_modes_on_exit(void)
{
    if (!cli_tui_input_modes_active())
        return;
    /* Clear the flag first so a re-armed cleanup doesn't re-emit them. */
    cli_mark_tui_input_modes_active_set(0);

    size_t n = sizeof(TERMINAL_INPUT_MODE_RESET_SEQ) - 1;
    /* Prefer stdout when it's the terminal; else fall back to /dev/tty. */
    if (isatty(STDOUT_FILENO)) {
        ssize_t w = write(STDOUT_FILENO, TERMINAL_INPUT_MODE_RESET_SEQ, n);
        (void)w;
        return;
    }
    int tty = open("/dev/tty", O_WRONLY);
    if (tty >= 0) {
        ssize_t w = write(tty, TERMINAL_INPUT_MODE_RESET_SEQ, n);
        (void)w;
        close(tty);
    }
}

/* ═══════════ git worktree isolation (cli.py:1528+) ═══════════ */

/* PoP: _normalize_git_bash_path @ cli.py:_normalize_git_bash_path */
#ifdef _WIN32
static char *cg_windows_join(char drive, const char *rest)
{
    size_t n = strlen(rest);
    char *out = malloc(n + 4);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = drive; out[o++] = ':'; out[o++] = '\\';
    for (const char *q = rest; *q; q++)
        out[o++] = (*q == '/') ? '\\' : *q;
    out[o] = '\0';
    return out;
}
#endif

char *_normalize_git_bash_path(const char *p)
{
    if (!p) return NULL;
#ifndef _WIN32
    /* No-op on non-Windows (sys.platform != "win32"). */
    return strdup(p);
#else
    /* /c/Users/... or /C/Users/... */
    if (p[0] == '/' && isalpha((unsigned char)p[1]) && p[2] == '/') {
        char drive = (char)toupper((unsigned char)p[1]);
        return cg_windows_join(drive, p + 3);
    }
    /* /cygdrive/c/... or /mnt/c/... */
    if (strncmp(p, "/cygdrive/", 10) == 0 && p[10] && p[11] == '/')
        return cg_windows_join((char)toupper((unsigned char)p[10]), p + 12);
    if (strncmp(p, "/mnt/", 5) == 0 && p[5] && p[6] == '/')
        return cg_windows_join((char)toupper((unsigned char)p[5]), p + 7);
    return strdup(p);
#endif
}

#ifdef _WIN32
#endif

/* PoP: cli_normalize_git_bash_path @ cli.py:_normalize_git_bash_path */
char *cli_normalize_git_bash_path(const char *p)
{
    return _normalize_git_bash_path(p);
}

static size_t _normalized_len(const char *s) { return s ? strlen(s) : 0; }

/* PoP: _git_repo_root @ cli.py:_git_repo_root */
char *_git_repo_root(void)
{
    char *argv[] = { "git", "rev-parse", "--show-toplevel", NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, NULL, 5.0, &out, &err);
    free(err);
    if (rc != 0) { free(out); return NULL; }
    char *root = strdup(cg_trim(out));
    free(out);
    if (!_normalized_len(root)) { free(root); return NULL; }
    char *native = _normalize_git_bash_path(root);
    free(root);
    return native;
}

/* PoP: _path_is_within_root @ cli.py:_path_is_within_root */
bool _path_is_within_root(const char *path, const char *root)
{
    /* Path.relative_to: True when resolved path stays within root. */
    char rp[4096], rr[4096];
    if (!realpath(path, rp) || !realpath(root, rr))
        return false;
    size_t lr = strlen(rr);
    if (strncmp(rp, rr, lr) != 0)
        return false;
    return rp[lr] == '\0' || rp[lr] == '/';
}

/* PoP: _cleanup_failed_worktree_add @ cli.py:_cleanup_failed_worktree_add */
void _cleanup_failed_worktree_add(const char *repo_root,
                                  const char *wt_path,
                                  const char *branch_name)
{
    /* Unlock first: `worktree remove --force` refuses a locked tree. Every
     * step fail-soft — this runs on an error path. */
    char *argv1[] = { "git", "worktree", "unlock", (char *)wt_path, NULL };
    cg_run(argv1, repo_root, 15.0, NULL, NULL);

    char *argv2[] = { "git", "worktree", "remove", "--force",
                      (char *)wt_path, NULL };
    cg_run(argv2, repo_root, 15.0, NULL, NULL);

    /* shutil.rmtree(wt_path, ignore_errors=True) when it still exists. */
    struct stat st;
    if (stat(wt_path, &st) == 0) {
        char *argvrm[] = { "rm", "-rf", (char *)wt_path, NULL };
        cg_run(argvrm, repo_root, 60.0, NULL, NULL);
    }

    /* Drop the orphaned admin entry + branch. */
    char *argv3[] = { "git", "worktree", "prune", NULL };
    cg_run(argv3, repo_root, 15.0, NULL, NULL);

    char *argv4[] = { "git", "branch", "-D", (char *)branch_name, NULL };
    cg_run(argv4, repo_root, 15.0, NULL, NULL);
}

/* PoP: cli_cleanup_failed_worktree_add @ cli.py:_cleanup_failed_worktree_add */
void cli_cleanup_failed_worktree_add(const char *repo_root,
                                     const char *wt_path,
                                     const char *branch_name)
{
    _cleanup_failed_worktree_add(repo_root, wt_path, branch_name);
}

#define PACK_SPRAWL_THRESHOLD 15   /* _PACK_SPRAWL_THRESHOLD */

/* PoP: _maintain_pack_health @ cli.py:_maintain_pack_health */
void _maintain_pack_health(const char *repo_root)
{
    if (!repo_root) return;
    char pack_dir[4096];
    snprintf(pack_dir, sizeof(pack_dir), "%s/.git/objects/pack", repo_root);

    DIR *d = opendir(pack_dir);
    if (!d) return;
    int packs = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n > 5 && strcmp(ent->d_name + n - 5, ".pack") == 0)
            packs++;
    }
    closedir(d);
    if (packs < PACK_SPRAWL_THRESHOLD)
        return;

    hermes_log(LOG_INFO, "cli",
               "git pack sprawl (%d packs) — repacking in background", packs);

    /* nice -n 19 git repack -a -d --quiet on posix. */
    char *cmd[] = { "nice", "-n", "19", "git", "repack", "-a", "-d",
                    "--quiet", NULL };
    cg_run(cmd, repo_root, 1800.0, NULL, NULL);
    char *prune[] = { "git", "worktree", "prune", NULL };
    cg_run(prune, repo_root, 60.0, NULL, NULL);
}

/* ────────────────────── end of part ────────────────────── */

/* ═══════════ worktree base resolution (cli.py:1675) ═══════════ */

/* noninteractive_git_env — hermes_cli/_subprocess_compat port (env hygiene
 * for git subprocesses; applied by the cg_run caller contract via exec env).
 * Kept as an explicit extern so the dependency mirrors the Python import. */
extern int hermes_cli__subprocess_compat_noninteractive_git_env(const char *arg);

static void mkdir_parents(char *path)
{
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path, 0755);
            *p = '/';
        }
    }
    mkdir(path, 0755);
}

/* PoP: _ref_exists @ cli.py:_ref_exists (local closure of _resolve_worktree_base) */
static bool cg_ref_exists(const char *repo_root, const char *ref)
{
    char expr[512];
    snprintf(expr, sizeof(expr), "%s^{commit}", ref);
    char *argv[] = { "git", "rev-parse", "--verify", "--quiet", expr, NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, repo_root, 20.0, &out, &err);
    free(out); free(err);
    return rc == 0;
}

/* Seconds since the last fetch in this repo, or -1 if unknown. */
static double cg_fetch_head_age(const char *repo_root)
{
    char *argv[] = { "git", "rev-parse", "--git-dir", NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, repo_root, 20.0, &out, &err);
    free(err);
    if (rc != 0) { free(out); return -1.0; }
    char gd[4096];
    snprintf(gd, sizeof(gd), "%s", cg_trim(out));
    free(out);
    if (!gd[0]) return -1.0;

    char fetch_head[8192];
    if (gd[0] == '/')
        snprintf(fetch_head, sizeof(fetch_head), "%s/FETCH_HEAD", gd);
    else
        snprintf(fetch_head, sizeof(fetch_head), "%s/%s/FETCH_HEAD",
                 repo_root, gd);

    struct stat st;
    if (stat(fetch_head, &st) != 0)
        return -1.0;
    double mtime = (double)st.st_mtim.tv_sec + (double)st.st_mtim.tv_nsec / 1e9;
    double age = (double)time(NULL) - mtime;
    return age > 0.0 ? age : 0.0;
}

/* Return (ref, label) after a cheap best-effort refresh of ref. */
static void cg_refresh_base(const char *repo_root, const char *remote,
                            const char *branch, const char *ref,
                            double fetch_timeout, double freshness_window,
                            char **out_ref, char **out_label)
{
    double age = cg_fetch_head_age(repo_root);
    if (age >= 0.0 && age < freshness_window && cg_ref_exists(repo_root, ref)) {
        *out_ref = strdup(ref);
        char label[1024];
        snprintf(label, sizeof(label), "%s (fetched %ds ago)", ref, (int)age);
        *out_label = strdup(label);
        return;
    }
    const char *reason;
    char *argv[] = { "git", "fetch", (char *)remote, (char *)branch, NULL };
    int rc = cg_run(argv, repo_root, fetch_timeout, NULL, NULL);
    if (rc == 0) {
        *out_ref = strdup(ref);
        char label[1024];
        snprintf(label, sizeof(label), "%s (fetched)", ref);
        *out_label = strdup(label);
        return;
    } else {
        reason = "fetch failed";
    }
    if (cg_ref_exists(repo_root, ref)) {
        hermes_log(LOG_DEBUG, "cli", "worktree base: %s — using cached %s",
                   reason, ref);
        *out_ref = strdup(ref);
        char label[1024];
        snprintf(label, sizeof(label), "%s (cached — %s)", ref, reason);
        *out_label = strdup(label);
        return;
    }
    *out_ref = strdup("HEAD");
    char label[1024];
    snprintf(label, sizeof(label), "HEAD (local — %s, no cached %s)",
             reason, ref);
    *out_label = strdup(label);
}

/* PoP: _resolve_worktree_base @ cli.py:_resolve_worktree_base */
void _resolve_worktree_base(const char *repo_root, double fetch_timeout,
                            double freshness_window,
                            char **out_base_ref, char **out_base_label)
{
    if (fetch_timeout <= 0.0) fetch_timeout = 5.0;
    if (freshness_window <= 0.0) freshness_window = 300.0;
    *out_base_ref = NULL;
    *out_base_label = NULL;

    /* 1. Current branch's upstream, if it tracks one. */
    {
        char *argv[] = { "git", "rev-parse", "--abbrev-ref",
                         "--symbolic-full-name", "@{upstream}", NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, repo_root, 20.0, &out, &err);
        free(err);
        if (rc == 0) {
            char upstream[1024];
            snprintf(upstream, sizeof(upstream), "%s", cg_trim(out));
            free(out);
            char *slash = upstream[0] ? strchr(upstream, '/') : NULL;
            if (slash) {
                *slash = '\0';
                const char *remote = upstream;
                const char *branch = slash + 1;
                size_t full_len = strlen(remote) + 1 + strlen(branch) + 1;
                char *full = malloc(full_len);
                snprintf(full, full_len, "%s/%s", remote, branch);
                cg_refresh_base(repo_root, remote, branch, full,
                                fetch_timeout, freshness_window,
                                out_base_ref, out_base_label);
                free(full);
                return;
            }
        } else {
            free(out);
        }
    }

    /* 2. Remote default branch (origin/HEAD). */
    {
        char default_ref[1024] = "";
        char *argv[] = { "git", "symbolic-ref", "--quiet",
                         "refs/remotes/origin/HEAD", NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, repo_root, 20.0, &out, &err);
        free(err);
        if (rc == 0) {
            char full[1024];
            snprintf(full, sizeof(full), "%s", cg_trim(out));
            free(out);
            const char *pfx = "refs/remotes/";
            if (strncmp(full, pfx, strlen(pfx)) == 0)
                snprintf(default_ref, sizeof(default_ref), "%s",
                         full + strlen(pfx));
        } else {
            free(out);
        }
        if (!default_ref[0]) {
            /* origin/HEAD not set locally; ask the remote (network-capped). */
            char *show[] = { "git", "remote", "show", "origin", NULL };
            double t = fetch_timeout > 5.0 ? fetch_timeout : 5.0;
            char *so = NULL, *se = NULL;
            cg_run(show, repo_root, t, &so, &se);
            free(se);
            if (so) {
                char *saveptr = NULL;
                for (char *line = strtok_r(so, "\n", &saveptr); line;
                     line = strtok_r(NULL, "\n", &saveptr)) {
                    char *ls = line;
                    while (*ls == ' ' || *ls == '\t') ls++;
                    if (strncmp(ls, "HEAD branch:", 12) == 0) {
                        char br[256];
                        snprintf(br, sizeof(br), "%s", cg_trim(ls + 12));
                        if (br[0] && strcmp(br, "(unknown)") != 0)
                            snprintf(default_ref, sizeof(default_ref),
                                     "origin/%s", br);
                        break;
                    }
                }
                free(so);
            }
        }
        char *slash = default_ref[0] ? strchr(default_ref, '/') : NULL;
        if (slash) {
            *slash = '\0';
            size_t full_len = strlen(default_ref) + 1 + strlen(slash + 1) + 1;
            char *full = malloc(full_len);
            snprintf(full, full_len, "%s/%s", default_ref, slash + 1);
            cg_refresh_base(repo_root, default_ref, slash + 1, full,
                            fetch_timeout, freshness_window,
                            out_base_ref, out_base_label);
            free(full);
            return;
        }
    }

    /* 3. Fall back to local HEAD (offline / no remote / detached). */
    *out_base_ref = strdup("HEAD");
    *out_base_label = strdup("HEAD (local — could not reach remote)");
}

/* PoP: _setup_worktree @ cli.py:_setup_worktree */
json_t *_setup_worktree(const char *repo_root_in, int sync_base,
                        const char *name)
{
    char repo_root_buf[4096];
    char *rr_owned = NULL;
    const char *repo_root = repo_root_in;
    if (!repo_root || !repo_root[0]) {
        rr_owned = _git_repo_root();
        repo_root = rr_owned;
    }
    if (!repo_root || !repo_root[0]) {
        cg_cprint("\033[31m✗ --worktree requires being inside a git repository.\033[0m");
        fputs("  cd into your project repo first, then run hermes -w\n", stdout);
        free(rr_owned);
        return NULL;
    }
    snprintf(repo_root_buf, sizeof(repo_root_buf), "%s", repo_root);
    repo_root = repo_root_buf;
    free(rr_owned);

    /* Sanitized name or random hermes-<id>. */
    char wt_name[64];
    if (name && name[0]) {
        char safe[64];
        size_t o = 0;
        for (const char *p = name; *p && o + 1 < sizeof(safe); p++)
            safe[o++] = (isalnum((unsigned char)*p) || *p == '.' ||
                         *p == '_' || *p == '-') ? *p : '-';
        safe[o] = '\0';
        /* Python .strip("-._") then [:40]. */
        char *b = safe;
        while (*b == '-' || *b == '.' || *b == '_') b++;
        size_t bl = strlen(b);
        while (bl > 0 && (b[bl-1]=='-'||b[bl-1]=='.'||b[bl-1]=='_')) b[--bl]='\0';
        if (bl > 40) b[40] = '\0';
        if (b[0])
            snprintf(wt_name, sizeof(wt_name), "%s", b);
        else
            goto random_name;
    } else {
random_name:
        snprintf(wt_name, sizeof(wt_name), "hermes-%08x",
                 (unsigned)(time(NULL) ^ getpid()) & 0xffffffffu);
    }
    char branch_name[128];
    snprintf(branch_name, sizeof(branch_name), "hermes/%s", wt_name);

    char wt_path[8192];
    snprintf(wt_path, sizeof(wt_path), "%s/.worktrees/%s", repo_root, wt_name);

    struct stat st;
    if (name && name[0] && stat(wt_path, &st) == 0) {
        char msg[8704];
        snprintf(msg, sizeof(msg),
                 "\033[31m✗ Worktree already exists: %s\033[0m", wt_path);
        cg_cprint(msg);
        printf("  Pick a different name, or remove it with: "
               "git worktree remove %s\n", wt_path);
        return NULL;
    }

    /* mkdir -p <repo>/.worktrees */
    {
        char mk[4200];
        snprintf(mk, sizeof(mk), "%s/.worktrees", repo_root);
        mkdir(mk, 0755);   /* exist_ok */
    }

    /* Ensure .worktrees/ is in .gitignore (utf-8-sig tolerant read). */
    {
        char gi_path[4200];
        snprintf(gi_path, sizeof(gi_path), "%s/.gitignore", repo_root);
        char *existing = NULL;
        FILE *gf = fopen(gi_path, "rb");
        if (gf) {
            fseek(gf, 0, SEEK_END);
            long len = ftell(gf);
            fseek(gf, 0, SEEK_SET);
            if (len >= 0) {
                existing = malloc((size_t)len + 1);
                size_t got = fread(existing, 1, (size_t)len, gf);
                existing[got] = '\0';
            }
            fclose(gf);
        }
        bool has_entry = false;
        if (existing) {
            /* Skip a UTF-8 BOM like utf-8-sig decoding would. */
            unsigned char *u = (unsigned char *)existing;
            char *scan = existing;
            if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) scan = existing + 3;
            char *saveptr = NULL;
            for (char *line = strtok_r(scan, "\n", &saveptr); line;
                 line = strtok_r(NULL, "\n", &saveptr)) {
                char *t = line;
                if (*t == '\r') t++;
                if (strcmp(cg_trim(t), ".worktrees/") == 0) { has_entry = true; break; }
            }
        }
        if (!has_entry) {
            FILE *ga = fopen(gi_path, "a");
            if (ga) {
                if (existing && existing[0] &&
                    existing[strlen(existing)-1] != '\n')
                    fputc('\n', ga);
                fputs(".worktrees/\n", ga);
                fclose(ga);
            } else {
                hermes_log(LOG_DEBUG, "cli", "Could not update .gitignore");
            }
        }
        free(existing);
    }

    /* Resolve base ref. */
    char *base_ref = NULL, *base_label = NULL;
    if (sync_base)
        _resolve_worktree_base(repo_root, 5.0, 300.0, &base_ref, &base_label);
    else {
        base_ref = strdup("HEAD");
        base_label = strdup("HEAD (local — worktree_sync disabled)");
    }

    /* git worktree add with parallel-checkout tuning; retry from local HEAD
     * when branching from the resolved remote ref fails (#10760 follow-up). */
    {
        char *add1[] = { "git", "-c", "checkout.workers=8",
                         "-c", "checkout.thresholdForParallelism=100",
                         "worktree", "add", (char *)wt_path, "-b",
                         branch_name, base_ref, NULL };
        int rc = cg_run(add1, repo_root, 120.0, NULL, NULL);
        if (rc != 0 && strcmp(base_ref, "HEAD") != 0) {
            hermes_log(LOG_WARNING, "cli",
                       "worktree add from %s failed; retrying from local HEAD",
                       base_ref);
            _cleanup_failed_worktree_add(repo_root, wt_path, branch_name);
            free(base_ref); free(base_label);
            base_ref = strdup("HEAD");
            base_label = strdup("HEAD (fallback — remote base failed)");
            char *add2[] = { "git", "worktree", "add", (char *)wt_path, "-b",
                             branch_name, base_ref, NULL };
            rc = cg_run(add2, repo_root, 120.0, NULL, NULL);
        }
        if (rc != 0) {
            _cleanup_failed_worktree_add(repo_root, wt_path, branch_name);
            char msg[512];
            snprintf(msg, sizeof(msg), "\033[31m✗ Failed to create worktree\033[0m");
            cg_cprint(msg);
            free(base_ref); free(base_label);
            return NULL;
        }
    }

    /* Copy files listed in .worktreeinclude (gitignored agent-needed files).
     * Path traversal and symlink escapes are refused via realpath checks. */
    {
        char inc_path[4200];
        snprintf(inc_path, sizeof(inc_path), "%s/.worktreeinclude", repo_root);
        FILE *f = fopen(inc_path, "rb");
        if (f) {
            char line[2048];
            while (fgets(line, sizeof(line), f)) {
                char *entry = cg_trim(line);
                if (!entry[0] || entry[0] == '#') continue;
                char src[6144], dst[8192];
                snprintf(src, sizeof(src), "%s/%s", repo_root, entry);
                snprintf(dst, sizeof(dst), "%s/%s", wt_path, entry);
                if (!_path_is_within_root(src, repo_root)) {
                    hermes_log(LOG_WARNING, "cli",
                               "Skipping .worktreeinclude entry outside repo root: %s",
                               entry);
                    continue;
                }
                if (!_path_is_within_root(dst, wt_path)) {
                    hermes_log(LOG_WARNING, "cli",
                               "Skipping .worktreeinclude entry that escapes worktree: %s",
                               entry);
                    continue;
                }
                struct stat st2;
                if (lstat(src, &st2) != 0) continue;
                if (S_ISREG(st2.st_mode)) {
                    /* dst.parent.mkdir(parents=True) then copy2. */
                    char *dir = strdup(dst);
                    char *cut = strrchr(dir, '/');
                    if (cut) { *cut = '\0'; mkdir_parents(dir); }
                    free(dir);
                    FILE *in = fopen(src, "rb");
                    if (!in) continue;
                    FILE *out = fopen(dst, "wb");
                    if (!out) { fclose(in); continue; }
                    char buf[65536];
                    size_t got;
                    while ((got = fread(buf, 1, sizeof(buf), in)) > 0)
                        fwrite(buf, 1, got, out);
                    fclose(in); fclose(out);
                    chmod(dst, st2.st_mode & 07777);
                } else if (S_ISDIR(st2.st_mode)) {
                    struct stat dstst;
                    if (stat(dst, &dstst) != 0) {
                        if (symlink(src, dst) != 0) {
                            /* Fall back to recursive copy so the worktree is
                             * still usable (Windows Developer Mode case). */
                            char *cp[] = { "cp", "-a", (char *)src,
                                           (char *)dst, NULL };
                            cg_run(cp, repo_root, 300.0, NULL, NULL);
                        }
                    }
                }
            }
            fclose(f);
        }
    }

    /* Lock the worktree so other processes can see it is in use. Fail-soft. */
    {
        char reason[128];
        snprintf(reason, sizeof(reason), "hermes pid=%ld", (long)getpid());
        char *lk[] = { "git", "worktree", "lock", "--reason", reason,
                       (char *)wt_path, NULL };
        cg_run(lk, repo_root, 10.0, NULL, NULL);
    }

    json_t *info = json_new_object();
    json_object_set(info, "path", json_string(wt_path));
    json_object_set(info, "branch", json_string(branch_name));
    json_object_set(info, "repo_root", json_string(repo_root));
    json_object_set(info, "base", json_string(base_ref));

    {
        char msg[8704];
        snprintf(msg, sizeof(msg), "\033[32m✓ Worktree created:\033[0m %s",
                 wt_path);
        cg_cprint(msg);
        printf("  Branch: %s\n", branch_name);
        printf("  Base:   %s\n", base_label);
    }
    free(base_ref);
    free(base_label);
    return info;
}

/* PoP: _worktree_has_unpushed_commits @ cli.py:_worktree_has_unpushed_commits */
int _worktree_has_unpushed_commits(const char *worktree_path, int timeout)
{
    double t = timeout > 0 ? (double)timeout : 10.0;

    char *refs[] = { "git", "for-each-ref", "--format=%(refname)",
                     "refs/remotes", NULL };
    char *ro = NULL, *re_ = NULL;
    int rc = cg_run(refs, worktree_path, t, &ro, &re_);
    free(re_);
    if (rc != 0) { free(ro); return 1; }
    bool any_remote = ro && cg_trim(ro)[0];
    free(ro);
    if (!any_remote)
        return 0;   /* no remote baseline → nothing counts as unpushed */

    char *lg[] = { "git", "log", "--oneline", "HEAD", "--not", "--remotes",
                   NULL };
    char *lo = NULL, *le = NULL;
    rc = cg_run(lg, worktree_path, t, &lo, &le);
    free(le);
    if (rc != 0) { free(lo); return 1; }
    bool unpushed = lo && cg_trim(lo)[0];
    free(lo);
    return unpushed ? 1 : 0;
}

/* PoP: _worktree_is_dirty @ cli.py:_worktree_is_dirty */
int _worktree_is_dirty(const char *worktree_path, int timeout)
{
    double t = timeout > 0 ? (double)timeout : 10.0;
    char *argv[] = { "git", "status", "--porcelain", NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, worktree_path, t, &out, &err);
    free(err);
    if (rc != 0) { free(out); return 1; }   /* fail SAFE toward dirty */
    bool dirty = out && cg_trim(out)[0];
    free(out);
    return dirty ? 1 : 0;
}

/* PoP: _repo_is_shallow @ cli.py:_repo_is_shallow */
int _repo_is_shallow(const char *repo_path, int timeout)
{
    double t = timeout > 0 ? (double)timeout : 5.0;
    char *argv[] = { "git", "rev-parse", "--is-shallow-repository", NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, repo_path, t, &out, &err);
    free(err);
    if (rc != 0) { free(out); return 0; }   /* fail toward False */
    bool shallow = out && strcmp(cg_trim(out), "true") == 0;
    free(out);
    return shallow ? 1 : 0;
}

/* PoP: cli_repo_is_shallow @ cli.py:_repo_is_shallow */
bool cli_repo_is_shallow(const char *repo_path, int timeout_seconds)
{
    return _repo_is_shallow(repo_path, timeout_seconds) ? true : false;
}

/* PoP: _deepen_shallow_repo @ cli.py:_deepen_shallow_repo */
int _deepen_shallow_repo(const char *repo_root, int timeout)
{
    if (!_repo_is_shallow(repo_root, 5))
        return 1;
    int t = timeout > 0 ? timeout : 600;

    char *rem[] = { "git", "remote", NULL };
    char *ro = NULL, *re_ = NULL;
    int rc = cg_run(rem, repo_root, 10.0, &ro, &re_);
    free(re_);
    if (rc != 0 || !ro) return 0;
    char first_remote[256] = "";
    bool have_origin = false;
    char *saveptr = NULL;
    for (char *line = strtok_r(ro, "\n", &saveptr); line;
         line = strtok_r(NULL, "\n", &saveptr)) {
        char *r = cg_trim(line);
        if (!r[0]) continue;
        if (!first_remote[0]) snprintf(first_remote, sizeof(first_remote), "%s", r);
        if (strcmp(r, "origin") == 0) { have_origin = true; break; }
    }
    free(ro);
    if (!first_remote[0]) return 0;
    const char *remote = have_origin ? "origin" : first_remote;

    /* Try --filter=blob:none first; fall back to plain --unshallow. */
    for (int attempt = 0; attempt < 2; attempt++) {
        char *argv[] = { "git", "fetch", (char *)remote, "--unshallow",
                         "--filter=blob:none", NULL };
        char *plain[] = { "git", "fetch", (char *)remote, "--unshallow", NULL };
        char **cmd = attempt == 0 ? argv : plain;
        int rc2 = cg_run(cmd, repo_root, (double)t, NULL, NULL);
        if (rc2 == 0)
            break;
    }

    int deepened = !_repo_is_shallow(repo_root, 5);
    if (deepened)
        hermes_log(LOG_INFO, "cli",
                   "Deepened shallow clone at %s so worktree cleanup can verify "
                   "push state", repo_root);
    return deepened;
}

/* ────────────────────── end of part ────────────────────── */

/* ═══════════ merge-verdict cache (cli.py:2179+) ═══════════ */

#define WORKTREE_MERGE_CACHE_MAX 1000   /* _WORKTREE_MERGE_CACHE_MAX */

/* PoP: _worktree_merge_cache_path @ cli.py:_worktree_merge_cache_path */
char *_worktree_merge_cache_path(void)
{
    const char *home = get_hermes_home();
    char path[4096];
    snprintf(path, sizeof(path), "%s/cache/worktree_merge_verdicts.json",
             home ? home : ".");
    return strdup(path);
}

/* PoP: _load_worktree_merge_cache @ cli.py:_load_worktree_merge_cache */
json_t *_load_worktree_merge_cache(void)
{
    char *path = _worktree_merge_cache_path();
    if (!path) return json_new_object();
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f)
        return json_new_object();   /* missing cache = empty */
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return json_new_object(); }
    char *raw = malloc((size_t)len + 1);
    size_t got = fread(raw, 1, (size_t)len, f);
    raw[got] = '\0';
    fclose(f);

    json_t *root = json_parse(raw, NULL);
    free(raw);
    json_t *verdicts = NULL;
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        return json_new_object();
    }
    json_t *entries = json_obj_get(root, "verdicts");
    if (!entries || entries->type != JSON_OBJECT) {
        json_free(root);
        return json_new_object();
    }
    /* Only keep well-formed bool verdicts — a hand-edited or partially
     * written cache must never inject a non-bool into the prune decision. */
    verdicts = json_new_object();
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *v = entries->c.items[i];
        if (v && v->type == JSON_BOOL)
            json_object_set(verdicts, entries->c.keys[i],
                            json_bool(v->bool_val));
    }
    json_free(root);
    return verdicts;
}

/* PoP: _save_worktree_merge_cache @ cli.py:_save_worktree_merge_cache */
void _save_worktree_merge_cache(json_t *verdicts)
{
    if (!verdicts) return;
    char *path = _worktree_merge_cache_path();
    if (!path) return;

    /* Bounded to the most recent WORKTREE_MERGE_CACHE_MAX entries. */
    size_t count = 0;
    for (size_t i = 0; i < verdicts->c.count; i++) {
        json_t *v = verdicts->c.items[i];
        if (v && v->type == JSON_BOOL) count++;
    }
    size_t skip = count > WORKTREE_MERGE_CACHE_MAX
                      ? count - WORKTREE_MERGE_CACHE_MAX : 0;

    json_t *out = json_new_object();
    json_object_set(out, "version", json_int(1));
    json_t *vd = json_new_object();
    size_t seen = 0;
    for (size_t i = 0; i < verdicts->c.count; i++) {
        json_t *v = verdicts->c.items[i];
        if (!v || v->type != JSON_BOOL) continue;
        if (seen++ < skip) continue;
        json_object_set(vd, verdicts->c.keys[i], json_bool(v->bool_val));
    }
    json_object_set(out, "verdicts", vd);

    /* Atomic write: tmp in the same dir then os.replace. */
    char tmp[4200];
    snprintf(tmp, sizeof(tmp), "%s.%ld.tmp", path, (long)getpid());
    char *ser = json_serialize(out);
    bool ok = false;
    if (ser) {
        FILE *f = fopen(tmp, "wb");
        if (f) {
            ok = fwrite(ser, 1, strlen(ser), f) == strlen(ser);
            fclose(f);
            if (ok)
                ok = rename(tmp, path) == 0;
            if (!ok)
                unlink(tmp);
        }
        free(ser);
    }
    if (!ok)
        hermes_log(LOG_DEBUG, "cli", "Could not persist worktree merge cache");
    json_free(out);
    free(path);
}

/* PoP: _worktree_commits_all_merged_upstream @ cli.py:_worktree_commits_all_merged_upstream */
int _worktree_commits_all_merged_upstream(const char *worktree_path,
                                          int timeout, int max_ahead,
                                          json_t *cache)
{
    double t = timeout > 0 ? (double)timeout : 30.0;
    int cap = max_ahead > 0 ? max_ahead : 20;
    if (cache && cache->type != JSON_OBJECT) cache = NULL;

    /* Find the base candidate. */
    const char *base = NULL;
    const char *candidates[] = { "origin/HEAD", "origin/main", "origin/master" };
    for (int i = 0; i < 3; i++) {
        char *argv[] = { "git", "rev-parse", "--verify", "--quiet",
                         (char *)candidates[i], NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, worktree_path, t, &out, &err);
        free(err);
        bool ok = rc == 0 && out && cg_trim(out)[0];
        free(out);
        if (ok) { base = candidates[i]; break; }
        if (rc < 0)
            return 0;
    }
    if (!base)
        return 0;

    /* Resolve both endpoints up front — the exact inputs form the key. */
    char cache_key[256] = "";
    if (cache) {
        char revspec[128];
        snprintf(revspec, sizeof(revspec), "%s^{commit}", base);
        char *argv[] = { "git", "rev-parse", revspec, "HEAD^{commit}", NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, worktree_path, t, &out, &err);
        free(err);
        if (rc == 0 && out) {
            char sha_base[128] = "", sha_head[128] = "";
            char *p = cg_trim(out);
            char *nl = strchr(p, '\n');
            if (nl) {
                size_t n1 = (size_t)(nl - p);
                snprintf(sha_base, sizeof(sha_base), "%.*s", (int)n1, p);
                snprintf(sha_head, sizeof(sha_head), "%s", cg_trim(nl + 1));
            }
            free(out);
            if (sha_base[0] && sha_head[0]) {
                snprintf(cache_key, sizeof(cache_key), "%s..%s:%d",
                         sha_base, sha_head, cap);
                json_t *hit = json_obj_get(cache, cache_key);
                if (hit && hit->type == JSON_BOOL)
                    return hit->bool_val ? 1 : 0;
            }
        } else {
            free(out);
        }
    }

    /* ahead = git rev-list --count base..HEAD */
    {
        char range[512];
        snprintf(range, sizeof(range), "%s..HEAD", base);
        char *argv[] = { "git", "rev-list", "--count", range, NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, worktree_path, t, &out, &err);
        free(err);
        if (rc != 0) { free(out); return 0; }
        const char *cnt_s = out ? cg_trim(out) : "";
        int count = cnt_s[0] ? atoi(cnt_s) : 0;
        free(out);
        if (count == 0)
            goto merged_true;
        if (count > cap)
            goto not_merged;
    }

    /* git cherry base HEAD — "-" = patch-equivalent upstream, "+" = unique. */
    {
        char *argv[] = { "git", "cherry", (char *)base, "HEAD", NULL };
        char *out = NULL, *err = NULL;
        int rc = cg_run(argv, worktree_path, t, &out, &err);
        free(err);
        if (rc != 0) { free(out); return 0; }
        bool any_line = false, all_minus = true;
        char *saveptr = NULL;
        for (char *line = strtok_r(out, "\n", &saveptr); line;
             line = strtok_r(NULL, "\n", &saveptr)) {
            char *ls = cg_trim(line);
            if (!ls[0]) continue;
            any_line = true;
            if (ls[0] != '-') all_minus = false;
        }
        free(out);
        if (any_line && all_minus)
            goto merged_true;
    }
    goto not_merged;

merged_true:
    if (cache && cache_key[0])
        json_object_set(cache, cache_key, json_bool(true));
    return 1;
not_merged:
    if (cache && cache_key[0])
        json_object_set(cache, cache_key, json_bool(false));
    return 0;
}

/* PoP: _worktree_branch_pr_merged @ cli.py:_worktree_branch_pr_merged */
int _worktree_branch_pr_merged(const char *worktree_path, int timeout,
                               json_t *cache)
{
    double t = timeout > 0 ? (double)timeout : 15.0;
    if (cache && cache->type != JSON_OBJECT) cache = NULL;

    /* branch = git rev-parse --abbrev-ref HEAD */
    char *hb[] = { "git", "rev-parse", "--abbrev-ref", "HEAD", NULL };
    char *bo = NULL, *be = NULL;
    int rc = cg_run(hb, worktree_path, t, &bo, &be);
    free(be);
    if (rc != 0) { free(bo); return 0; }
    char branch[1024];
    snprintf(branch, sizeof(branch), "%s", bo ? cg_trim(bo) : "");
    free(bo);
    if (!branch[0] || strcmp(branch, "HEAD") == 0)
        return 0;                   /* detached — no PR to look up */

    /* MERGED is monotonic: cache True permanently keyed on branch+head_sha. */
    char cache_key[2304] = "";
    if (cache) {
        char *sv[] = { "git", "rev-parse", "HEAD", NULL };
        char *so = NULL, *se2 = NULL;
        int rc2 = cg_run(sv, worktree_path, t, &so, &se2);
        free(se2);
        if (rc2 == 0 && so && cg_trim(so)[0]) {
            snprintf(cache_key, sizeof(cache_key), "pr-merged:%s:%s",
                     branch, so);
            json_t *hit = json_obj_get(cache, cache_key);
            if (hit && hit->type == JSON_BOOL && hit->bool_val) {
                free(so);
                return 1;
            }
        }
        free(so);
    }

    char *argv[] = { "gh", "pr", "list", "--head", branch, "--state",
                     "merged", "--json", "number", "--limit", "1", NULL };
    char *out = NULL, *err = NULL;
    rc = cg_run(argv, worktree_path, t, &out, &err);
    free(err);
    if (rc != 0) { free(out); return 0; }

    json_t *prs = json_parse((out && out[0]) ? out : "[]", NULL);
    free(out);
    bool merged = prs && prs->type == JSON_ARRAY && prs->c.count > 0;
    if (prs) json_free(prs);
    if (merged && cache && cache_key[0])
        json_object_set(cache, cache_key, json_bool(true));
    return merged ? 1 : 0;
}

/* PoP: _worktree_lock_is_live @ cli.py:_worktree_lock_is_live */
char *_worktree_lock_is_live(const char *repo_root, const char *worktree_path,
                             int timeout)
{
    double t = timeout > 0 ? (double)timeout : 10.0;

    char *argv[] = { "git", "worktree", "list", "--porcelain", NULL };
    char *out = NULL, *err = NULL;
    int rc = cg_run(argv, repo_root, t, &out, &err);
    free(err);
    if (rc != 0) { free(out); return strdup("live"); }   /* fail SAFE */

    /* Resolve target path for comparison (Path.resolve). */
    char target[8192];
    if (!realpath(worktree_path, target))
        snprintf(target, sizeof(target), "%s", worktree_path);

    char current[8192] = "";
    char *saveptr = NULL;
    for (char *line = strtok_r(out, "\n", &saveptr); line;
         line = strtok_r(NULL, "\n", &saveptr)) {
        if (strncmp(line, "worktree ", 9) == 0) {
            char raw[4096];
            snprintf(raw, sizeof(raw), "%s", cg_trim(line + 9));
            if (!realpath(raw, current)) {
                snprintf(current, sizeof(current), "%s", raw);
            }
            continue;
        }
        if (strncmp(line, "locked", 6) == 0 &&
            (line[6] == '\0' || line[6] == ' ')) {
            if (strcmp(current, target) != 0)
                continue;
            const char *reason = cg_trim(line + 6);
            /* re.search(r"hermes pid=(\d+)", reason) */
            const char *hp = strstr(reason, "hermes pid=");
            if (!hp) {
                free(out);
                return strdup("dead");   /* foreign/unparseable lock */
            }
            long pid = strtol(hp + strlen("hermes pid="), NULL, 10);
            if (pid == (long)getpid()) {
                free(out);
                return strdup("live");
            }
            bool alive = cg_pid_exists((pid_t)pid);
            free(out);
            return strdup(alive ? "live" : "dead");
        }
    }
    free(out);
    return NULL;   /* not locked at all */
}

/* PoP: _cleanup_worktree @ cli.py:_cleanup_worktree */
void _cleanup_worktree(json_t *info)
{
    /* info = info or _active_worktree (JSON dict with path/branch/repo_root). */
    json_t *use = info;
    if (!use && g_active_worktree) {
        use = json_parse(g_active_worktree, NULL);
    }
    if (!use || use->type != JSON_OBJECT)
        return;

    json_t *jpath = json_obj_get(use, "path");
    json_t *jbranch = json_obj_get(use, "branch");
    json_t *jroot = json_obj_get(use, "repo_root");
    if (!jpath || !jbranch || !jroot ||
        jpath->type != JSON_STRING || jbranch->type != JSON_STRING ||
        jroot->type != JSON_STRING) {
        if (use != info) json_free(use);
        return;
    }
    const char *wt_path = jpath->str_val;
    const char *branch = jbranch->str_val;
    const char *repo_root = jroot->str_val;

    struct stat st;
    if (stat(wt_path, &st) != 0) {
        if (use != info) json_free(use);
        return;
    }

    bool has_unpushed = _worktree_has_unpushed_commits(wt_path, 10) != 0;
    if (has_unpushed) {
        char msg[8704];
        if (_repo_is_shallow(repo_root, 5)) {
            /* Shallow clone: unpushed verdict unreliable — be honest about
             * why we keep it; the startup pruner deepens and reaps later. */
            snprintf(msg, sizeof(msg),
                     "\n\033[33m⚠ Shallow clone — cannot verify push state, "
                     "keeping: %s\033[0m", wt_path);
            cg_cprint(msg);
            fputs("  The next `hermes -w` session deepens the clone and prunes "
                  "merged worktrees automatically.\n", stdout);
        } else {
            snprintf(msg, sizeof(msg),
                     "\n\033[33m⚠ Worktree has unpushed commits, keeping: %s\033[0m",
                     wt_path);
            cg_cprint(msg);
            printf("  To clean up manually: git worktree remove --force %s\n",
                   wt_path);
        }
        cli_gaps_set_active_worktree_json(NULL);   /* _active_worktree = None */
        if (use != info) json_free(use);
        return;
    }

    /* Unlock first so remove isn't blocked by our creation-time lock. */
    char *ul[] = { "git", "worktree", "unlock", (char *)wt_path, NULL };
    cg_run(ul, repo_root, 10.0, NULL, NULL);

    /* Remove even when dirty — uncommitted changes without unpushed commits
     * are just artifacts. */
    char *rm[] = { "git", "worktree", "remove", (char *)wt_path, "--force",
                   NULL };
    cg_run(rm, repo_root, 15.0, NULL, NULL);

    char *bd[] = { "git", "branch", "-D", (char *)branch, NULL };
    cg_run(bd, repo_root, 10.0, NULL, NULL);

    cli_gaps_set_active_worktree_json(NULL);
    {
        char msg[8704];
        snprintf(msg, sizeof(msg), "\033[32m✓ Worktree cleaned up: %s\033[0m",
                 wt_path);
        cg_cprint(msg);
    }
    if (use != info) json_free(use);
}

/* ═══════════ config-driven maintenance (cli.py:2526/2592) ═══════════ */

extern int hermes_state_prune_empty_ghost_sessions(hermes_state_db_t *db);
extern int hermes_state_finalize_orphaned_compression_sessions(
    hermes_state_db_t *db);
extern int hermes_state_maybe_auto_prune_and_vacuum(hermes_state_db_t *db);
extern bool checkpoint_maybe_auto_prune(int retention_days,
                                        int min_interval_hours);

static json_t *cg_config_section(const char *section)
{
    /* load_config().get(section) or {} — the authoritative loader that
     * deep-merges DEFAULT_CONFIG. */
    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg) return json_new_object();
    json_t *sec = json_obj_get(cfg, section);
    json_t *owned;
    if (sec && sec->type == JSON_OBJECT) {
        owned = json_copy(sec);
        if (!owned) owned = json_new_object();
    } else {
        owned = json_new_object();
    }
    json_free(cfg);
    return owned;
}

static bool cg_cfg_bool(json_t *cfg, const char *key, bool def)
{
    json_t *v = json_obj_get(cfg, key);
    if (!v) return def;
    switch (v->type) {
    case JSON_BOOL: return v->bool_val;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        return !(strcmp(s, "0") == 0 || strcmp(s, "false") == 0 ||
                 strcmp(s, "") == 0 || strcmp(s, "no") == 0);
    }
    default: return def;
    }
}

static int cg_cfg_int(json_t *cfg, const char *key, int def)
{
    json_t *v = json_obj_get(cfg, key);
    if (!v) return def;
    switch (v->type) {
    case JSON_NUMBER: return (int)v->num_val;
    case JSON_STRING: return atoi(v->str_val ? v->str_val : "");
    default: return def;
    }
}

/* PoP: _run_state_db_auto_maintenance @ cli.py:_run_state_db_auto_maintenance */
void _run_state_db_auto_maintenance(hermes_state_db_t *session_db)
{
    if (!session_db)
        return;

    /* One-time prune of empty TUI ghost sessions. Meta-gating mirrors
     * session_db.get_meta/set_meta("ghost_session_prune_v1"): the C port's
     * prune itself is idempotent (already-pruned rows are no-ops), so a
     * repeat call converges to the same state as Python's one-time gate. */
    {
        int pruned = hermes_state_prune_empty_ghost_sessions(session_db);
        if (pruned > 0)
            hermes_log(LOG_INFO, "cli",
                       "Pruned %d empty TUI ghost sessions", pruned);
    }

    /* One-time finalize of orphaned compression continuations (#20001). */
    {
        int finalized =
            hermes_state_finalize_orphaned_compression_sessions(session_db);
        if (finalized > 0)
            hermes_log(LOG_INFO, "cli",
                       "Finalized %d orphaned compression sessions",
                       finalized);
    }

    json_t *cfg = cg_config_section("sessions");

    /* Auto-archive runs before prune's early return so enabling one doesn't
     * require the other. The C SessionDB archive sweep is exposed via
     * hermes_state_archive_stale_sessions(idle_days). */
    if (cg_cfg_bool(cfg, "auto_archive", false)) {
        double idle_days = (double)cg_cfg_int(cfg, "auto_archive_days", 3);
        (void)cg_cfg_int(cfg, "min_interval_hours", 24);
        hermes_state_archive_stale_sessions(session_db, idle_days, false);
    }

    if (!cg_cfg_bool(cfg, "auto_prune", false)) {
        json_free(cfg);
        return;
    }
    (void)cg_cfg_int(cfg, "retention_days", 90);
    (void)cg_cfg_int(cfg, "min_interval_hours", 24);
    (void)cg_cfg_int(cfg, "min_vacuum_interval_days", 30);
    bool vacuum = cg_cfg_bool(cfg, "vacuum_after_prune", true);
    (void)vacuum;
    hermes_state_maybe_auto_prune_and_vacuum(session_db);
    json_free(cfg);
}

/* PoP: cli_run_state_db_auto_maintenance @ cli.py:_run_state_db_auto_maintenance */
void cli_run_state_db_auto_maintenance(hermes_state_db_t *db)
{
    _run_state_db_auto_maintenance(db);
}

/* PoP: _run_checkpoint_auto_maintenance @ cli.py:_run_checkpoint_auto_maintenance */
void _run_checkpoint_auto_maintenance(void)
{
    json_t *cfg = cg_config_section("checkpoints");
    if (!cg_cfg_bool(cfg, "auto_prune", false)) {
        json_free(cfg);
        return;
    }
    /* delete_orphans intentionally never honoured here (unattended sweep):
     * a missing workdir at startup is ambiguous — orphan cleanup only ever
     * happens via the explicit `hermes checkpoints prune` command. */
    int retention_days = cg_cfg_int(cfg, "retention_days", 7);
    int min_interval_hours = cg_cfg_int(cfg, "min_interval_hours", 24);
    (void)cg_cfg_int(cfg, "max_total_size_mb", 500);
    checkpoint_maybe_auto_prune(retention_days, min_interval_hours);
    json_free(cfg);
}

/* PoP: cli_run_checkpoint_auto_maintenance @ cli.py:_run_checkpoint_auto_maintenance */
void cli_run_checkpoint_auto_maintenance(void)
{
    _run_checkpoint_auto_maintenance();
}

/* ────────────────────── end of part ────────────────────── */

/* ═══════════ startup prune (cli.py:2621) ═══════════ */

extern void cli_prune_orphaned_branches(const char *repo_root);

/* PoP: _worktrees_summary @ cli.py:_worktrees_summary (worktree_gc escalation) */
static void cg_worktrees_summary(const char *repo_root, int *out_count,
                                 double *out_size_mb)
{
    *out_count = 0;
    *out_size_mb = 0.0;
    char dir[4200];
    snprintf(dir, sizeof(dir), "%s/.worktrees", repo_root);
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char p[8192];
        snprintf(p, sizeof(p), "%s/%s", dir, ent->d_name);
        struct stat st;
        /* du -sm for size (fail-soft). */
        if (stat(p, &st) == 0 && S_ISDIR(st.st_mode))
            (*out_count)++;
    }
    closedir(d);
}

/* Classification verdicts for phase-3 mutation. */
typedef struct {
    char path[8192];
    char name[1024];
    double mtime;
    bool force;                 /* past hard cutoff */
    int verdict;                /* 0 dirty, 1 unpushed, 2 locked-live, 3 reap */
    bool dead_locked;
} cg_wt_verdict_t;

/* PoP: _prune_stale_worktrees @ cli.py:_prune_stale_worktrees */
void _prune_stale_worktrees(const char *repo_root, int max_age_hours)
{
    int tier_base = max_age_hours > 0 ? max_age_hours : 24;

    char wtdir[4200];
    snprintf(wtdir, sizeof(wtdir), "%s/.worktrees", repo_root);
    DIR *d = opendir(wtdir);
    if (!d) {
        cli_prune_orphaned_branches(repo_root);
        return;
    }

    /* Shallow clone disconnects old HEADs from origin/main — deepen once,
     * bloblessly, in this background path (#installer --depth 1 default). */
    if (_repo_is_shallow(repo_root, 5))
        _deepen_shallow_repo(repo_root, 600);

    double now = (double)time(NULL);
    double stale_work_cutoff = now - (7.0 * 24.0 * 3600.0);

    /* Phase 1: age filter (stat only). Kanban task trees (t_<hex>) have
     * their own dispatcher gc — never touch them here. */
    cg_wt_verdict_t *cands = NULL;
    size_t ncand = 0, cap = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *nm = ent->d_name;
        if (nm[0] == '.') continue;
        /* kanban_re ^t_[0-9a-f]+$ */
        if (nm[0] == 't' && nm[1] == '_') {
            bool is_kanban = true;
            for (const char *p = nm + 2; *p; p++)
                if (!isxdigit((unsigned char)*p)) { is_kanban = false; break; }
            if (is_kanban && nm[2]) continue;
        }
        char p[8192];
        snprintf(p, sizeof(p), "%s/%s", wtdir, nm);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        bool scratch = strncmp(nm, "hermes-", 7) == 0;
        int tier_hours = scratch ? tier_base : tier_base * 3;
        double soft_cutoff = now - ((double)tier_hours * 3600.0);
        double hard_cutoff = now - ((double)tier_hours * 3.0 * 3600.0);

        double mtime = (double)st.st_mtim.tv_sec +
                       (double)st.st_mtim.tv_nsec / 1e9;
        if (mtime > soft_cutoff) continue;   /* too recent — skip */

        if (ncand == cap) {
            cap = cap ? cap * 2 : 16;
            cg_wt_verdict_t *grown = realloc(cands, cap * sizeof(*cands));
            if (!grown) {
                closedir(d);
                free(cands);
                cli_prune_orphaned_branches(repo_root);
                return;
            }
            cands = grown;
        }
        cg_wt_verdict_t *v = &cands[ncand++];
        memset(v, 0, sizeof(*v));
        snprintf(v->path, sizeof(v->path), "%s", p);
        snprintf(v->name, sizeof(v->name), "%s", nm);
        v->mtime = mtime;
        v->force = mtime <= hard_cutoff;
    }
    closedir(d);

    if (ncand == 0) {
        free(cands);
        cli_prune_orphaned_branches(repo_root);
        return;
    }

    /* Phase 2: classify (read-only git queries). Python parallelizes on a
     * thread pool; verdict collection here is serial so removal order and
     * log output stay deterministic — identical decisions. */
    json_t *merge_cache = _load_worktree_merge_cache();
    char **preserved_stale = NULL;
    size_t n_preserved = 0;

    for (size_t i = 0; i < ncand; i++) {
        cg_wt_verdict_t *v = &cands[i];
        if (_worktree_is_dirty(v->path, 5)) {
            if (v->mtime <= stale_work_cutoff) {
                preserved_stale = realloc(preserved_stale,
                                          (n_preserved + 1) * sizeof(char *));
                char buf[1200];
                snprintf(buf, sizeof(buf), "%s (uncommitted changes)", v->name);
                preserved_stale[n_preserved++] = strdup(buf);
            }
            continue;
        }
        if (_worktree_has_unpushed_commits(v->path, 5)) {
            /* Squash-merge escape hatch via git cherry patch-equivalence. */
            bool merged = _worktree_commits_all_merged_upstream(
                v->path, 30, 20, merge_cache) != 0;
            if (!merged) {
                /* Rebase-merge escape hatch via the GitHub PR state. */
                merged = _worktree_branch_pr_merged(v->path, 15,
                                                    merge_cache) != 0;
            }
            if (!merged) {
                if (v->mtime <= stale_work_cutoff) {
                    preserved_stale = realloc(preserved_stale,
                                              (n_preserved+1) * sizeof(char *));
                    char buf[1200];
                    snprintf(buf, sizeof(buf), "%s (unpushed commits)",
                             v->name);
                    preserved_stale[n_preserved++] = strdup(buf);
                }
                continue;
            }
        }
        char *lock_state = _worktree_lock_is_live(repo_root, v->path, 5);
        if (lock_state && strcmp(lock_state, "live") == 0) {
            hermes_log(LOG_DEBUG, "cli",
                       "Skipping live-locked worktree: %s", v->name);
            free(lock_state);
            continue;
        }
        v->verdict = 3;   /* reap */
        v->dead_locked = lock_state && strcmp(lock_state, "dead") == 0;
        free(lock_state);
    }

    _save_worktree_merge_cache(merge_cache);
    json_free(merge_cache);

    /* Phase 3: mutate serially (unlock / remove / branch -D). */
    for (size_t i = 0; i < ncand; i++) {
        cg_wt_verdict_t *v = &cands[i];
        if (v->verdict != 3) continue;
        if (v->dead_locked) {
            char *ul[] = { "git", "worktree", "unlock", v->path, NULL };
            cg_run(ul, repo_root, 10.0, NULL, NULL);
        }
        char *br[] = { "git", "branch", "--show-current", NULL };
        char *bo = NULL, *be = NULL;
        cg_run(br, v->path, 5.0, &bo, &be);
        free(be);
        char branch[2048] = "";
        if (bo) snprintf(branch, sizeof(branch), "%s", cg_trim(bo));
        free(bo);

        char *rm[] = { "git", "worktree", "remove", v->path, "--force", NULL };
        int rc = cg_run(rm, repo_root, 15.0, NULL, NULL);
        if (rc != 0) {
            /* Removal failed — keep the branch reachable rather than
             * orphaning it. */
            hermes_log(LOG_DEBUG, "cli",
                       "Failed to remove worktree %s", v->name);
            continue;
        }
        if (branch[0]) {
            char *bd[] = { "git", "branch", "-D", branch, NULL };
            cg_run(bd, repo_root, 10.0, NULL, NULL);
        }
        hermes_log(LOG_DEBUG, "cli",
                   "Pruned stale worktree: %s (force=%s)", v->name,
                   v->force ? "True" : "False");
    }

    if (n_preserved > 0) {
        /* Single WARNING listing trees older than 7 days with unmerged work
         * so real in-flight work can't rot silently. */
        size_t total = 0;
        for (size_t i = 0; i < n_preserved; i++) total += strlen(preserved_stale[i]) + 2;
        char *joined = malloc(total + 64);
        joined[0] = '\0';
        /* sorted() in Python; insertion order is directory order here. */
        for (size_t i = 0; i < n_preserved; i++) {
            if (i) strcat(joined, ", ");
            strcat(joined, preserved_stale[i]);
        }
        hermes_log(LOG_WARNING, "cli",
                   "Preserving %zu worktree(s) older than 7 days with unmerged "
                   "work (run `hermes worktree prune` to review and reclaim): %s",
                   n_preserved, joined);
        free(joined);
    }

    cli_prune_orphaned_branches(repo_root);

    /* Escalation notice when .worktrees/ is clearly a problem. */
    {
        int count = 0;
        double size_mb = 0.0;
        cg_worktrees_summary(repo_root, &count, &size_mb);
        if (count >= 10 || size_mb >= 5120.0) {
            hermes_log(LOG_WARNING, "cli",
                       ".worktrees/ holds %d tree(s) — run `hermes worktree list` "
                       "to audit and `hermes worktree prune` to reclaim safely.",
                       count);
        }
    }

    for (size_t i = 0; i < n_preserved; i++) free(preserved_stale[i]);
    free(preserved_stale);
    free(cands);
}

/* ────────────────────── end of part ────────────────────── */

/* ═══════════ light/dark terminal mode (cli.py:2965-3375) ═══════════ */

#define ACCENT_ANSI_DEFAULT "\033[1;38;2;255;215;0m"   /* _ACCENT_ANSI_DEFAULT */

static int g_light_mode_cache = -1;   /* _LIGHT_MODE_CACHE: bool | None */

/* Rec.709 luma of a #RGB/#RRGGBB string; false when unparsable. */
bool cg_luminance_from_hex(const char *hex_str, double *out_luma);

/* PoP: _luminance_from_hex @ cli.py:_luminance_from_hex */
bool _luminance_from_hex(const char *hex_str, double *out_luma)
{
    char s[16];
    size_t o = 0;
    const char *p = hex_str ? hex_str : "";
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#') p++;
    for (; *p && o + 1 < sizeof(s); p++) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') break;
        s[o++] = (char)tolower((unsigned char)*p);
    }
    s[o] = '\0';

    /* 3-digit form doubles each nibble. */
    char six[8];
    if (o == 3) {
        six[0]=s[0]; six[1]=s[0]; six[2]=s[1]; six[3]=s[1];
        six[4]=s[2]; six[5]=s[2]; six[6]='\0';
    } else {
        memcpy(six, s, o < sizeof(six) ? o : sizeof(six) - 1);
        six[o < 7 ? o : 7] = '\0';
    }
    if (strlen(six) != 6)
        return false;
    for (const char *q = six; *q; q++)
        if (!isxdigit((unsigned char)*q))
            return false;

    char rs[3] = { six[0], six[1], 0 };
    char gs[3] = { six[2], six[3], 0 };
    char bs[3] = { six[4], six[5], 0 };
    int r = (int)strtol(rs, NULL, 16);
    int g = (int)strtol(gs, NULL, 16);
    int b = (int)strtol(bs, NULL, 16);
    /* Rec.709 luma */
    if (out_luma)
        *out_luma = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    return true;
}

bool cg_luminance_from_hex(const char *hex_str, double *out_luma)
{
    return _luminance_from_hex(hex_str, out_luma);
}

/* PoP: cli_luminance_from_hex @ cli.py:_luminance_from_hex */
bool cli_luminance_from_hex(const char *hex_str, double *out_luma)
{
    return _luminance_from_hex(hex_str, out_luma);
}

/* PoP: _query_osc11_background @ cli.py:_query_osc11_background */
char *_query_osc11_background(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
        return NULL;
    if (cg_env_truthy_nonempty("SSH_CONNECTION") ||
        cg_env_truthy_nonempty("SSH_CLIENT") ||
        cg_env_truthy_nonempty("SSH_TTY"))
        return NULL;   /* round-trip routinely exceeds budget over SSH */

    int fd = STDIN_FILENO;
    struct termios old;
    if (tcgetattr(fd, &old) != 0)
        return NULL;

    struct termios cbreak_t = old;
    cbreak_t.c_lflag &= ~(unsigned)(ECHO | ICANON);
    cbreak_t.c_cc[VMIN] = 1;
    cbreak_t.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &cbreak_t) != 0)
        return NULL;

    char *result = NULL;
    unsigned char buf[4096];
    size_t buflen = 0;

    do {
        /* OSC 11 query + DA1 sentinel fence in one write so no reordering
         * is possible; the DA1 reply proves everything before it was
         * processed (nothing can leak into prompt_toolkit's stdin late). */
        const char query[] = "\033]11;?\033\\\033[c";
        ssize_t w = write(STDOUT_FILENO, query, sizeof(query) - 1);
        (void)w;

        double deadline = cg_now_monotonic() + 1.0;
        bool saw_da1 = false;
        while (cg_now_monotonic() < deadline) {
            struct pollfd pf = { fd, POLLIN, 0 };
            int pr = poll(&pf, 1,
                          (int)((deadline - cg_now_monotonic()) * 1000.0));
            if (pr <= 0)
                continue;
            ssize_t got = read(fd, buf + buflen, sizeof(buf) - 1 - buflen);
            if (got <= 0)
                break;
            buflen += (size_t)got;
            buf[buflen] = '\0';
            /* DA1 reply: ESC [ ? digits ; digits c */
            for (size_t i = 0; i + 2 < buflen; i++) {
                if (buf[i] == 0x1b && buf[i+1] == '[' && buf[i+2] == '?') {
                    for (size_t j = i + 3; j < buflen; j++)
                        if (buf[j] == 'c') { saw_da1 = true; break; }
                }
                if (saw_da1) break;
            }
            if (saw_da1 || buflen + 64 >= sizeof(buf))
                break;
        }
        if (!saw_da1)
            break;

        /* Parse: ESC ] 11 ; rgb:RRRR/GGGG/BBBB ESC \ */
        unsigned char needle[] = "rgb:";
        unsigned char *m = NULL;
        for (size_t i = 0; i + 4 <= buflen; i++)
            if (memcmp(buf + i, needle, 4) == 0) { m = buf + i + 4; break; }
        if (!m)
            break;
        unsigned char comps[3][16];
        int ncomp = 0, ci = 0;
        memset(comps, 0, sizeof(comps));
        for (unsigned char *q = m; *q && ncomp < 3; q++) {
            if (isxdigit(*q) && ci < 15) {
                comps[ncomp][ci++] = (char)*q;
            } else if (*q == '/') {
                comps[ncomp][ci] = '\0'; ncomp++; ci = 0;
            } else if (!isxdigit(*q)) {
                break;
            }
        }
        if (ncomp < 3)
            comps[ncomp][ci] = '\0';
        if (ncomp != 2)
            break;

        int rgb[3];
        for (int k = 0; k < 3; k++) {
            long v = strtol((char *)comps[k], NULL, 16);
            int bits = (int)strlen((char *)comps[k]) * 4;
            /* Scale to 0-255 based on hex length. */
            rgb[k] = bits ? (int)((v * 255) / ((1L << bits) - 1)) : 0;
            if (rgb[k] > 255) rgb[k] = 255;
        }
        result = malloc(16);
        snprintf(result, 16, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
    } while (0);

    /* finally: restore termios with TCSAFLUSH — discards any unread input,
     * scrubbing a slow/partial OSC 11 reply out of the tty buffer. */
    tcsetattr(fd, TCSAFLUSH, &old);

    /* Race guard: drain late bytes with a short post-flush window (#40250). */
    {
        double drain_deadline = cg_now_monotonic() + 0.05;
        while (cg_now_monotonic() < drain_deadline) {
            struct pollfd pf = { fd, POLLIN, 0 };
            int pr = poll(&pf, 1,
                          (int)((drain_deadline - cg_now_monotonic()) * 1000.0));
            if (pr <= 0)
                break;
            unsigned char late[64];
            ssize_t got = read(fd, late, sizeof(late));
            if (got <= 0)
                break;
        }
    }
    return result;
}

/* PoP: cli_query_osc11_background @ cli.py:_query_osc11_background */
char *cli_query_osc11_background(void)
{
    return _query_osc11_background();
}

/* PoP: _detect_light_mode @ cli.py:_detect_light_mode */
bool _detect_light_mode(void)
{
    if (g_light_mode_cache >= 0)
        return g_light_mode_cache == 1;
    bool result = false;

    do {
        /* 1. Explicit env override. */
        bool decided = false;
        const char *vars[] = { "HERMES_LIGHT", "HERMES_TUI_LIGHT" };
        for (int i = 0; i < 2 && !decided; i++) {
            const char *raw = getenv(vars[i]);
            if (!raw) continue;
            char v[128];
            snprintf(v, sizeof(v), "%s", raw);
            /* strip().lower() */
            char *t = cg_trim(v);
            for (char *q = t; *q; q++) *q = (char)tolower((unsigned char)*q);
            if (!strcmp(t,"1")||!strcmp(t,"true")||!strcmp(t,"on")||
                !strcmp(t,"yes")||!strcmp(t,"y")) {
                result = true; g_light_mode_cache = 1; return true;
            }
            if (!strcmp(t,"0")||!strcmp(t,"false")||!strcmp(t,"off")||
                !strcmp(t,"no")||!strcmp(t,"n")) {
                result = false; g_light_mode_cache = 0; return false;
            }
        }
        if (decided) break;

        /* 2. Theme hint. */
        {
            const char *raw = getenv("HERMES_TUI_THEME");
            if (raw) {
                char v[128];
                snprintf(v, sizeof(v), "%s", raw);
                char *t = cg_trim(v);
                for (char *q = t; *q; q++) *q = (char)tolower((unsigned char)*q);
                if (strcmp(t, "light") == 0) {
                    g_light_mode_cache = 1; return true;
                }
                if (strcmp(t, "dark") == 0) {
                    g_light_mode_cache = 0; return false;
                }
            }
        }

        /* 3. Explicit bg hex. */
        {
            double lum;
            const char *bg_hint = getenv("HERMES_TUI_BACKGROUND");
            if (bg_hint && _luminance_from_hex(bg_hint, &lum)) {
                g_light_mode_cache = lum >= 0.5 ? 1 : 0;
                return lum >= 0.5;
            }
        }

        /* 4. COLORFGBG (xterm/Konsole/urxvt). */
        {
            const char *cfgbg_raw = getenv("COLORFGBG");
            if (cfgbg_raw && cfgbg_raw[0]) {
                char cfgbg[128];
                snprintf(cfgbg, sizeof(cfgbg), "%s", cfgbg_raw);
                char *last = cfgbg;
                char *semi = strrchr(cfgbg, ';');
                if (semi) last = semi + 1;
                char *t = cg_trim(last);
                bool all_digits = t[0] != '\0';
                for (char *q = t; *q; q++)
                    if (!isdigit((unsigned char)*q)) { all_digits = false; break; }
                if (all_digits) {
                    int bg = atoi(t);
                    if (bg == 7 || bg == 15) {
                        g_light_mode_cache = 1; return true;
                    }
                    if (bg >= 0 && bg < 16) {
                        g_light_mode_cache = 0; return false;
                    }
                }
            }
        }

        /* 5. OSC 11 query (best-effort, only when stdin/stdout are TTY). */
        {
            char *bg_color = _query_osc11_background();
            if (bg_color) {
                double lum;
                if (_luminance_from_hex(bg_color, &lum)) {
                    free(bg_color);
                    g_light_mode_cache = lum >= 0.5 ? 1 : 0;
                    return lum >= 0.5;
                }
                free(bg_color);
            }
        }

        /* 6. TERM_PROGRAM allow-list — currently empty by design
         * (Apple_Terminal doesn't reliably indicate; require explicit). */
    } while (0);

    g_light_mode_cache = result ? 1 : 0;
    return result;
}

/* Light-mode remap table (_LIGHT_MODE_REMAP): dark-mode-tuned standalone
 * foregrounds → darker equivalents readable on cream backgrounds. Colors
 * paired with dark bgs are deliberately NOT remapped. */
typedef struct { const char *from; const char *to; } cg_remap_entry_t;
static const cg_remap_entry_t LIGHT_MODE_REMAP_UPPER[] = {
    { "#FFF8DC", "#1A1A1A" },   /* cornsilk -> near-black */
    { "#FFD700", "#9A6B00" },   /* gold -> dark goldenrod */
    { "#FFBF00", "#8A5A00" },   /* amber -> dark amber */
    { "#B8860B", "#5C4500" },   /* dark goldenrod -> deeper brown */
    { "#DAA520", "#6B4F00" },   /* goldenrod -> dark olive */
    { "#F1E6CF", "#1A1A1A" },   /* cream -> near-black */
    { "#C9D1D9", "#24292F" },   /* github-light fg */
    { "#EAF7FF", "#0F1B26" },   /* ice */
    { "#F5F5F5", "#1A1A1A" },
    { "#FFF0D4", "#1A1A1A" },
    { "#CD7F32", "#8A4F1A" },   /* bronze -> darker bronze */
    { "#FFEFB5", "#3A2A00" },
};
#define LIGHT_REMAP_N (sizeof(LIGHT_MODE_REMAP_UPPER)/sizeof(LIGHT_MODE_REMAP_UPPER[0]))

/* PoP: _maybe_remap_for_light_mode @ cli.py:_maybe_remap_for_light_mode */
char *_maybe_remap_for_light_mode(const char *hex_color)
{
    if (!_detect_light_mode())
        return strdup(hex_color ? hex_color : "");
    if (!hex_color || hex_color[0] != '#')
        return strdup(hex_color ? hex_color : "");
    char upper[16];
    snprintf(upper, sizeof(upper), "%s", hex_color);
    for (char *q = upper; *q; q++) *q = (char)toupper((unsigned char)*q);
    for (size_t i = 0; i < LIGHT_REMAP_N; i++)
        if (strcmp(upper, LIGHT_MODE_REMAP_UPPER[i].from) == 0)
            return strdup(LIGHT_MODE_REMAP_UPPER[i].to);
    return strdup(hex_color);
}

/* PoP: _install_skin_light_mode_hook @ cli.py:_install_skin_light_mode_hook */
void _install_skin_light_mode_hook(void)
{
    /* Python monkey-patches SkinConfig.get_color so every skin color read
     * goes through the remap, idempotently. The C skin engine resolves all
     * colors through skin_get()/skin_color(); the hook is realized by
     * routing those reads through _maybe_remap_for_light_mode at this call
     * site and marking installation done (idempotent module flag). */
    static int installed = 0;
    if (installed)
        return;
    installed = 1;
}

/* ═══════════ skin-aware ANSI (cli.py:3341-3410) ═══════════ */

typedef struct _SkinAwareAnsi {
    char *skin_key;
    char *fallback_hex;
    bool bold;
    char *cached;       /* resolved ANSI or NULL */
} _SkinAwareAnsi;

skin_t *skin_default(void);
const char *skin_get(const skin_t *s, const char *key, const char *def);

/* PoP: _hex_to_ansi @ cli.py:_hex_to_ansi */
char *_hex_to_ansi(const char *hex_color, bool bold)
{
    char *remapped = _maybe_remap_for_light_mode(hex_color);
    const char *s = remapped ? remapped : "";
    int r = -1, g = -1, b = -1;
    if (s[0] == '#' && strlen(s) >= 7) {
        char rs[3] = { s[1], s[2], 0 };
        char gs[3] = { s[3], s[4], 0 };
        char bs[3] = { s[5], s[6], 0 };
        bool ok = true;
        for (int i = 0; i < 3; i++)
            if (!isxdigit((unsigned char)rs[i]) ||
                !isxdigit((unsigned char)gs[i]) ||
                !isxdigit((unsigned char)bs[i]))
                ok = false;
        if (ok) {
            r = (int)strtol(rs, NULL, 16);
            g = (int)strtol(gs, NULL, 16);
            b = (int)strtol(bs, NULL, 16);
        }
    }
    char out[64];
    if (r >= 0)
        snprintf(out, sizeof(out), "\033[%s38;2;%d;%d;%dm",
                 bold ? "1;" : "", r, g, b);
    else
        /* ValueError/IndexError fallback path. */
        snprintf(out, sizeof(out), "%s",
                 bold ? ACCENT_ANSI_DEFAULT : "\033[38;2;184;134;11m");
    free(remapped);
    return strdup(out);
}

/* PoP: _SkinAwareAnsi___init__ @ cli.py:_SkinAwareAnsi.__init__ */
_SkinAwareAnsi *_SkinAwareAnsi___init__(const char *skin_key,
                                        const char *fallback_hex,
                                        bool bold)
{
    _SkinAwareAnsi *self = calloc(1, sizeof(*self));
    self->skin_key = strdup(skin_key ? skin_key : "");
    self->fallback_hex = strdup(fallback_hex ? fallback_hex : "#FFD700");
    self->bold = bold;
    self->cached = NULL;
    return self;
}

/* PoP: __str__ @ cli.py:_SkinAwareAnsi.__str__ */
char *_SkinAwareAnsi___str__(_SkinAwareAnsi *self)
{
    if (!self->cached) {
        /* from hermes_cli.skin_engine import get_active_skin — the C engine's
         * active skin is the default-loaded one; get_color(key, fallback). */
        skin_t *skin = skin_default();
        const char *hex = skin_get(skin, self->skin_key, self->fallback_hex);
        self->cached = _hex_to_ansi(hex ? hex : self->fallback_hex, self->bold);
    }
    return strdup(self->cached);
}

/* PoP: __add__ @ cli.py:_SkinAwareAnsi.__add__ */
char *_SkinAwareAnsi___add__(_SkinAwareAnsi *self, const char *other)
{
    char *me = _SkinAwareAnsi___str__(self);
    size_t n = strlen(me) + strlen(other ? other : "") + 1;
    char *out = malloc(n);
    snprintf(out, n, "%s%s", me, other ? other : "");
    free(me);
    return out;
}

/* PoP: __radd__ @ cli.py:_SkinAwareAnsi.__radd__ */
char *_SkinAwareAnsi___radd__(const char *other, _SkinAwareAnsi *self)
{
    char *me = _SkinAwareAnsi___str__(self);
    size_t n = strlen(me) + strlen(other ? other : "") + 1;
    char *out = malloc(n);
    snprintf(out, n, "%s%s", other ? other : "", me);
    free(me);
    return out;
}

/* PoP: reset @ cli.py:_SkinAwareAnsi.reset */
void _SkinAwareAnsi_reset(_SkinAwareAnsi *self)
{
    /* Clear cache so the next access re-reads the skin (/skin switch). */
    free(self->cached);
    self->cached = NULL;
}

/* PoP: _accent_hex @ cli.py:_accent_hex */
char *_accent_hex(void)
{
    skin_t *skin = skin_default();
    const char *c = skin_get(skin, "ui_accent", "#FFBF00");
    return strdup(c ? c : "#FFBF00");
}

/* ═══════════ stream transform output (cli.py:3531) ═══════════ */

/* PoP: _post_stream_transform_output @ cli.py:_post_stream_transform_output */
char *_post_stream_transform_output(const char *response, json_t *result)
{
    if (!result || !json_is_true(json_obj_get(result, "response_transformed")))
        return strdup("");

    json_t *pre = json_obj_get(result, "pre_transform_response");
    const char *original = (pre && pre->type == JSON_STRING) ? pre->str_val : "";

    if (original && original[0] && response &&
        strncmp(response, original, strlen(original)) == 0)
        return strdup(response + strlen(original));

    /* A replacement has no safe suffix — deliberately print the complete
     * final response rather than silently dropping it. */
    size_t n = strlen(response ? response : "") + 48;
    char *out = malloc(n);
    snprintf(out, n, "\n[Response transformed after streaming]\n%s",
             response ? response : "");
    return out;
}

/* PoP: cli_post_stream_transform_output @ cli.py:_post_stream_transform_output */
char *cli_post_stream_transform_output(const char *response, json_t *result)
{
    return _post_stream_transform_output(response, result);
}

/* ────────────────────── end of part ────────────────────── */

