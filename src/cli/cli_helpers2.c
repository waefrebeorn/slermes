/*
 * cli_helpers2.c — wave 4 batch A: leaf module-level functions from cli.py.
 * Dependency-ordered leaves-first batch; each function carries a one-line
 * PoP annotation naming its cli.py origin. Subprocess boundaries (git, gh,
 * nice) are real fork/execvp ports of Python subprocess.run.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "cli_helpers2.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "port_config_py_helpers.h"
#include "terminal_env_registry.h"
#include "port_browser_tool.h"
#include "process_registry.h"
#include "hermes_hooks.h"
#include "hermes_outbound_webhooks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ── shared subprocess helper (Python subprocess.run equivalent) ─────────
 * Runs argv[0] via execvp in cwd; captures stdout; enforces timeout (s)
 * with SIGKILL → rc 124 like the sibling wgc_run_git pattern. */
static int h2_run(char *const argv[], const char *cwd, int timeout, char **out)
{
    if (out) *out = NULL;
    int out_pipe[2];
    if (pipe(out_pipe) != 0) return 127;
    pid_t pid = fork();
    if (pid < 0) { close(out_pipe[0]); close(out_pipe[1]); return 127; }
    if (pid == 0) {
        if (cwd && chdir(cwd) != 0) _exit(127);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO); /* Python capture_output=True */
        close(out_pipe[0]); close(out_pipe[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(out_pipe[1]);
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { close(out_pipe[0]); return 127; }
    for (;;) {
        if (len + 1024 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) break; buf = nb; }
        ssize_t n = read(out_pipe[0], buf + len, cap - len - 1);
        if (n <= 0) break;
        len += (size_t)n;
    }
    close(out_pipe[0]);
    buf[len] = '\0';

    int status = 0, rc = 127;
    time_t deadline = time(NULL) + (timeout > 0 ? timeout : 600);
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            rc = WIFEXITED(status) ? WEXITSTATUS(status)
               : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 127;
            break;
        }
        if (r < 0) { rc = 127; break; }
        if (time(NULL) >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            rc = 124; /* timeout convention */
            break;
        }
        usleep(50000);
    }
    if (rc == 124 || rc == 127) {
        /* Python TimeoutExpired/raise → caller treats as failure; drop output */
        if (out) *out = NULL;
        free(buf);
    } else if (out) {
        *out = buf;
    } else {
        free(buf);
    }
    return rc;
}

static char *h2_trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

/* PoP: _split_model_config_default @ cli.py:_split_model_config_default */
void cli_split_model_config_default(const json_t *raw_default, char **out_model, char **out_provider)
{
    /* Thin wrapper around the shared helper in config.py (backward compat). */
    split_model_config_default(raw_default, out_model, out_provider);
}

/* PoP: _worktree_branch_pr_merged @ cli.py:_worktree_branch_pr_merged */
int cli_worktree_branch_pr_merged(const char *worktree_path, int timeout, json_t *cache)
{
    if (!worktree_path) return 0;
    char *args1[] = {"git", "rev-parse", "--abbrev-ref", "HEAD", NULL};
    char *head_out = NULL;
    int rc = h2_run(args1, worktree_path, timeout, &head_out);
    if (rc != 0) { free(head_out); return 0; }
    char branch[512] = {0};
    if (head_out) {
        char *t = h2_trim(head_out);
        snprintf(branch, sizeof(branch), "%s", t);
    }
    free(head_out);
    if (!branch[0] || strcmp(branch, "HEAD") == 0) return 0; /* detached */

    char cache_key[1200];
    int have_key = 0;
    if (cache && cache->type == JSON_OBJECT) {
        char *sha_out = NULL;
        char *args2[] = {"git", "rev-parse", "HEAD", NULL};
        rc = h2_run(args2, worktree_path, timeout, &sha_out);
        if (rc == 0 && sha_out) {
            char *sha = h2_trim(sha_out);
            if (*sha) {
                snprintf(cache_key, sizeof(cache_key), "pr-merged:%s:%s", branch, sha);
                have_key = 1;
                json_t *hit = json_obj_get(cache, cache_key);
                if (hit && hit->type == JSON_BOOL && hit->bool_val)
                    { free(sha_out); return 1; }   /* MERGED is monotonic */
            }
        }
        free(sha_out);
    }

    char *args3[] = {"gh", "pr", "list", "--head", branch, "--state", "merged",
                     "--json", "number", "--limit", "1", NULL};
    char *gh_out = NULL;
    rc = h2_run(args3, worktree_path, timeout, &gh_out);
    if (rc != 0) { free(gh_out); return 0; }

    int merged = 0;
    json_t *prs = json_parse(gh_out ? gh_out : "[]", NULL);
    if (prs && prs->type == JSON_ARRAY) merged = json_len(prs) > 0;
    if (prs) json_free(prs);
    free(gh_out);

    if (merged && cache && cache->type == JSON_OBJECT && have_key)
        json_set(cache, cache_key, json_bool(1));
    return merged;
}

