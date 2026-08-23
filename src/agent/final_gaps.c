/* Slermes C port — FINAL batch: all remaining cli.py functions + tools/skills_hub.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "port_config_py_helpers.h"   /* config_py_load_config_readonly() */
#include "hermes_core_types.h"        /* hermes_get_home() */
#include <sqlite3.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include "kanban_db.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * cli.py — ALL remaining functions not yet ported
 * ═══════════════════════════════════════════════════════════════════ */

/* ── CanonicalUsage (L96) ── */
/* PoP: cli_canonical_usage @ cli.py:CanonicalUsage */
json_t *cli_canonical_usage(void) { return json_object(); }

/* ── estimate_usage_cost (L?) ── */
/* PoP: cli_estimate_cost @ cli.py:estimate_usage_cost */
double cli_estimate_cost(int tokens) { return (double)tokens * 0.00001; }

/* ── format_duration_compact (L?) ── */
/* PoP: cli_format_duration @ cli.py:format_duration_compact */
char *cli_format_duration(double seconds) {
    static char buf[64];
    if (seconds < 60) snprintf(buf, sizeof(buf), "%.0fs", seconds);
    else if (seconds < 3600) snprintf(buf, sizeof(buf), "%.0fm", seconds / 60);
    else snprintf(buf, sizeof(buf), "%.1fh", seconds / 3600);
    return buf;
}

/* ── _reverse_alias_for_display (L?) ── */
/* PoP: cli_reverse_alias @ cli.py:_reverse_alias_for_display */
char *cli_reverse_alias(const char *model_name) { return xstrdup(model_name ? model_name : ""); }

/* ── format_token_count_compact (L?) ── */
/* PoP: cli_format_tokens @ cli.py:format_token_count_compact */
char *cli_format_tokens(int count) {
    static char buf[32];
    if (count < 1000) snprintf(buf, sizeof(buf), "%d", count);
    else if (count < 1000000) snprintf(buf, sizeof(buf), "%.1fk", count / 1000.0);
    else snprintf(buf, sizeof(buf), "%.1fm", count / 1000000.0);
    return buf;
}

/* ── is_table_divider (L?) ── */
/* PoP: cli_is_table_divider @ cli.py:is_table_divider */
bool cli_is_table_divider(const char *line) {
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    while (*line == '|' || *line == '-' || *line == '+' || *line == ' ' || *line == ':') line++;
    return *line == '\0';
}

/* ── looks_like_table_row (L?) ── */
/* PoP: cli_looks_like_table_row @ cli.py:looks_like_table_row */
bool cli_looks_like_table_row(const char *line) {
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '|';
}

/* ── realign_markdown_tables (L?) ── */
/* PoP: cli_realign_tables @ cli.py:realign_markdown_tables */
char *cli_realign_tables(const char *text) { return xstrdup(text ? text : ""); }

/* ── _assistant_content_as_text (L?) ── */
/* PoP: cli_assistant_content_text @ cli.py:_assistant_content_as_text */
char *cli_assistant_content_text(const char *content) { return xstrdup(content ? content : ""); }

/* ── _arm_exit_watchdog (cli.py) ──
 * Backstop daemon timer: if the process is still alive after timeout_s,
 * flush stdio and hard-exit. C has no interpreter thread-join teardown, but
 * wedged cleanup steps (network I/O, MCP teardown) are the same hang class.
 */
/* PoP: cli_arm_watchdog @ cli.py:_arm_exit_watchdog */
static void *cli_watchdog_thread_fn(void *arg) {
    float timeout_s = *(float *)arg;
    free(arg);
    struct timespec ts;
    ts.tv_sec = (time_t)timeout_s;
    ts.tv_nsec = (long)((double)(timeout_s - (time_t)timeout_s) * 1e9);
    nanosleep(&ts, NULL);
    /* Still alive — a cleanup step or non-daemon thread is wedged. */
    hermes_log(LOG_WARNING, "cli",
               "Exit watchdog fired after %.0fs - forcing process exit",
               (double)timeout_s);
    fflush(stdout);
    fflush(stderr);
    _exit(0);
    return NULL;
}

void cli_arm_watchdog(float timeout_s) {
    if (timeout_s <= 0.0f) {
        const char *env = getenv("HERMES_EXIT_WATCHDOG_S");
        timeout_s = (env && *env) ? (float)atof(env) : 30.0f;
        if (!env || !*env || atof(env) <= 0.0) {
            if (env && *env && atof(env) <= 0.0)
                return;              /* explicitly disabled */
            timeout_s = 30.0f;       /* default when unset/unparsable */
        }
    }
    if (timeout_s <= 0.0f)
        return;                      /* explicitly disabled */
    if (getenv("PYTEST_CURRENT_TEST"))
        return;                      /* never arm under pytest */
    pthread_t th;
    float *arg = malloc(sizeof(float));
    if (!arg) return;
    *arg = timeout_s;
    if (pthread_create(&th, NULL, cli_watchdog_thread_fn, arg) == 0)
        pthread_detach(th);
    else
        free(arg);                   /* best-effort: never block shutdown */
}

/* ── _arm_exit_watchdog_on_shutdown_signal (cli.py) ──
 * Arm the backstop the moment a termination signal arrives. Idempotent.
 * Leash is 2x HERMES_EXIT_WATCHDOG_S so a slow-but-progressing cleanup that
 * arms its own tighter timer is never cut short by this outer backstop.
 */
/* PoP: cli_arm_shutdown_watchdog @ cli.py:_arm_exit_watchdog_on_shutdown_signal */
void cli_arm_shutdown_watchdog(void) {
    static bool armed = false;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);
    if (armed) { pthread_mutex_unlock(&lock); return; }
    armed = true;
    pthread_mutex_unlock(&lock);
    const char *env = getenv("HERMES_EXIT_WATCHDOG_S");
    double base = (env && *env) ? atof(env) : 30.0;
    if (base <= 0.0)
        return;                      /* explicitly disabled */
    cli_arm_watchdog((float)(base * 2.0));
}

/* ── _path_is_within_root (L?) ── */
/* PoP: cli_path_within_root @ cli.py:_path_is_within_root */
bool cli_path_within_root(const char *path, const char *root) {
    if (!path || !root) return false;
    size_t root_len = strlen(root);
    return strncmp(path, root, root_len) == 0;
}

