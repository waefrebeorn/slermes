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

/* Clear-side setter used by _reset_terminal_input_modes_on_exit
 * (cli_gaps_2026_08_25.c): Python clears _tui_input_modes_active so a
 * re-armed cleanup never re-emits the reset codes. */
void cli_mark_tui_input_modes_active_set(int v)
{
    g_tui_input_modes_active = v ? 1 : 0;
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

/* ════════════════════════════════════════════════════════════════════════
 * Batch 0 continuation: status-bar / formatting surface (hcli_t convention).
 * ════════════════════════════════════════════════════════════════════════ */
#include "cli_hermes_cli.h"
#include "hermes_state_db.h"
#include <sys/ioctl.h>
#include <math.h>

extern char *cl2_format_duration_compact(double seconds);

/* PoP: hcli_release_active_session @ cli.py:_release_active_session */
void hcli_release_active_session(hcli_t *self)
{
    if (!self || !self->active_session_lease) return;
    if (self->lease_release)
        self->lease_release(self->active_session_lease);
    /* Python wraps release() in try/except with a debug log; the C lease
     * callback is best-effort by contract. */
    self->active_session_lease = NULL;
}

/* PoP: cli_redraw_rebuilds_scrollback @ cli.py:_redraw_rebuilds_scrollback */
bool cli_redraw_rebuilds_scrollback(void)
{
    json_t *cfg = config_py_read_raw_config();
    bool result = false;
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *display = json_obj_get(cfg, "display");
        if (display && display->type == JSON_OBJECT) {
            json_t *raw = json_obj_get(display, "cli_rebuild_scrollback_on_redraw");
            if (raw && raw->type == JSON_STRING && raw->str_val) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", raw->str_val);
                char *t = h2_trim(tmp);
                for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
                result = (!strcmp(t, "1") || !strcmp(t, "true") ||
                          !strcmp(t, "yes") || !strcmp(t, "on") ||
                          !strcmp(t, "always"));
            } else if (raw && raw->type == JSON_BOOL) {
                result = raw->bool_val;
            } else if (raw && raw->type == JSON_NUMBER) {
                result = raw->num_val != 0.0;
            }
        }
    }
    if (cfg) json_free(cfg);
    return result;
}

/* PoP: cli_status_bar_context_style @ cli.py:_status_bar_context_style */
const char *cli_status_bar_context_style(int percent_used)
{
    if (percent_used < 0) return "class:status-bar-dim";   /* None */
    if (percent_used >= 95) return "class:status-bar-critical";
    if (percent_used > 80) return "class:status-bar-bad";
    if (percent_used >= 50) return "class:status-bar-warn";
    return "class:status-bar-good";
}

/* PoP: cli_battery_status_style @ cli.py:_battery_status_style */
const char *cli_battery_status_style(const char *category)
{
    if (!category) return "class:status-bar-dim";
    if (!strcmp(category, "good")) return "class:status-bar-good";
    if (!strcmp(category, "warn")) return "class:status-bar-warn";
    if (!strcmp(category, "bad")) return "class:status-bar-bad";
    if (!strcmp(category, "critical")) return "class:status-bar-critical";
    return "class:status-bar-dim";
}

/* PoP: cli_compression_count_style @ cli.py:_compression_count_style */
const char *cli_compression_count_style(int count)
{
    if (count >= 10) return "class:status-bar-bad";
    if (count >= 5) return "class:status-bar-warn";
    return "class:status-bar-dim";
}

/* PoP: cli_build_context_bar @ cli.py:_build_context_bar */
char *cli_build_context_bar(int percent_used, int width)
{
    if (width < 0) width = 10;
    int safe = percent_used < 0 ? 0 : percent_used;
    if (safe < 0) safe = 0;
    if (safe > 100) safe = 100;
    /* Python round(): banker's rounding on .5 — round-half-even. */
    double exact = ((double)safe / 100.0) * (double)width;
    int filled = (int)lround(exact);
    /* Match Python round() half-to-even at exact .5 boundaries. */
    if (exact - (double)(long)exact == 0.5) {
        long fl = (long)exact;
        filled = (int)((fl % 2 == 0) ? fl : fl + 1);
    }
    int empty = width - filled;
    if (empty < 0) empty = 0;
    /* UTF-8: '█' = 3 bytes, '░' = 3 bytes. */
    size_t cap = (size_t)width * 3 + 8;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = '[';
    for (int i = 0; i < filled; i++) { memcpy(out + o, "\xE2\x96\x88", 3); o += 3; }
    for (int i = 0; i < empty; i++) { memcpy(out + o, "\xE2\x96\x91", 3); o += 3; }
    out[o++] = ']';
    out[o] = '\0';
    return out;
}