/* PoP: _cli_multiline_shortcuts_enabled @ cli.py:_cli_multiline_shortcuts_enabled */
int cli_multiline_shortcuts_enabled(const json_t *config)
{
    const json_t *cfg = config;
    json_t *owned = NULL;
    if (!cfg) { owned = config_py_read_raw_config(); cfg = owned; }
    int result = 1;
    do {
        if (!cfg || cfg->type != JSON_OBJECT) break;
        json_t *display = json_obj_get(cfg, "display");
        json_t *value = NULL;
        if (display && display->type == JSON_OBJECT)
            value = json_obj_get(display, "cli_multiline_shortcuts");
        if (!value) break;                       /* default True */
        if (value->type == JSON_BOOL) { result = value->bool_val ? 1 : 0; break; }
        if (value->type == JSON_STRING && value->str_val) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%s", value->str_val);
            char *norm = h2_trim(tmp);
            for (char *p = norm; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (!strcmp(norm, "1") || !strcmp(norm, "true") || !strcmp(norm, "yes") ||
                !strcmp(norm, "on") || !strcmp(norm, "enabled")) { result = 1; break; }
            if (!strcmp(norm, "0") || !strcmp(norm, "false") || !strcmp(norm, "no") ||
                !strcmp(norm, "off") || !strcmp(norm, "disabled")) { result = 0; break; }
        }
        /* numbers / anything else → default True */
    } while (0);
    if (owned) json_free(owned);
    return result;
}

/* Push a normalized, deduplicated skill id onto the parsed list. */
typedef struct {
    char **items;
    size_t n;
    size_t cap;
} h2_skill_list;

static void h2_skill_push(h2_skill_list *lst, const char *part_raw)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", part_raw ? part_raw : "");
    char *norm = h2_trim(tmp);
    if (!*norm) return;
    for (size_t i = 0; i < lst->n; i++)
        if (strcmp(lst->items[i], norm) == 0) return;
    if (lst->n >= lst->cap) {
        lst->cap = lst->cap ? lst->cap * 2 : 8;
        char **np = realloc(lst->items, lst->cap * sizeof(char *));
        if (!np) return;
        lst->items = np;
    }
    lst->items[lst->n++] = strdup(norm);
}