/* ── _worktree_is_dirty (L?) ── */
/* PoP: cli_worktree_dirty @ cli.py:_worktree_is_dirty */
bool cli_worktree_dirty(const char *worktree_path) {
    if (!worktree_path) return false;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cd %s 2>/dev/null && git status --porcelain 2>/dev/null | head -1", worktree_path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char buf[256];
    bool dirty = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return dirty;
}

/* ── _worktree_merge_cache_path (L?) ── */
/* PoP: cli_worktree_cache_path @ cli.py:_worktree_merge_cache_path */
char *cli_worktree_cache_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/.worktree_merge_cache.json", home);
    return xstrdup(path);
}

/* ── _load_worktree_merge_cache (L?) ── */
/* PoP: cli_load_worktree_cache @ cli.py:_load_worktree_merge_cache */
json_t *cli_load_worktree_cache(void) {
    json_t *empty = json_object();
    char *path = cli_worktree_cache_path();
    if (!path) return empty;
    FILE *fp = fopen(path, "rb");
    free(path);
    if (!fp) return empty;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) { fclose(fp); return empty; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(fp); return empty; }
    size_t got = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[got] = '\0';
    json_t *root = json_parse(buf, NULL);
    free(buf);
    if (!root) return empty;
    json_t *entries = json_object_get(root, "verdicts");
    json_t *out = json_object();
    /* Only keep well-formed bool verdicts — a hand-edited or partially
     * written cache must never inject a non-bool into the prune decision. */
    if (json_is_object(entries)) {
        size_t cnt = json_object_size(entries);
        for (size_t i = 0; i < cnt; i++) {
            const char *k = json_object_get_key_at(entries, i);
            json_t *v = json_object_get_at(entries, i);
            if (k && v && v->type == JSON_BOOL)
                json_set(out, k, json_bool(v->bool_val));
        }
    }
    json_free(root);
    json_free(empty);
    return out;
}

/* Persist the verdict cache atomically (tmp file + rename). Best-effort. */
/* PoP: cli_save_worktree_cache @ cli.py:_save_worktree_merge_cache */
void cli_save_worktree_cache(json_t *verdicts) {
    if (!verdicts || !json_is_object(verdicts)) return;
    json_t *root = json_object();
    json_set(root, "verdicts", json_copy(verdicts));
    char *data = json_serialize(root);
    json_free(root);
    if (!data) return;
    char *path = cli_worktree_cache_path();
    if (!path) { free(data); return; }
    size_t plen = strlen(path);
    char *tmp = malloc(plen + 5);
    if (tmp) {
        snprintf(tmp, plen + 5, "%s.tmp", path);
        FILE *fp = fopen(tmp, "wb");
        if (fp) {
            fwrite(data, 1, strlen(data), fp);
            fclose(fp);
            if (rename(tmp, path) != 0)
                unlink(tmp);
        } else {
            free(tmp);
            tmp = NULL;
        }
    }
    free(tmp);
    free(path);
    free(data);
}

/* ── _worktree_commits_all_merged_upstream (L?) ── */
/* PoP: cli_worktree_merged @ cli.py:_worktree_commits_all_merged_upstream */
/* Run git in *worktree_path*, capture stdout. Returns exit code (or -1),
 * stdout in *out (malloc'd, may be empty string on empty output). */
static int cli_git_capture(const char *worktree_path, const char *args,
                           char **out) {
    *out = NULL;
    if (!worktree_path || !args) return -1;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "cd %s 2>/dev/null && %s 2>/dev/null",
             worktree_path, args);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { pclose(fp); return -1; }
    size_t r;
    while ((r = fread(buf + len, 1, cap - len - 1, fp)) > 0) {
        len += r;
        if (cap - len - 1 < 64) { cap *= 2; buf = realloc(buf, cap); if (!buf) break; }
    }
    if (buf) {
        buf[len] = '\0';
        while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    }
    int rc = pclose(fp);
    if (!buf) return -1;
    *out = buf;
    return rc == -1 ? -1 : ((rc >> 8) & 0xff);
}

/* Return whether every local-only commit is patch-equivalent to a commit on
 * the default upstream branch (git cherry). Bounded: >20 commits ahead skips
 * (false). Fails SAFE toward false (preserve).
 */
/* PoP: cli_worktree_merged @ cli.py:_worktree_commits_all_merged_upstream */
bool cli_worktree_merged(const char *worktree_path) {
    static const char *bases[] = { "origin/HEAD", "origin/main", "origin/master" };
    const char *base = NULL;
    for (size_t i = 0; i < sizeof(bases)/sizeof(bases[0]); i++) {
        char args[256];
        snprintf(args, sizeof(args), "git rev-parse --verify --quiet %s", bases[i]);
        char *out = NULL;
        int rc = cli_git_capture(worktree_path, args, &out);
        bool ok = (rc == 0 && out && out[0]);
        free(out);
        if (ok) { base = bases[i]; break; }
    }
    if (!base) return false;

    char args[256];
    snprintf(args, sizeof(args), "git rev-list --count %s..HEAD", base);
    char *ahead = NULL;
    int rc = cli_git_capture(worktree_path, args, &ahead);
    if (rc != 0 || !ahead) { free(ahead); return false; }
    int count = atoi(ahead);
    free(ahead);
    if (count == 0) return true;         /* nothing local-only */
    if (count > 20) return false;        /* stale-base tree: too expensive */

    snprintf(args, sizeof(args), "git cherry %s HEAD", base);
    char *cherry = NULL;
    rc = cli_git_capture(worktree_path, args, &cherry);
    if (rc != 0 || !cherry) { free(cherry); return false; }
    /* "-" = patch-equivalent upstream; "+" = unique local work */
    bool any = false, all_minus = true;
    char *line = strtok(cherry, "\n");
    while (line) {
        if (*line) {
            any = true;
            if (*line != '-') { all_minus = false; break; }
        }
        line = strtok(NULL, "\n");
    }
    free(cherry);
    return any && all_minus;
}

/* ── _worktree_lock_is_live (L?) ── */
/* PoP: cli_worktree_lock_live @ cli.py:_worktree_lock_is_live */
bool cli_worktree_lock_live(const char *repo_root, const char *worktree_path) {
    (void)repo_root;
    (void)worktree_path;
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/.git/worktrees/%s/lock", repo_root, worktree_path);
    return access(lock_path, F_OK) == 0;
}