/* PoP: cli_format_prompt_elapsed @ cli.py:_format_prompt_elapsed */
char *cli_format_prompt_elapsed(bool has_start, double start_time,
                                double duration, bool live)
{
    if (!has_start && duration == 0.0)
        return strdup("\xE2\x8F\xB2 0s");            /* ⏲ 0s */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double now = (double)ts.tv_sec + ts.tv_nsec / 1e9;
    double elapsed = has_start ? (now - start_time) : duration;
    if (elapsed < 0.0) elapsed = 0.0;

    int days = (int)(elapsed / 86400.0);
    double remaining = fmod(elapsed, 86400.0);
    int hours = (int)(remaining / 3600.0);
    remaining = fmod(remaining, 3600.0);
    int minutes = (int)(remaining / 60.0);
    int seconds = (int)fmod(remaining, 60.0);

    char time_str[64];
    if (days > 0) {
        snprintf(time_str, sizeof(time_str), "%dd %dh %dm", days, hours, minutes);
    } else if (hours > 0) {
        if (seconds)
            snprintf(time_str, sizeof(time_str), "%dh %dm %ds", hours, minutes, seconds);
        else
            snprintf(time_str, sizeof(time_str), "%dh %dm", hours, minutes);
    } else if (minutes > 0) {
        if (seconds)
            snprintf(time_str, sizeof(time_str), "%dm %ds", minutes, seconds);
        else
            snprintf(time_str, sizeof(time_str), "%dm", minutes);
    } else {
        snprintf(time_str, sizeof(time_str), "%ds", (int)elapsed);
    }

    /* ⏱ U+23F1 = E2 8F B1 ; ⏲ U+23F2 = E2 8F B2 */
    const char *emoji = live ? "\xE2\x8F\xB1" : "\xE2\x8F\xB2";
    char *out = malloc(strlen(emoji) + strlen(time_str) + 4);
    if (!out) return NULL;
    sprintf(out, "%s %s", emoji, time_str);
    return out;
}

/* PoP: cli_format_idle_since @ cli.py:_format_idle_since */
char *cli_format_idle_since(bool has_last_finished_at, double last_finished_at,
                            bool turn_live)
{
    if (turn_live || !has_last_finished_at) return strdup("");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double now = (double)ts.tv_sec + ts.tv_nsec / 1e9;
    double idle = now - last_finished_at;
    if (idle < 0.0) idle = 0.0;
    char *compact = cl2_format_duration_compact(idle);
    if (!compact) return NULL;
    /* ✓ = E2 9C 93 */
    char *out = malloc(strlen(compact) + 16);
    if (!out) { free(compact); return NULL; }
    sprintf(out, "\xE2\x9C\x93 %s", compact);
    free(compact);
    return out;
}

/* PoP: hcli_get_status_bar_session_title @ cli.py:_get_status_bar_session_title */
char *hcli_get_status_bar_session_title(hcli_t *self)
{
    if (!self) return strdup("");
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + ts.tv_nsec / 1e9;

    const char *session_id = self->session_id ? self->session_id : "";
    char pending_buf[1024] = {0};
    const char *pending = "";
    if (self->pending_title) {
        snprintf(pending_buf, sizeof(pending_buf), "%s", self->pending_title);
        char *t = h2_trim(pending_buf);
        pending = t;
    }
    if (*pending) {
        free(self->status_bar_title_session_id);
        free(self->status_bar_title_cache);
        self->status_bar_title_session_id = strdup(session_id);
        self->status_bar_title_cache = strdup(pending);
        self->status_bar_title_checked_at = now;
        return strdup(pending);
    }

    const char *cached_sid = self->status_bar_title_session_id;
    if (cached_sid && strcmp(cached_sid, session_id) == 0 &&
        now - self->status_bar_title_checked_at < 1.5 &&
        self->status_bar_title_cache)
        return strdup(self->status_bar_title_cache);

    char title[1024] = {0};
    if (self->session_db && *session_id) {
        char *row_json = hermes_state_get_session(self->session_db, session_id);
        if (row_json) {
            json_t *row = json_parse(row_json, NULL);
            json_t *t = row ? json_obj_get(row, "title") : NULL;
            if (t && t->type == JSON_STRING && t->str_val) {
                snprintf(title, sizeof(title), "%s", t->str_val);
                char *tt = h2_trim(title);
                memmove(title, tt, strlen(tt) + 1);
            }
            if (row) json_free(row);
            free(row_json);
        }
    }
    free(self->status_bar_title_session_id);
    free(self->status_bar_title_cache);
    self->status_bar_title_session_id = strdup(session_id);
    self->status_bar_title_cache = strdup(title);
    self->status_bar_title_checked_at = now;
    return strdup(title);
}