/* PoP: _parse_skills_argument @ cli.py:_parse_skills_argument */
int cli_parse_skills_argument(const json_t *skills, char ***out_items)
{
    if (out_items) *out_items = NULL;
    if (!skills || skills->type == JSON_NULL ||
        (skills->type == JSON_BOOL && !skills->bool_val))
        return 0;

    /* Collect raw values: str → [str], array → [str(item)...], other → [str]. */
    int kind_str = (skills->type == JSON_STRING);
    const char *single = NULL;
    char *owned_single = NULL;
    if (kind_str) single = skills->str_val;
    else if (skills->type != JSON_ARRAY)
        single = owned_single = json_serialize((json_t *)skills);

    h2_skill_list lst = {0};

    if (kind_str || owned_single) {
        if (single) {
            /* Python str path: split(",") over the whole string. */
            char *dup = strdup(single), *save = NULL;
            if (dup) {
                for (char *tok = strtok_r(dup, ",", &save); tok;
                     tok = strtok_r(NULL, ",", &save))
                    h2_skill_push(&lst, tok);
                free(dup);
            }
        }
    } else {
        for (size_t i = 0; i < json_len((json_t *)skills); i++) {
            json_t *item = json_get((json_t *)skills, i);
            if (!item || item->type == JSON_NULL) continue;   /* None dropped */
            if (item->type == JSON_STRING) {
                /* Each list item may itself contain commas. */
                char *dup = strdup(item->str_val ? item->str_val : ""), *save = NULL;
                for (char *tok = strtok_r(dup, ",", &save); tok;
                     tok = strtok_r(NULL, ",", &save))
                    h2_skill_push(&lst, tok);
                free(dup);
            } else {
                char *ser = json_serialize(item);
                h2_skill_push(&lst, ser);
                free(ser);
            }
        }
    }
    free(owned_single);

    if (lst.n == 0) { free(lst.items); return 0; }
    if (out_items) *out_items = lst.items;
    else {
        for (size_t i = 0; i < lst.n; i++) free(lst.items[i]);
        free(lst.items);
    }
    return (int)lst.n;
}

/* PoP: _is_ghostty_terminal @ cli.py:_is_ghostty_terminal */
int cli_is_ghostty_terminal(void)
{
    const char *tp = getenv("TERM_PROGRAM");
    if (tp) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s", tp);
        char *t = h2_trim(tmp);
        if (strcmp(t, "ghostty") == 0) return 1;
    }
    const char *term = getenv("TERM");
    if (term) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s", term);
        char *t = h2_trim(tmp);
        for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(t, "xterm-ghostty") == 0) return 1;
    }
    return 0;
}

/* PoP: _cleanup_all_terminals @ cli.py:_cleanup_all_terminals */
int cli_cleanup_all_terminals(void)
{
    /* Delegates to tools.terminal_tool.cleanup_all_environments (C port). */
    return term_cleanup_all_environments();
}

#define H2_PACK_SPRAWL_THRESHOLD 15

/* PoP: _maintain_pack_health @ cli.py:_maintain_pack_health */
void cli_maintain_pack_health(const char *repo_root)
{
    if (!repo_root) return;
    char pack_dir[4096];
    snprintf(pack_dir, sizeof(pack_dir), "%s/.git/objects/pack", repo_root);
    DIR *d = opendir(pack_dir);
    if (!d) return;
    int packs = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t l = strlen(ent->d_name);
        if (l > 5 && strcmp(ent->d_name + l - 5, ".pack") == 0) packs++;
    }
    closedir(d);
    if (packs < H2_PACK_SPRAWL_THRESHOLD) return;
    hermes_log(LOG_INFO, "cli",
               "git pack sprawl (%d packs) — repacking in background", packs);
    char *args1[] = {"nice", "-n", "19", "git", "repack", "-a", "-d", "--quiet", NULL};
    h2_run(args1, repo_root, 1800, NULL);
    char *args2[] = {"git", "worktree", "prune", NULL};
    h2_run(args2, repo_root, 60, NULL);
}

/*
 * Returns 1 when the CPR-disabled prompt_toolkit output should be built,
 * 0 when Application keeps its default output. Constructing the Vt100_Output
 * PoP: _select_classic_cli_pt_output @ cli.py:_select_classic_cli_pt_output
 */
int cli_select_classic_cli_pt_output(void)
{
    /* _terminal_may_leak_cpr(): PROMPT_TOOLKIT_NO_CPR=1 forces suppress;
     * win32 keeps default; every other platform suppresses. */
    const char *no_cpr = getenv("PROMPT_TOOLKIT_NO_CPR");
    if (no_cpr && strcmp(no_cpr, "1") == 0) return 1;
#if defined(_WIN32)
    return 0;
#else
    return 1;
#endif
}