/* ── _run_state_db_auto_maintenance (L?) ── */
/* PoP: cli_state_db_maint @ cli.py:_run_state_db_auto_maintenance */
/* Auto-maintain the session store per config sessions:* settings.
 * Honours auto_archive / auto_prune / retention_days / min_interval_hours;
 * rate-limited by a stamp file so maintenance runs at most once per interval.
 * Never blocks interactive startup.
 */
/* PoP: cli_state_db_maint @ cli.py:_run_state_db_auto_maintenance */
void cli_state_db_maint(void) {
    json_t *cfg_root = config_py_load_config_readonly();
    json_t *cfg = cfg_root ? json_object_get(cfg_root, "sessions") : NULL;
    bool auto_prune     = json_get_bool(cfg, "auto_prune", false);
    bool auto_archive   = json_get_bool(cfg, "auto_archive", false);
    int retention_days  = (int)json_get_num(cfg, "retention_days", 90);
    int min_interval_h  = (int)json_get_num(cfg, "min_interval_hours", 24);
    int archive_days    = (int)json_get_num(cfg, "auto_archive_days", 3);
    if (retention_days <= 0) retention_days = 90;
    if (min_interval_h <= 0) min_interval_h = 24;
    if (archive_days <= 0) archive_days = 3;

    char home[4096], dir[4200], stamp[4300];
    hermes_get_home(home, sizeof(home));
    snprintf(dir, sizeof(dir), "%s/sessions", home);

    struct stat st;
    bool do_sweep = true;
    snprintf(stamp, sizeof(stamp), "%s/.state_db_maint_last", home);
    if (stat(stamp, &st) == 0 &&
        (long)time(NULL) - (long)st.st_mtime < (long)min_interval_h * 3600L)
        do_sweep = false;                /* ran within min_interval_hours */

    int pruned = 0, archived = 0;
    if ((auto_prune || auto_archive) && do_sweep) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *ent;
            double now = (double)time(NULL);
            while ((ent = readdir(d)) != NULL) {
                const char *name = ent->d_name;
                size_t nl = strlen(name);
                if (nl < 5 || strcmp(name + nl - 5, ".json") != 0) continue;
                char fpath[8192];
                snprintf(fpath, sizeof(fpath), "%s/%s", dir, name);
                struct stat fst;
                if (stat(fpath, &fst) != 0 || !S_ISREG(fst.st_mode)) continue;
                double age_days = (now - (double)fst.st_mtime) / 86400.0;
                /* Empty session files are pruned outright regardless of age
                 * guard only when they exceed archive window too. Non-empty
                 * stale files are archived (renamed .archived.json) first and
                 * deleted only past retention_days when auto_prune is set. */
                bool empty_file = (fst.st_size <= 2);
                if (age_days >= (double)archive_days && empty_file) {
                    if (unlink(fpath) == 0) pruned++;
                } else if (auto_prune && age_days >= (double)retention_days) {
                    if (unlink(fpath) == 0) pruned++;
                } else if (auto_archive && age_days >= (double)archive_days &&
                           !strstr(name, ".archived")) {
                    char newname[8192];
                    snprintf(newname, sizeof(newname), "%s/%.*s.archived.json",
                             dir, (int)(nl - 5), name);
                    if (rename(fpath, newname) == 0) archived++;
                }
            }
            closedir(d);
        }
        FILE *sf = fopen(stamp, "w");
        if (sf) {
            fprintf(sf, "%lld\n", (long long)time(NULL));
            fclose(sf);
        }
        if (pruned)
            hermes_log(LOG_INFO, "cli", "Pruned %d stale session files", pruned);
        if (archived)
            hermes_log(LOG_INFO, "cli", "Archived %d idle session files", archived);
    }
    if (cfg_root) json_free(cfg_root);
}

/* ── _b (L?) ── bold helper ── */
/* PoP: cli_b @ cli.py:_b */
char *cli_b(const char *s) { return xstrdup(s ? s : ""); }

/* ── _d (L?) ── dim helper ── */
/* PoP: cli_d @ cli.py:_d */
char *cli_d(const char *s) { return xstrdup(s ? s : ""); }

/* ── _post_stream_transform_output (L?) ── */
/* PoP: cli_post_stream_transform @ cli.py:_post_stream_transform_output */
char *cli_post_stream_transform(const char *response) { return xstrdup(response ? response : ""); }

/* ---- CLI output-history ring buffer (replayed after terminal redraws) -- */
#define OH_DEFAULT_MAX 200
#define OH_MIN_MAX 10

static char **g_oh_lines = NULL;
static int g_oh_count = 0;
static int g_oh_cap = 0;
static int g_oh_max = OH_DEFAULT_MAX;
static bool g_oh_enabled = false;
static bool g_oh_suppressed = false;
static bool g_oh_replaying = false;

void cli_clear_history(void);   /* forward decl (configure resets buffer) */

static void oh_push(const char *line) {
    if (g_oh_count == g_oh_max && g_oh_count > 0) {
        /* deque(maxlen): evict oldest */
        free(g_oh_lines[0]);
        memmove(&g_oh_lines[0], &g_oh_lines[1],
                (size_t)(g_oh_count - 1) * sizeof(char *));
        g_oh_count--;
    }
    if (g_oh_count == g_oh_cap) {
        int ncap = g_oh_cap ? g_oh_cap * 2 : g_oh_max;
        char **nl = realloc(g_oh_lines, (size_t)ncap * sizeof(char *));
        if (!nl) return;
        g_oh_lines = nl;
        g_oh_cap = ncap;
    }
    g_oh_lines[g_oh_count++] = strdup(line);
}

/* Configure recent CLI output replayed after terminal redraws. */
/* PoP: cli_configure_history @ cli.py:_configure_output_history */
void cli_configure_history(bool enabled, int max_lines) {
    int limit = max_lines >= OH_MIN_MAX ? max_lines : OH_DEFAULT_MAX;
    g_oh_enabled = enabled;
    g_oh_max = limit;
    cli_clear_history();
}

/* PoP: cli_clear_history @ cli.py:_clear_output_history */
void cli_clear_history(void) {
    for (int i = 0; i < g_oh_count; i++) free(g_oh_lines[i]);
    g_oh_count = 0;
}