/* PoP: cli_status_bar_display_width @ cli.py:_status_bar_display_width */
/* prompt_toolkit get_cwidth port: decode UTF-8 codepoints and classify —
 * zero-width combining/variation selectors, wide East-Asian & emoji ranges,
 * everything else (including ambiguous-width glyphs like ─ ✓ ⏱) = 1. */
static int h2_cp_width(unsigned long cp)
{
    if (cp == 0) return 0;
    /* Combining marks + variation selectors + zero-width space family. */
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) ||
        cp == 0xFEFF || (cp >= 0xFE00 && cp <= 0xFE0F) ||
        (cp >= 0x1AB0 && cp <= 0x1AFF) || (cp >= 0x20D0 && cp <= 0x20FF))
        return 0;
    /* Wide East-Asian / emoji ranges (East_Asian_Width W or F). */
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3041 && cp <= 0x33FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xA000 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0xFE30 && cp <= 0xFE6F) ||
        (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1F300 && cp <= 0x1F64F) || (cp >= 0x1F900 && cp <= 0x1F9FF) ||
        (cp >= 0x1F680 && cp <= 0x1F6FF) || (cp >= 0x20000))
        return 2;
    return 1;
}

static int h2_utf8_next(const unsigned char *s, unsigned long *cp)
{
    if (*s < 0x80) { *cp = *s; return 1; }
    int n; unsigned long v;
    if ((*s & 0xE0) == 0xC0) { n = 2; v = *s & 0x1Fu; }
    else if ((*s & 0xF0) == 0xE0) { n = 3; v = *s & 0x0Fu; }
    else if ((*s & 0xF8) == 0xF0) { n = 4; v = *s & 0x07u; }
    else { *cp = 0xFFFD; return 1; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *cp = 0xFFFD; return i; }
        v = (v << 6) | (unsigned long)(s[i] & 0x3Fu);
    }
    *cp = v;
    return n;
}

int cli_status_bar_display_width(const char *text)
{
    if (!text) return 0;
    int w = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        unsigned long cp;
        p += h2_utf8_next(p, &cp);
        w += h2_cp_width(cp);
    }
    return w;
}

/* Walk text returning the byte offset whose accumulated display width first
 * exceeds max_w (i.e. the trim point), or the full length. */
static size_t h2_width_limit_offset(const char *text, int add_w, int max_w)
{
    size_t off = 0;
    int w = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        unsigned long cp;
        int n = h2_utf8_next(p, &cp);
        int cw = h2_cp_width(cp);
        if (w + cw + add_w > max_w) break;
        w += cw;
        off += (size_t)n;
        p += n;
    }
    return off;
}

/* PoP: cli_trim_status_bar_text @ cli.py:_trim_status_bar_text */
char *cli_trim_status_bar_text(const char *text, int max_width)
{
    if (!text) text = "";
    if (max_width <= 0) return strdup("");
    if (cli_status_bar_display_width(text) <= max_width)
        return strdup(text);
    static const char ellipsis[] = "...";
    int ellipsis_width = 3;
    if (max_width <= ellipsis_width) {
        char buf[4] = {0};
        snprintf(buf, (size_t)max_width + 1, "%s", ellipsis);
        return strdup(buf);
    }
    size_t off = h2_width_limit_offset(text, ellipsis_width, max_width);
    char *head = malloc(off + 1);
    if (!head) return NULL;
    memcpy(head, text, off);
    head[off] = '\0';
    /* rstrip() */
    size_t n = strlen(head);
    while (n > 0 && isspace((unsigned char)head[n - 1])) head[--n] = '\0';
    char *out = realloc(head, n + sizeof(ellipsis));
    if (!out) { free(head); return NULL; }
    memcpy(out + n, ellipsis, sizeof(ellipsis));
    return out;
}