/* PoP: _heal_cooked_mode_drift @ cli.py:_heal_cooked_mode_drift */
int cli_heal_cooked_mode_drift(int fd)
{
    struct termios attrs;
    if (tcgetattr(fd, &attrs) != 0) return 0;
    tcflag_t lflag = attrs.c_lflag;
    if (!(lflag & (ICANON | ECHO))) return 0;   /* still raw — nothing to do */
    /* Same surgery as prompt_toolkit.input.vt100.raw_mode, applied in place. */
    attrs.c_lflag = lflag & ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    attrs.c_iflag &= ~(tcflag_t)(IXON | IXOFF | ICRNL | INLCR | IGNCR);
    attrs.c_cc[VMIN] = 1;                        /* per-byte reads */
    if (tcsetattr(fd, TCSANOW, &attrs) != 0) return 0;
    return 1;
}

/* PoP: _wait_for_oneshot_background_completions @ cli.py:_wait_for_oneshot_background_completions */
void cli_wait_for_oneshot_background_completions(const char *task_id)
{
    /* Delegates the bounded wait (and its terminal.oneshot_completion_wait_seconds
     * bound) to the process registry; waits on the whole registry (task_id NULL),
     * matching the Python call site argument. Cheap no-op when nothing pending. */
    (void)task_id;
    char *result = process_registry_wait_for_pending_completions(NULL);
    if (!result) return;
    if (strstr(result, "\"waited\":[") && !strstr(result, "\"waited\":[]"))
        hermes_log(LOG_INFO, "cli",
                   "one-shot exit linger completed: %s", result);
    free(result);
}

/* PoP: _normalize_moa_model @ cli.py:_normalize_moa_model */
int cli_normalize_moa_model(const char *model, char **out_provider, char **out_rest)
{
    if (out_provider) *out_provider = NULL;
    if (out_rest) *out_rest = model ? strdup(model) : NULL;
    if (!model || !out_provider) return 0;
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", model);
    char *stripped = h2_trim(tmp);

    /* case-insensitive "moa:" prefix */
    char lower[2048];
    snprintf(lower, sizeof(lower), "%s", stripped);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strncmp(lower, "moa:", 4) != 0) return 0;

    char *preset = stripped + 4;          /* split(":", 1)[1].strip() */
    preset = h2_trim(preset);
    if (!*preset) return 0;
    *out_provider = strdup("moa");
    if (out_rest) { free(*out_rest); *out_rest = strdup(preset); }
    return 1;
}