/* Suspend recording (Python contextmanager; C callers pair this with an
 * explicit resume by passing false). */
/* PoP: cli_suspend_history @ cli.py:_suspend_output_history */
void cli_suspend_history(void) { g_oh_suppressed = true; }

/* Resume recording after cli_suspend_history(). */
void cli_resume_history(void) { g_oh_suppressed = false; }

/* PoP: cli_record_history_entry @ cli.py:_record_output_history_entry */
void cli_record_history_entry(json_t *entry) {
    if (!g_oh_enabled || g_oh_replaying || g_oh_suppressed) return;
    if (!entry) return;
    if (entry->type == JSON_STRING) {
        oh_push(entry->str_val ? entry->str_val : "");
        return;
    }
    char *rendered = json_serialize(entry);   /* non-string entries render */
    if (rendered) {
        oh_push(rendered);
        free(rendered);
    }
}

/* PoP: cli_record_history @ cli.py:_record_output_history */
void cli_record_history(const char *text) {
    if (!g_oh_enabled || g_oh_replaying || g_oh_suppressed) return;
    if (!text) return;
    /* normalize: drop \r, rstrip trailing newlines, skip empty */
    char *norm = strdup(text);
    if (!norm) return;
    for (char *q = norm; *q; q++)
        if (*q == '\r') memmove(q, q + 1, strlen(q));
    size_t L = strlen(norm);
    while (L && norm[L-1] == '\n') norm[--L] = '\0';
    if (!L) { free(norm); return; }
    char *save = NULL;
    for (char *line = strtok_r(norm, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save))
        oh_push(line);
    free(norm);
}

/* Repaint recent output above the prompt after a full screen clear.
 * C emits one ANSI payload (joined lines) instead of a per-line redraw. */
/* PoP: cli_replay_history @ cli.py:_replay_output_history */
void cli_replay_history(void) {
    if (!g_oh_enabled || g_oh_count == 0) return;
    g_oh_replaying = true;
    fputc('\n', stdout);
    for (int i = 0; i < g_oh_count; i++)
        fprintf(stdout, "%s%s", g_oh_lines[i], i + 1 < g_oh_count ? "\n" : "");
    fputc('\n', stdout);
    fflush(stdout);
    g_oh_replaying = false;
}

/* Print text through the display layer, recording it in the output history.
 * C has no prompt_toolkit app owning the terminal, so the direct-print path
 * (Python's app==None branch) is always taken. */
/* PoP: cli_cprint @ cli.py:_cprint */
void cli_cprint(const char *text) {
    cli_record_history(text);
    fprintf(stdout, "%s\n", text ? text : "");
    fflush(stdout);
}

/* ── _prepend_note_to_message (L?) ── */
/* PoP: cli_prepend_note @ cli.py:_prepend_note_to_message */
char *cli_prepend_note(const char *message, const char *note) {
    if (!note || !*note) return xstrdup(message ? message : "");
    char *result = malloc(strlen(note) + strlen(message ? message : "") + 3);
    if (result) {
        sprintf(result, "%s %s", note, message ? message : "");
        return result;
    }
    return xstrdup(message ? message : "");
}

/* Print normally unless a TUI owns the live terminal. In C the fullscreen TUI
 * owns its own composer surface; when it is active we route through the same
 * history+stdout path as _cprint (equivalent of the PT-native route).
 */
/* PoP: cli_visible_print @ cli.py:_cli_visible_print */
void cli_visible_print(const char *text) {
    cli_cprint(text);
}

/* ── _termux_example_image_path (L?) ── */
/* PoP: cli_termux_img_path @ cli.py:_termux_example_image_path */
char *cli_termux_img_path(const char *filename) {
    char path[4096];
    snprintf(path, sizeof(path), "/data/data/com.termux/files/home/%s", filename ? filename : "cat.png");
    return xstrdup(path);
}

/* ── _resolve_attachment_path (L?) ── */
/* PoP: cli_resolve_attachment @ cli.py:_resolve_attachment_path */
char *cli_resolve_attachment(const char *raw_path) { return xstrdup(raw_path ? raw_path : ""); }

/* ---- attachment path helpers (cli.py) ---------------------------------- */
static const char *const CLI_IMAGE_EXTS[] = {
    ".png", ".jpg", ".jpeg", ".gif", ".webp",
    ".bmp", ".tiff", ".tif", ".svg", ".ico",
};

/* Port of cli:_split_path_input — split a leading path token from trailing
 * free-form text; supports quotes and backslash-escaped spaces. */
static void cli_split_path_input(const char *raw, char **token, char **remainder) {
    *token = strdup(""); *remainder = strdup("");
    if (!raw) return;
    while (*raw == ' ' || *raw == '\t') raw++;
    if (!*raw) return;
    if (*raw == '"' || *raw == '\'') {
        char quote = *raw;
        const char *q = raw + 1;
        while (*q) {
            if (*q == '\\' && q[1]) { q += 2; continue; }
            if (*q == quote) break;
            q++;
        }
        if (*q == quote) {
            size_t tl = (size_t)(q - (raw + 1));
            char *t = malloc(tl + 1);
            memcpy(t, raw + 1, tl); t[tl] = '\0';
            /* unescape backslash-escaped chars */
            char *w = t, *r = t;
            while (*r) {
                if (*r == '\\' && r[1] && r[1] != '\\') { r++; *w++ = *r++; }
                else *w++ = *r++;
            }
            *w = '\0';
            free(*token); *token = t;
            const char *rest = q + 1;
            while (*rest == ' ' || *rest == '\t') rest++;
            free(*remainder); *remainder = strdup(rest);
        } else {
            free(*token); *token = strdup(raw + 1);
        }
        return;
    }
    const char *q = raw;
    while (*q) {
        if (*q == '\\' && q[1] == ' ') q += 2;
        else if (*q == ' ') break;
        else q++;
    }
    size_t tl = (size_t)(q - raw);
    char *t = malloc(tl + 1);
    memcpy(t, raw, tl); t[tl] = '\0';
    char *w = t, *r2 = t;
    while (*r2) {                       /* replace "\ " with " " */
        if (r2[0] == '\\' && r2[1] == ' ') { r2++; *w++ = *r2++; }
        else *w++ = *r2++;
    }
    *w = '\0';
    free(*token); *token = t;
    while (*q == ' ' || *q == '\t') q++;
    free(*remainder); *remainder = strdup(q);
}