/* PoP: cli_right_align_status_title @ cli.py:_right_align_status_title */
char *cli_right_align_status_title(const char *text, const char *title_in, int width)
{
    char title[1024] = {0};
    snprintf(title, sizeof(title), "%s", title_in ? title_in : "");
    char *t = h2_trim(title);
    if (!*t || width < 24)
        return cli_trim_status_bar_text(text, width);

    int title_width = width / 3;
    if (title_width < 6) title_width = 6;
    if (title_width > 30) title_width = 30;
    char *inner = cli_trim_status_bar_text(t, title_width - 2);
    /* badge = f" {inner} " ; suffix = f" ─{badge}" (─ = E2 94 80) */
    size_t blen = strlen(inner) + 8;
    char *badge = malloc(blen);
    if (!badge) { free(inner); return NULL; }
    snprintf(badge, blen, " %s ", inner);
    char *suffix = malloc(blen + 8);
    if (!suffix) { free(badge); free(inner); return NULL; }
    snprintf(suffix, blen + 8, " \xE2\x94\x80%s", badge);
    int left_width = width - cli_status_bar_display_width(suffix);
    if (left_width < 0) left_width = 0;

    /* left = trim(text.rstrip(), left_width) */
    char *rstrip = strdup(text ? text : "");
    size_t rn = strlen(rstrip);
    while (rn > 0 && isspace((unsigned char)rstrip[rn - 1])) rstrip[--rn] = '\0';
    char *left = cli_trim_status_bar_text(rstrip, left_width);
    free(rstrip);
    int pad = left_width - cli_status_bar_display_width(left);
    if (pad < 0) pad = 0;

    size_t cap = strlen(left) + (size_t)pad + strlen(suffix) + 1;
    char *out = malloc(cap);
    if (!out) { free(badge); free(suffix); free(left); free(inner); return NULL; }
    sprintf(out, "%s%*s%s", left, pad, "", suffix);
    free(badge); free(suffix); free(left); free(inner);
    return out;
}

/* PoP: hcli_right_align_status_title_fragments @ cli.py:_right_align_status_title_fragments */
hcli_fragment_t *hcli_right_align_status_title_fragments(char **styles,
                                                         char **values,
                                                         size_t n,
                                                         const char *title_in,
                                                         int width,
                                                         size_t *out_len)
{
    if (out_len) *out_len = 0;
    char title[1024] = {0};
    snprintf(title, sizeof(title), "%s", title_in ? title_in : "");
    char *t = h2_trim(title);
    if (!*t || width < 24) return NULL;   /* caller keeps original frags */

    int title_width = width / 3;
    if (title_width < 6) title_width = 6;
    if (title_width > 30) title_width = 30;
    char *badge_inner = cli_trim_status_bar_text(t, title_width - 2);
    size_t blen = strlen(badge_inner) + 8;
    char *badge = malloc(blen);
    if (!badge) { free(badge_inner); return NULL; }
    snprintf(badge, blen, " %s ", badge_inner);
    /* suffix_width = width(" ─") + width(badge) */
    int suffix_width = cli_status_bar_display_width(" \xE2\x94\x80") +
                       cli_status_bar_display_width(badge);
    int left_width = width - suffix_width;
    if (left_width < 0) left_width = 0;

    hcli_fragment_t *trimmed = malloc((n + 2) * sizeof(hcli_fragment_t));
    if (!trimmed) { free(badge); free(badge_inner); return NULL; }
    size_t nt = 0;
    int used = 0;
    for (size_t i = 0; i < n; i++) {
        int remaining = left_width - used;
        if (remaining <= 0) break;
        int vw = cli_status_bar_display_width(values[i]);
        if (vw <= remaining) {
            trimmed[nt].style = styles[i] ? strdup(styles[i]) : NULL;
            trimmed[nt].text = values[i] ? strdup(values[i]) : NULL;
            nt++;
            used += vw;
            continue;
        }
        char *clipped = cli_trim_status_bar_text(values[i], remaining);
        if (clipped && *clipped) {
            trimmed[nt].style = styles[i] ? strdup(styles[i]) : NULL;
            trimmed[nt].text = clipped;
            nt++;
            used += cli_status_bar_display_width(clipped);
        } else {
            free(clipped);
        }
        break;
    }

    if (used < left_width) {
        char *pad = malloc((size_t)(left_width - used) + 1);
        if (pad) {
            memset(pad, ' ', (size_t)(left_width - used));
            pad[left_width - used] = '\0';
            trimmed[nt].style = strdup("class:status-bar-dim");
            trimmed[nt].text = pad;
            nt++;
        }
    }
    trimmed[nt].style = strdup("class:status-bar-dim");
    trimmed[nt].text = strdup(" \xE2\x94\x80");   /* " ─" */
    nt++;
    trimmed[nt].style = strdup("class:status-bar-session-title");
    trimmed[nt].text = badge;                      /* ownership moves here */
    nt++;

    free(badge_inner);
    if (out_len) *out_len = nt;
    return trimmed;
}

/* PoP: cli_get_tui_terminal_width @ cli.py:_get_tui_terminal_width */
int cli_get_tui_terminal_width(void)
{
    /* The TUI-layout query (prompt_toolkit get_app().output.get_size()) has no
     * C counterpart; fall through to the shutil.get_terminal_size((80,24))
     * branch, which queries the controlling terminal via TIOCGWINSZ. */
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}

/* PoP: cli_scrollback_box_width @ cli.py:_scrollback_box_width */
int cli_scrollback_box_width(int width)
{
    if (width <= 0) {
        width = cli_get_tui_terminal_width();
        if (width <= 0) width = 80;
    }
    return width < 32 ? 32 : width;
}