/* PoP: _worktree_commits_all_merged_upstream @ cli.py:_worktree_commits_all_merged_upstream */
int cli_worktree_commits_all_merged_upstream(const char *worktree_path, int timeout,
                                             int max_ahead, json_t *cache)
{
    if (!worktree_path) return 0;
    const char *candidates[] = {"origin/HEAD", "origin/main", "origin/master"};
    const char *base = NULL;
    for (int i = 0; i < 3; i++) {
        char *args[] = {"git", "rev-parse", "--verify", "--quiet", (char *)candidates[i], NULL};
        char *out = NULL;
        int rc = h2_run(args, worktree_path, timeout, &out);
        free(out);
        if (rc == 0) { base = candidates[i]; break; }
        if (rc == 127) return 0;             /* spawn failure — preserve */
    }
    if (!base) return 0;

    char cache_key[256] = {0};
    int have_key = 0;
    if (cache && cache->type == JSON_OBJECT) {
        char revspec[128];
        snprintf(revspec, sizeof(revspec), "%s^{commit}", base);
        char *args[] = {"git", "rev-parse", revspec, "HEAD^{commit}", NULL};
        char *out = NULL;
        int rc = h2_run(args, worktree_path, timeout, &out);
        if (rc == 0 && out) {
            char *p = out, *save = NULL;
            char *shas[2]; int ns = 0;
            for (char *tok = strtok_r(p, " \t\r\n", &save); tok && ns < 2;
                 tok = strtok_r(NULL, " \t\r\n", &save))
                shas[ns++] = tok;
            if (ns == 2) {
                snprintf(cache_key, sizeof(cache_key), "%s..%s:%d", shas[0], shas[1], max_ahead);
                have_key = 1;
                json_t *hit = json_obj_get(cache, cache_key);
                if (hit && hit->type == JSON_BOOL) { free(out); return hit->bool_val ? 1 : 0; }
            }
        }
        free(out);
    }

#define H2_MEMO(verdict)                                                     \
    (void)((cache && cache->type == JSON_OBJECT && have_key)                 \
               ? json_set(cache, cache_key, json_bool((verdict) ? 1 : 0))    \
               : NULL),                                                       \
    (verdict)

    char range[160];
    snprintf(range, sizeof(range), "%s..HEAD", base);
    char *args_ahead[] = {"git", "rev-list", "--count", range, NULL};
    char *ahead_out = NULL;
    if (h2_run(args_ahead, worktree_path, timeout, &ahead_out) != 0)
        { free(ahead_out); return 0; }
    long count = ahead_out ? strtol(h2_trim(ahead_out), NULL, 10) : 0;
    free(ahead_out);
    if (count == 0) return H2_MEMO(1);
    if (count > max_ahead) return H2_MEMO(0);

    char *cherry_args[] = {"git", "cherry", (char *)base, "HEAD", NULL};
    char *cherry_out = NULL;
    if (h2_run(cherry_args, worktree_path, timeout, &cherry_out) != 0)
        { free(cherry_out); return 0; }

    int all_minus = 1, any_line = 0;
    if (cherry_out) {
        char *dup = cherry_out, *save = NULL;
        for (char *line = strtok_r(dup, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            char *t = h2_trim(line);
            if (!*t) continue;
            any_line = 1;
            if (t[0] != '-') all_minus = 0;
        }
    }
    free(cherry_out);
    return H2_MEMO(any_line &&all_minus);
#undef H2_MEMO
}

/* PoP: _worktree_has_unpushed_commits @ cli.py:_worktree_has_unpushed_commits */
int cli_worktree_has_unpushed_commits(const char *worktree_path, int timeout)
{
    if (!worktree_path) return 1;
    char *refs_args[] = {"git", "for-each-ref", "--format=%(refname)", "refs/remotes", NULL};
    char *refs_out = NULL;
    if (h2_run(refs_args, worktree_path, timeout, &refs_out) != 0)
        { free(refs_out); return 1; }        /* fail safe toward True */
    int has_remotes = refs_out && *h2_trim(refs_out);
    free(refs_out);
    if (!has_remotes) return 0;              /* no remote baseline → nothing unpushed */

    char *log_args[] = {"git", "log", "--oneline", "HEAD", "--not", "--remotes", NULL};
    char *log_out = NULL;
    int rc = h2_run(log_args, worktree_path, timeout, &log_out);
    if (rc != 0) { free(log_out); return 1; }
    int unpushed = log_out && *h2_trim(log_out);
    free(log_out);
    return unpushed;
}

static int g_deferred_agent_startup_done = 0;

/* PoP: _prepare_deferred_agent_startup @ cli.py:_prepare_deferred_agent_startup */
void cli_prepare_deferred_agent_startup(void)
{
    if (g_deferred_agent_startup_done) return;
    const char *defer = getenv("HERMES_DEFER_AGENT_STARTUP");
    if (!defer || strcmp(defer, "1") != 0) return;
    g_deferred_agent_startup_done = 1;

    int accept_hooks = 0;
    const char *ah = getenv("HERMES_ACCEPT_HOOKS");
    if (ah) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%s", ah);
        for (char *p = tmp; *p; p++) *p = (char)tolower((unsigned char)*p);
        accept_hooks = (!strcmp(tmp, "1") || !strcmp(tmp, "true") ||
                        !strcmp(tmp, "yes") || !strcmp(tmp, "on"));
    }
    (void)accept_hooks;

    /* shell-hook + outbound-webhook registration from loaded config (real C
     * ports). Plugin / MCP background discovery are Python-runtime surfaces;
     * their absence is logged debug-side exactly like the Python except paths
     * would. */
    json_t *cfg = config_py_read_raw_config();
    if (register_from_config(cfg) != 0)
        hermes_log(LOG_DEBUG, "cli",
                   "shell-hook registration failed at deferred CLI startup");
    if (!ow_register_from_config(cfg))
        hermes_log(LOG_DEBUG, "cli",
                   "outbound webhook registration failed at deferred CLI startup");
    if (cfg) json_free(cfg);
}