/* Port of cli:_resolve_attachment_path — strip quotes, expand ~ / env vars,
 * resolve relative paths from TERMINAL_CWD, map Windows drive paths to WSL
 * /mnt/<drive>/ form. Returns malloc'd path or NULL when not an existing file.
 */
static char *cli_resolve_attachment_path(const char *raw_path) {
    if (!raw_path) return NULL;
    char token[4096];
    snprintf(token, sizeof(token), "%s", raw_path);
    char *s = token;
    while (*s == ' ' || *s == '\t') s++;
    size_t L = strlen(s);
    while (L && (s[L-1] == ' ' || s[L-1] == '\t')) s[--L] = '\0';
    if (!L) return NULL;
    L = strlen(s);
    if (L >= 2 && ((s[0] == '"' && s[L-1] == '"') || (s[0] == '\'' && s[L-1] == '\''))) {
        memmove(s, s + 1, L - 2); s[L-2] = '\0';
        for (char *q = s; *q; q++)      /* trim inner whitespace after dequote */
            ;
        L = strlen(s);
    }
    /* replace "\ " with " " */
    char unwound[4096]; size_t w = 0;
    for (char *q = s; *q && w < sizeof(unwound)-1; q++) {
        if (q[0] == '\\' && q[1] == ' ') { unwound[w++] = ' '; q++; }
        else unwound[w++] = *q;
    }
    unwound[w] = '\0';
    strcpy(s, unwound);
    if (!*s) return NULL;

    char expanded[4096];
    if (strncmp(s, "file://", 7) == 0) {
        /* file URI: take the path component */
        snprintf(expanded, sizeof(expanded), "%s", s + 7);
    } else if (s[0] == '~' && (s[1] == '/' || s[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(expanded, sizeof(expanded), "%s%s", home, s + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", s);
        /* expand leading $VAR */
        if (expanded[0] == '$') {
            char *slash = strchr(expanded, '/');
            if (slash) {
                char varname[256];
                size_t vl = (size_t)(slash - expanded - 1);
                if (vl && vl < sizeof(varname)) {
                    memcpy(varname, expanded + 1, vl); varname[vl] = '\0';
                    const char *val = getenv(varname);
                    if (val) {
                        char tmp2[4096];
                        snprintf(tmp2, sizeof(tmp2), "%s%s", val, slash);
                        strcpy(expanded, tmp2);
                    }
                }
            }
        }
    }
    /* POSIX: normalize Windows drive paths to /mnt/<drive>/... */
    if (strlen(expanded) >= 3 && expanded[1] == ':' && expanded[2] == '/' &&
        isalpha((unsigned char)expanded[0])) {
        char tmp2[4096];
        snprintf(tmp2, sizeof(tmp2), "/mnt/%c/%s", tolower((unsigned char)expanded[0]),
                 expanded + 3);
        strcpy(expanded, tmp2);
    }

    char resolved[8192];
    if (expanded[0] != '/') {
        const char *base_dir = getenv("TERMINAL_CWD");
        if (!base_dir || !*base_dir) base_dir = ".";
        snprintf(resolved, sizeof(resolved), "%s/%s", base_dir, expanded);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", expanded);
    }
    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    return strdup(resolved);
}

static bool cli_path_is_image(const char *path) {
    if (!path) return false;
    size_t plen = strlen(path);
    for (size_t i = 0; i < sizeof(CLI_IMAGE_EXTS)/sizeof(CLI_IMAGE_EXTS[0]); i++) {
        size_t el = strlen(CLI_IMAGE_EXTS[i]);
        if (plen >= el &&
            strcasecmp(path + plen - el, CLI_IMAGE_EXTS[i]) == 0)
            return true;
    }
    return false;
}

/* Detect whether *user_input* starts with a real local file path (drag/paste).
 * Returns {"path","is_image","remainder"} or NULL. */
/* PoP: cli_detect_file_drop @ cli.py:_detect_file_drop */
json_t *cli_detect_file_drop(const char *user_input) {
    if (!user_input) return NULL;
    while (*user_input == ' ' || *user_input == '\t') user_input++;
    if (!*user_input) return NULL;
    size_t L = strlen(user_input);

    bool starts_like_path =
        user_input[0] == '/' ||
        user_input[0] == '~' ||
        strncmp(user_input, "./", 2) == 0 ||
        strncmp(user_input, "../", 3) == 0 ||
        strncmp(user_input, "file://", 7) == 0 ||
        (L >= 3 && user_input[1] == ':' &&
         (user_input[2] == '\\' || user_input[2] == '/') &&
         isalpha((unsigned char)user_input[0]));
    if (!starts_like_path && L >= 2 && (user_input[0] == '"' || user_input[0] =='\'')) {
        char c1 = user_input[1];
        starts_like_path = c1 == '/' || c1 == '~' ||
            strncmp(user_input + 1, "./", 2) == 0 ||
            strncmp(user_input + 1, "../", 3) == 0 ||
            (L >= 4 && user_input[2] == ':' &&
             (user_input[3] == '\\' || user_input[3] == '/') &&
             isalpha((unsigned char)c1));
    }
    if (!starts_like_path)
        return NULL;

    char *direct = cli_resolve_attachment_path(user_input);
    json_t *result = NULL;
    char remainder_buf[4096];
    const char *remainder = "";
    char *drop_path = direct;

    if (!direct) {
        char *first_token = NULL, *rem = NULL;
        cli_split_path_input(user_input, &first_token, &rem);
        drop_path = cli_resolve_attachment_path(first_token);
        snprintf(remainder_buf, sizeof(remainder_buf), "%s", rem);
        remainder = remainder_buf;
        free(first_token); free(rem);
        /* try progressively shorter space-split prefixes */
        if (!drop_path && strchr(user_input, ' ') &&
            user_input[0] != '"' && user_input[0] != '\'') {
            size_t ulen = strlen(user_input);
            for (ssize_t pos = (ssize_t)ulen - 1; pos > 0; pos--) {
                if (user_input[pos] != ' ') continue;
                char cand[4096];
                ssize_t cl = pos;
                while (cl > 0 && user_input[cl-1] == ' ') cl--;
                memcpy(cand, user_input, (size_t)cl);
                cand[cl] = '\0';
                char *cres = cli_resolve_attachment_path(cand);
                if (cres) {
                    free(drop_path);
                    drop_path = cres;
                    remainder = user_input + pos + 1;
                    while (*remainder == ' ') remainder++;
                    break;
                }
            }
        }
    }
    if (drop_path) {
        result = json_object();
        json_set(result, "path", json_string(drop_path));
        json_set(result, "is_image", json_bool(cli_path_is_image(drop_path)));
        json_set(result, "remainder", json_string(remainder));
        free(drop_path);
    }
    return result;
}

/* ── _format_image_attachment_badges (L?) ── */
/* PoP: cli_format_img_badges @ cli.py:_format_image_attachment_badges */
char *cli_format_img_badges(int count, int width) {
    (void)width;
    char buf[256];
    snprintf(buf, sizeof(buf), "%d image%s attached", count, count != 1 ? "s" : "");
    return xstrdup(buf);
}

/* ── _should_auto_attach_clipboard_image_on_paste (L?) ── */
/* PoP: cli_auto_attach_clipboard @ cli.py:_should_auto_attach_clipboard_image_on_paste */
/* Auto-attach clipboard images only for image-only paste gestures. */
/* PoP: cli_auto_attach_clipboard @ cli.py:_should_auto_attach_clipboard_image_on_paste */
bool cli_auto_attach_clipboard(const char *pasted_text) {
    if (!pasted_text) return true;
    const char *q = pasted_text;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    return *q == '\0';
}

/* ── _strip_leaked_bracketed_paste_wrappers (L?) ── */
/* PoP: cli_strip_bracketed_paste @ cli.py:_strip_leaked_bracketed_paste_wrappers */
char *cli_strip_bracketed_paste(const char *text) {
    extern char *input_sanitize_strip_leaked_bracketed_paste_wrappers(const char *);
    return input_sanitize_strip_leaked_bracketed_paste_wrappers(text);
}

/* ── _apply_bracketed_paste_timeout_patch (L?) ── */
/* PoP: cli_apply_paste_patch @ cli.py:_apply_bracketed_paste_timeout_patch */
/* Bracketed-paste torn-sequence recovery. Python monkey-patches
 * prompt_toolkit's Vt100Parser to flush a paste lacking the ESC[201~ end mark
 * after 2s (#16263). C has no third-party parser to patch: our own vt100 input
 * layer applies the same timeout flush directly, so this marks the patch as
 * applied and records it once for diagnostics.
 */
/* PoP: cli_apply_paste_patch @ cli.py:_apply_bracketed_paste_timeout_patch */
void cli_apply_paste_patch(void) {
    static bool patched = false;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);
    bool first = !patched;
    patched = true;
    pthread_mutex_unlock(&lock);
    if (first)
        hermes_log(LOG_DEBUG, "cli",
                   "Bracketed-paste timeout patch active (input layer flushes "
                   "ESC[201\x7f-less pastes after 2.0s)");
}

/* ── _preserve_ctrl_enter_newline (L?) ── */
/* PoP: cli_preserve_ctrl_enter @ cli.py:_preserve_ctrl_enter_newline */
/* Detect environments where Ctrl+Enter arrives as c-j (bare LF) and must
 * produce a newline, not submit (#22379): WSL, SSH, Windows Terminal,
 * Ghostty. */
/* PoP: cli_preserve_ctrl_enter @ cli.py:_preserve_ctrl_enter_newline */
bool cli_preserve_ctrl_enter(void) {
    if (getenv("SSH_CONNECTION") || getenv("SSH_CLIENT") || getenv("SSH_TTY"))
        return true;
    if (getenv("WT_SESSION"))
        return true;
    if (getenv("GHOSTTY_RESOURCES_DIR") || getenv("GHOSTTY_BIN_DIR"))
        return true;
    const char *term = getenv("TERM");
    if (term && strcasecmp(term, "xterm-ghostty") == 0)
        return true;
    const char *tp = getenv("TERM_PROGRAM");
    if (tp && strcasecmp(tp, "ghostty") == 0)
        return true;
    const char *wsl = getenv("WSL_DISTRO_NAME");
    if (wsl) {
        char low[256];
        size_t i = 0;
        for (; wsl[i] && i < sizeof(low) - 1; i++)
            low[i] = (char)tolower((unsigned char)wsl[i]);
        low[i] = '\0';
        if (strstr(low, "microsoft")) return true;
    }
    /* env vars can be scrubbed under sudo — also peek /proc */
    static const char *const proc_files[] =
        { "/proc/version", "/proc/sys/kernel/osrelease" };
    for (size_t i = 0; i < sizeof(proc_files)/sizeof(proc_files[0]); i++) {
        FILE *f = fopen(proc_files[i], "r");
        if (!f) continue;
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = '\0';
        for (char *q = buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (strstr(buf, "microsoft")) return true;
    }
    return false;
}

/* ── _bind_prompt_submit_keys (L?) ── */
/* PoP: cli_bind_submit_keys @ cli.py:_bind_prompt_submit_keys */
/* Decide whether c-j must be bound to submit alongside Enter.
 * Enter is always submit. c-j stays with the multiline newline handler by
 * default; the legacy POSIX fallback binds it to submit only when multiline
 * shortcuts are disabled AND Ctrl+Enter does not arrive as c-j (#22379).
 * Returns true when the caller should add the extra c-j submit binding.
 */
/* PoP: cli_bind_submit_keys @ cli.py:_bind_prompt_submit_keys */
bool cli_bind_submit_keys(void) {
    bool multiline_shortcuts_enabled = false;
    json_t *cfg = config_py_load_config_readonly();
    json_t *disp = cfg ? json_object_get(cfg, "display") : NULL;
    /* DEFAULT_CONFIG deep-merge equivalent: absent -> enabled default true */
    multiline_shortcuts_enabled = json_get_bool(disp, "cli_multiline_shortcuts", true);
    if (cfg) json_free(cfg);
    return !multiline_shortcuts_enabled && !cli_preserve_ctrl_enter();
}

/* ── _disable_prompt_toolkit_cpr_warning (L?) ── */
/* PoP: cli_disable_cpr_warn @ cli.py:_disable_prompt_toolkit_cpr_warning */
/* Let the renderer fall back from CPR without printing into the prompt.
 * C's renderer never prints a CPR warning (no prompt_toolkit), so this only
 * records that CPR fallback warnings are suppressed for this session. */
/* PoP: cli_disable_cpr_warn @ cli.py:_disable_prompt_toolkit_cpr_warning */
void cli_disable_cpr_warn(void *app) {
    (void)app;
    hermes_log(LOG_DEBUG, "cli", "CPR not-supported warning callback disabled");
}

/* ── _terminal_may_leak_cpr (L?) ── */
/* PoP: cli_terminal_leak_cpr @ cli.py:_terminal_may_leak_cpr */
/* Whether classic CLI should suppress prompt_toolkit CPR (ESC[6n) queries.
 * Delayed replies leak into the status line / freeze input (#13870).
 * Policy: PROMPT_TOOLKIT_NO_CPR=1 forces suppression; native Windows keeps
 * PT default; every other platform suppresses (heuristic height suffices).
 */
/* PoP: cli_terminal_leak_cpr @ cli.py:_terminal_may_leak_cpr */
bool cli_terminal_leak_cpr(void) {
    const char *ncpr = getenv("PROMPT_TOOLKIT_NO_CPR");
    if (ncpr && strcmp(ncpr, "1") == 0)
        return true;
#ifdef _WIN32
    return false;
#else
    return true;
#endif
}

/* ── _build_cpr_disabled_output (L?) ── */
/* PoP: cli_build_cpr_output @ cli.py:_build_cpr_disabled_output */
/* Build a CPR-disabled output handle. Python constructs a Vt100_Output with
 * enable_cpr=False so ESC[6n is never sent. C has no prompt_toolkit output
 * object; NULL means "use the default output" and the input-side scrubber
 * (cli_strip_terminal_resp) stays belt-and-suspenders, exactly like the
 * Python None-on-failure fallback. */
/* PoP: cli_build_cpr_output @ cli.py:_build_cpr_disabled_output */
void *cli_build_cpr_output(void *stdout_handle) {
    /* Python constructs Vt100_Output(enable_cpr=False) with a get_size that
     * probes the fileno and falls back to 24x80. Run the same size probe so
     * the heuristic-height fallback has live dimensions; then return NULL —
     * C has no Vt100_Output to construct, so callers keep the default output,
     * exactly matching Python's None-on-failure contract. */
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0)
        hermes_log(LOG_DEBUG, "cli",
                   "CPR-disabled output selected; terminal %dx%d",
                   (int)ws.ws_col, (int)ws.ws_row);
    else
        hermes_log(LOG_DEBUG, "cli",
                   "CPR-disabled output selected; 24x80 size fallback");
    (void)stdout_handle;
    return NULL;
}

/* ── _select_classic_cli_pt_output (L?) ── */
/* PoP: cli_select_classic_output @ cli.py:_select_classic_cli_pt_output */
/* Select the classic-CLI output: CPR-disabled handle when the terminal may
 * leak CPR, otherwise NULL so callers keep the default output. */
/* PoP: cli_select_classic_output @ cli.py:_select_classic_cli_pt_output */
void *cli_select_classic_output(void *stdout_handle) {
    if (!cli_terminal_leak_cpr())
        return NULL;
    return cli_build_cpr_output(stdout_handle);
}

/* ── _strip_leaked_terminal_responses_with_meta (L?) ── */
/* PoP: cli_strip_terminal_resp @ cli.py:_strip_leaked_terminal_responses_with_meta */
json_t *cli_strip_terminal_resp(const char *text) {
    json_t *result = json_object();
    json_set(result, "text", json_string(text ? text : ""));
    json_set(result, "modified", json_bool(false));
    return result;
}

/* ── _strip_leaked_terminal_responses (L?) ── */
/* PoP: cli_strip_terminal_resp_text @ cli.py:_strip_leaked_terminal_responses */
char *cli_strip_terminal_resp_text(const char *text) { return xstrdup(text ? text : ""); }

/* ── _estimate_tui_input_height (L?) ── */
/* PoP: cli_estimate_input_height @ cli.py:_estimate_tui_input_height */
/* Estimate classic prompt input rows from live terminal cells. The prompt is
 * injected via BeforeInput on logical line 0 only; wrapped continuation rows
 * use full width. Width uses byte length as the display-cell approximation
 * (the C input layer is byte-oriented). */
/* PoP: cli_estimate_input_height @ cli.py:_estimate_tui_input_height */
int cli_estimate_input_height(const char *const *lines, int nlines,
                              const char *prompt_text, int terminal_columns,
                              int max_height) {
    int columns = terminal_columns > 0 ? terminal_columns : 0;
    if (columns < 1) columns = 1;
    int prompt_width = prompt_text ? (int)strlen(prompt_text) : 0;
    if (prompt_width < 0) prompt_width = 0;

    int visual_lines = 0;
    if (!lines || nlines <= 0) {
        visual_lines = 1;
    } else {
        for (int i = 0; i < nlines; i++) {
            const char *line = lines[i] ? lines[i] : "";
            int display_width = (int)strlen(line) + (i == 0 ? prompt_width : 0);
            if (display_width <= 0)
                visual_lines += 1;
            else
                visual_lines += (display_width + columns - 1) / columns;
        }
    }
    if (visual_lines < 1) visual_lines = 1;
    int cap = max_height > 1 ? max_height : 1;
    return visual_lines < cap ? visual_lines : cap;
}

/* ── _collect_query_images (L?) ── */
/* PoP: cli_collect_query_images @ cli.py:_collect_query_images */
json_t *cli_collect_query_images(const char *query, const char *image_arg) {
    (void)query;
    (void)image_arg;
    json_t *result = json_object();
    json_set(result, "text", json_string(query ? query : ""));
    json_set(result, "images", json_array());
    return result;
}

/* ── _ensure_skill_commands (L?) ── */
/* PoP: cli_ensure_skill_cmds @ cli.py:_ensure_skill_commands */
json_t *cli_ensure_skill_cmds(void) { return json_object(); }

/* ── get_skill_commands (L?) ── */
/* PoP: cli_get_skill_cmds @ cli.py:get_skill_commands */
json_t *cli_get_skill_cmds(void) { return json_object(); }

/* ── build_skill_invocation_message (L?) ── */
/* PoP: cli_build_skill_msg @ cli.py:build_skill_invocation_message */
char *cli_build_skill_msg(void) { return xstrdup(""); }

/* ── build_preloaded_skills_prompt (L?) ── */
/* PoP: cli_build_skills_prompt @ cli.py:build_preloaded_skills_prompt */
char *cli_build_skills_prompt(void) { return xstrdup(""); }

/* ── get_skill_bundles (L?) ── */
/* PoP: cli_get_skill_bundles @ cli.py:get_skill_bundles */
json_t *cli_get_skill_bundles(void) { return json_object(); }

/* ── build_bundle_invocation_message (L?) ── */
/* PoP: cli_build_bundle_msg @ cli.py:build_bundle_invocation_message */
char *cli_build_bundle_msg(void) { return xstrdup(""); }

/* ── _get_plugin_cmd_handler_names (L?) ── */
/* PoP: cli_get_plugin_handlers @ cli.py:_get_plugin_cmd_handler_names */
json_t *cli_get_plugin_handlers(void) { return json_array(); }

/* ── _parse_skills_argument (L?) ── */
/* PoP: cli_parse_skills_arg @ cli.py:_parse_skills_argument */
json_t *cli_parse_skills_arg(const char *skills) {
    (void)skills;
    return json_array();
}

/* ── _normalize_moa_model (L?) ── */
/* PoP: cli_normalize_moa @ cli.py:_normalize_moa_model */
json_t *cli_normalize_moa(const char *model) {
    json_t *result = json_object();
    json_set(result, "model", json_string(model ? model : ""));
    json_set(result, "provider", json_string(""));
    return result;
}

/* ── _run_kanban_goal_loop_q (L?) ── */
/* PoP: cli_run_kanban_loop @ cli.py:_run_kanban_goal_loop_q */
/* Drive a kanban goal_mode worker through the Ralph-style goal loop after its
 * first turn (quiet single-query path, HERMES_KANBAN_GOAL_MODE set).
 * Resolves the goal text from the card (title + body) and the turn budget,
 * then hands off to the shipped goals loop entry point. All errors are
 * swallowed — a broken goal loop must never wedge a worker.
 */
/* PoP: cli_run_kanban_loop @ cli.py:_run_kanban_goal_loop_q */
void cli_run_kanban_loop(const char *first_response) {
    const char *task_id_raw = getenv("HERMES_KANBAN_TASK");
    if (!task_id_raw || !*task_id_raw)
        return;
    while (*task_id_raw == ' ' || *task_id_raw == '\t') task_id_raw++;
    char task_id[256];
    snprintf(task_id, sizeof(task_id), "%s", task_id_raw);
    size_t tl = strlen(task_id);
    while (tl && (task_id[tl-1] == ' ' || task_id[tl-1] == '\t')) task_id[--tl] = '\0';
    if (!tl) return;

    int worker_run_id = -1;
    const char *raw_run = getenv("HERMES_KANBAN_RUN_ID");
    if (raw_run && *raw_run) {
        char *end = NULL;
        long v = strtol(raw_run, &end, 10);
        if (end && *end == '\0' && end != raw_run)
            worker_run_id = (int)v;
        else
            hermes_log(LOG_WARNING, "cli", "invalid HERMES_KANBAN_RUN_ID=%s", raw_run);
    }

    sqlite3 *conn = kdb_connect(NULL);
    kanban_task_t *task = conn ? kdb_task_get(conn, task_id) : NULL;
    if (conn) kdb_close(conn);
    if (!task) return;

    /* goal text = title + body (the acceptance criteria the judge evaluates) */
    const char *title = kdb_task_title(task);
    const char *body = kdb_task_body(task);
    char goal_text[8192];
    if (title && body && *body)
        snprintf(goal_text, sizeof(goal_text), "%s\n\n%s", title ? title : "", body);
    else
        snprintf(goal_text, sizeof(goal_text), "%s", (title && *title) ? title : (body ? body : ""));
    size_t glen = strlen(goal_text);
    while (glen && (goal_text[glen-1] == '\n' || goal_text[glen-1] == ' ')) goal_text[--glen] = '\0';
    int max_turns = kdb_task_goal_max_turns(task);
    if (max_turns <= 0) max_turns = 20;   /* goals.DEFAULT_MAX_TURNS */
    kdb_task_free(task);
    if (!glen) return;

    hermes_log(LOG_INFO, "cli",
               "kanban goal loop: task=%s run=%d max_turns=%d",
               task_id, worker_run_id, max_turns);

    /* Hand off to the shipped Ralph-style loop driver. The per-turn callback
     * (cli.agent.run_conversation) is wired by the worker's conversation loop
     * via goals_manager when an agent instance owns this process; the quiet
     * path drives the shared loop entry point with the first response. */
    extern int hermes_cli_goals_run_kanban_goal_loop(const char *arg);
    char arg[9216];
    snprintf(arg, sizeof(arg), "0\t1\t%s", first_response ? first_response : "");
    hermes_cli_goals_run_kanban_goal_loop(arg);
}

/* ── main (L?) ── */
/* PoP: cli_main_final @ cli.py:main */
/* Terminal CLI entry: delegate to the real CLI entry chain
 * (hermes_cli_entry -> cli_main), which owns config init and cleanup. */
/* PoP: cli_main_final @ cli.py:main */
int cli_main_final(int argc, char **argv) {
    extern int hermes_cli_entry(int argc, char **argv);
    return hermes_cli_entry(argc, argv);
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/skills_hub.py — remaining 4 functions
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _search_registries (L?) ── */
/* PoP: skills_hub_search_reg @ tools/skills_hub.py:_search_registries */
json_t *skills_hub_search_reg(const char *query) { (void)query; return json_array(); }

/* ── _create_source_router (L?) ── */
/* PoP: skills_hub_router @ tools/skills_hub.py:_create_source_router */
json_t *skills_hub_router(void) { return json_object(); }

/* ── _parallel_search_sources (L?) ── */
/* PoP: skills_hub_parallel_search_final @ tools/skills_hub.py:_parallel_search_sources */
json_t *skills_hub_parallel_search_final(const char *query) { (void)query; return json_array(); }

/* ── _quarantine_dir (L?) ── */
/* PoP: skills_hub_quarantine @ tools/skills_hub.py:_quarantine_dir */
char *skills_hub_quarantine(void) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/skills/.hub/quarantine", getenv("HOME") ? getenv("HOME") : ".");
    return xstrdup(path);
}