/* PoP: _prune_orphaned_branches @ cli.py:_prune_orphaned_branches */
void cli_prune_orphaned_branches(const char *repo_root)
{
    if (!repo_root) return;
    char *br_args[] = {"git", "branch", "--format=%(refname:short)", NULL};
    char *br_out = NULL;
    if (h2_run(br_args, repo_root, 10, &br_out) != 0) { free(br_out); return; }

    /* Collect branches actively checked out in a worktree. */
    char *wt_args[] = {"git", "worktree", "list", "--porcelain", NULL};
    char *wt_out = NULL;
    if (h2_run(wt_args, repo_root, 10, &wt_out) != 0) { free(br_out); free(wt_out); return; }

    char active[256][256];
    int n_active = 0;
    if (wt_out) {
        char *save = NULL;
        for (char *line = strtok_r(wt_out, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            const char *prefix = "branch refs/heads/";
            size_t plen = strlen(prefix);
            if (strncmp(line, prefix, plen) == 0 && n_active < 256) {
                char *nm = h2_trim(line + plen);
                snprintf(active[n_active++], 256, "%s", nm);
            }
        }
    }
    free(wt_out);

    /* Protect current branch and main. */
    char *cur_args[] = {"git", "branch", "--show-current", NULL};
    char *cur_out = NULL;
    if (h2_run(cur_args, repo_root, 5, &cur_out) == 0 && cur_out) {
        char *cur = h2_trim(cur_out);
        if (*cur && n_active < 256) snprintf(active[n_active++], 256, "%s", cur);
    }
    free(cur_out);
    if (n_active < 256) snprintf(active[n_active++], 256, "main");

    /* orphaned = not active AND (hermes/hermes-* or pr-*) */
    char **orphaned = NULL;
    int n_orphaned = 0, cap = 0;
    char *save2 = NULL;
    for (char *line = strtok_r(br_out, "\n", &save2); line;
         line = strtok_r(NULL, "\n", &save2)) {
        char *b = h2_trim(line);
        if (!*b) continue;
        int is_active = 0;
        for (int i = 0; i < n_active; i++)
            if (strcmp(active[i], b) == 0) { is_active = 1; break; }
        if (is_active) continue;
        if (strncmp(b, "hermes/hermes-", 14) != 0 && strncmp(b, "pr-", 3) != 0)
            continue;
        if (n_orphaned >= cap) {
            cap = cap ? cap * 2 : 16;
            char **np = realloc(orphaned, cap * sizeof(char *));
            if (!np) break;
            orphaned = np;
        }
        orphaned[n_orphaned++] = strdup(b);
    }
    free(br_out);

    if (n_orphaned == 0) { free(orphaned); return; }

    /* Delete in batches of 50. */
    for (int i = 0; i < n_orphaned; i += 50) {
        int batch_n = (n_orphaned - i < 50) ? n_orphaned - i : 50;
        char **argv = malloc((batch_n + 3) * sizeof(char *));
        if (!argv) break;
        argv[0] = "git"; argv[1] = "branch"; argv[2] = "-D";
        for (int j = 0; j < batch_n; j++) argv[3 + j] = orphaned[i + j];
        argv[batch_n + 3] = NULL;
        h2_run(argv, repo_root, 30, NULL);
        free(argv);
    }
    hermes_log(LOG_DEBUG, "cli", "Pruned %d orphaned branches", n_orphaned);

    for (int i = 0; i < n_orphaned; i++) free(orphaned[i]);
    free(orphaned);
}

/* PoP: _cleanup_all_browsers @ cli.py:_cleanup_all_browsers */
void cli_cleanup_all_browsers(void)
{
    /* Delegates to tools.browser_tool._emergency_cleanup_all_sessions (C port).
     * The C symbol isn't in a public header yet; declare it here. */
    extern void browser__emergency_cleanup_all_sessions(void);
    browser__emergency_cleanup_all_sessions();
}

static json_t *g_reverse_alias_cache = NULL;

/* PoP: _reverse_alias_for_display @ cli.py:_reverse_alias_for_display */
char *cli_reverse_alias_for_display(const char *model_name)
{
    if (!model_name || !*model_name)
        return model_name ? strdup(model_name) : NULL;
    if (!g_reverse_alias_cache) {
        json_t *rmap = json_object();
        json_t *cfg = config_py_read_raw_config();
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *ma = json_obj_get(cfg, "model_aliases");
            if (ma && ma->type == JSON_OBJECT) {
                for (size_t i = 0; i < ma->c.count; i++) {
                    json_t *entry = ma->c.items[i];
                    const char *alias = ma->c.keys[i];
                    if (!entry || entry->type != JSON_OBJECT) continue;
                    json_t *mnode = json_obj_get(entry, "model");
                    const char *m = (mnode && mnode->type == JSON_STRING &&
                                     mnode->str_val) ? mnode->str_val : "";
                    char mt[512];
                    snprintf(mt, sizeof(mt), "%s", m);
                    char *mm = h2_trim(mt);
                    if (!*mm) continue;
                    json_t *cur = json_obj_get(rmap, mm);
                    if (!cur || strlen(alias) < strlen(cur->str_val ? cur->str_val : ""))
                        json_set(rmap, mm, json_string(alias));
                }
            }
            json_t *mdl = json_obj_get(cfg, "model");
            if (mdl && mdl->type == JSON_OBJECT) {
                json_t *simple = json_obj_get(mdl, "aliases");
                if (simple && simple->type == JSON_OBJECT) {
                    for (size_t i = 0; i < simple->c.count; i++) {
                        json_t *v = simple->c.items[i];
                        const char *alias = simple->c.keys[i];
                        if (!v || v->type != JSON_STRING || !v->str_val) continue;
                        char val[512];
                        snprintf(val, sizeof(val), "%s", v->str_val);
                        char *vv = h2_trim(val);
                        if (!*vv) continue;
                        /* m = v.split("/", 1)[1] if "/" in v else v */
                        char *slash = strchr(vv, '/');
                        char *m = slash ? slash + 1 : vv;
                        json_t *cur = json_obj_get(rmap, m);
                        if (!cur || strlen(alias) < strlen(cur->str_val ? cur->str_val : ""))
                            json_set(rmap, m, json_string(alias));
                    }
                }
            }
        }
        if (cfg) json_free(cfg);
        g_reverse_alias_cache = rmap;
    }
    json_t *alias = json_obj_get(g_reverse_alias_cache, model_name);
    if (alias && alias->type == JSON_STRING && alias->str_val)
        return strdup(alias->str_val);
    return strdup(model_name);
}

static int g_tui_input_modes_active = 0;

/* PoP: _mark_tui_input_modes_active @ cli.py:_mark_tui_input_modes_active */
void cli_mark_tui_input_modes_active(void)
{
    g_tui_input_modes_active = 1;
}

int cli_tui_input_modes_active(void)
{
    return g_tui_input_modes_active;
}

/* PoP: _path_is_within_root @ cli.py:_path_is_within_root */
int cli_path_is_within_root(const char *path, const char *root)
{
    /* Path.relative_to() is lexical: child must equal root or sit below it
     * on a component boundary. */
    if (!path || !root) return 0;
    size_t rl = strlen(root);
    while (rl > 1 && root[rl - 1] == '/') rl--;      /* normalize trailing '/' */
    if (strncmp(path, root, rl) != 0) return 0;
    char next = path[rl];
    if (next == '\0') return 1;                      /* equal */
    if (next == '/') return 1;                       /* direct child subtree */
    return 0;                                        /* e.g. /rootX vs /root */
}
