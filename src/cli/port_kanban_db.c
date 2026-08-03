/*
 * port_kanban_db.c — Port of Python hermes_cli/kanban_db.py
 *
 * Git operations and path utilities for the Kanban board.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "kanban_db.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <strings.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>
#include <ctype.h>

/*
 * _git_toplevel — Return the git toplevel containing path, or NULL if not in a repo.
 *
 * Python: def _git_toplevel(path: Path) -> Optional[Path]:
 *   result = subprocess.run(["git", "-C", str(path), "rev-parse", "--show-toplevel"], ...)
 *   if result.returncode != 0: return None
 *   out = (result.stdout or "").strip()
 *   if not out: return None
 *   return Path(out)
 */
/* Port of Python: _git_toplevel */
const char* _git_toplevel(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--show-toplevel", NULL);
        _exit(1);
    }

    /* Parent */
    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    /* Strip trailing newline */
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';

    if (buf[0] == '\0') return NULL;

    /* Return a strdup'd copy */
    return strdup(buf);
}

/*
 * _git_branch_exists — Check if a branch exists in a git repo.
 *
 * Python: def _git_branch_exists(repo_root: Path, branch_name: str) -> bool:
 *   result = subprocess.run(["git", "-C", repo_root, "show-ref", "--verify", f"refs/heads/{branch_name}"], ...)
 *   return result.returncode == 0
 */
/* Port of Python: _git_branch_exists */
bool _git_branch_exists(const char* repo_root, const char* branch_name)
{
    if (!repo_root || !branch_name) return false;

    int pipefd[2];
    if (pipe(pipefd) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char ref[512];
        snprintf(ref, sizeof(ref), "refs/heads/%s", branch_name);
        execlp("git", "git", "-C", repo_root, "show-ref", "--verify", ref, NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[256];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/*
 * _git_common_dir — Return the git common dir for a path.
 *
 * Python: def _git_common_dir(path: Path) -> Optional[Path]:
 *   result = subprocess.run(["git", "-C", path, "rev-parse", "--path-format=absolute", "--git-common-dir"], ...)
 */
/* Port of Python: _git_common_dir */
const char* _git_common_dir(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--path-format=absolute", "--git-common-dir", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _git_dir — Return the git dir for a path.
 */
/* Port of Python: _git_dir */
const char* _git_dir(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "rev-parse", "--path-format=absolute", "--git-dir", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _git_current_branch — Return the current branch name.
 *
 * Python: def _git_current_branch(path: Path) -> Optional[str]:
 *   result = subprocess.run(["git", "-C", path, "branch", "--show-current"], ...)
 */
/* Port of Python: _git_current_branch */
const char* _git_current_branch(const char* path)
{
    if (!path || !path[0]) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", path, "branch", "--show-current", NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[1024];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }
    buf[n] = '\0';

    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0') return NULL;

    return strdup(buf);
}

/*
 * _nearest_existing_path — Walk up the directory tree to find an existing path.
 *
 * Python: def _nearest_existing_path(path: Path) -> Path:
 *   current = path
 *   while not current.exists() and current != current.parent:
 *       current = current.parent
 *   return current
 */
/* Port of Python: _nearest_existing_path */
const char* _nearest_existing_path(const char* path)
{
    if (!path || !path[0]) return ".";

    /* Try the path itself first */
    struct stat st;
    if (stat(path, &st) == 0) {
        return strdup(path);
    }

    /* Walk up the tree */
    char* current = strdup(path);
    if (!current) return ".";

    while (1) {
        char* parent = dirname(current);
        if (!parent) break;
        if (strcmp(parent, current) == 0) break; /* Reached root */

        if (stat(parent, &st) == 0) {
            free(current);
            return strdup(parent);
        }

        char* tmp = strdup(parent);
        free(current);
        current = tmp;
        if (!current) break;
    }

    free(current);
    return ".";
}

/* ===========================================================================
 *  Kanban path / config helpers — ported from hermes_cli/kanban_db.py
 *  These were REAL_GAP. Faithful re-implementations.
 * =========================================================================== */

#define KB_DEFAULT_CLAIM_TTL_SECONDS (15 * 60)
#define KB_DEFAULT_CRASH_GRACE_SECONDS 30
#define KB_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS 300
#define KB_DEFAULT_BOARD "default"

/* Resolve the shared Hermes root anchoring the kanban board. */
static void kanban_root_dir(char *out, size_t sz)
{
    const char *override = getenv("HERMES_KANBAN_HOME");
    if (override && override[0]) { snprintf(out, sz, "%s", override); return; }
    const char *home = getenv("HERMES_HOME");
    if (home && home[0]) { snprintf(out, sz, "%s", home); return; }
    const char *sl = getenv("SLERMES_HOME");
    if (sl && sl[0]) { snprintf(out, sz, "%s", sl); return; }
    const char *h = getenv("HOME");
    snprintf(out, sz, "%s/.hermes", h ? h : ".");
}

/*
 * PoP: _resolve_claim_ttl_seconds @ hermes_cli/kanban_db.py:_resolve_claim_ttl_seconds */
int resolve_claim_ttl_seconds(int ttl_seconds)
{
    if (ttl_seconds >= 0) return ttl_seconds < 1 ? 1 : ttl_seconds; /* explicit call-site value wins */
    const char *raw = getenv("HERMES_KANBAN_CLAIM_TTL_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed > 0) return (int)parsed;
    }
    return KB_DEFAULT_CLAIM_TTL_SECONDS;
}

/*
 * PoP: _resolve_crash_grace_seconds @ hermes_cli/kanban_db.py:_resolve_crash_grace_seconds */
int resolve_crash_grace_seconds(void)
{
    const char *raw = getenv("HERMES_KANBAN_CRASH_GRACE_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed >= 0) return (int)parsed;
    }
    return KB_DEFAULT_CRASH_GRACE_SECONDS;
}

/*
 * PoP: _resolve_rate_limit_cooldown_seconds @ hermes_cli/kanban_db.py:_resolve_rate_limit_cooldown_seconds */
int resolve_rate_limit_cooldown_seconds(void)
{
    const char *raw = getenv("HERMES_KANBAN_RATE_LIMIT_COOLDOWN_SECONDS");
    if (raw) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed >= 0) return (int)parsed;
    }
    return KB_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS;
}

/*
 * PoP: _relative_age @ hermes_cli/kanban_db.py:_relative_age
 * Returns malloc'd coarse age string. Caller frees. */
char *relative_age(long ts, long now)
{
    if (ts <= 0) return strdup("");
    if (now <= 0) now = (long)time(NULL);
    long delta = now - ts;
    if (delta < 0) return strdup("just now");
    if (delta < 60) return strdup("just now");
    if (delta < 3600) { char b[32]; snprintf(b, sizeof(b), "%ldm ago", delta/60); return strdup(b); }
    if (delta < 86400) { char b[32]; snprintf(b, sizeof(b), "%ldh ago", delta/3600); return strdup(b); }
    { char b[32]; snprintf(b, sizeof(b), "%ldd ago", delta/86400); return strdup(b); }
}

/*
 * PoP: _normalize_board_slug @ hermes_cli/kanban_db.py:_normalize_board_slug
 * Returns malloc'd normalized slug or NULL (invalid/empty). Caller frees. */
char *normalize_board_slug(const char *slug)
{
    if (!slug) return NULL;
    char s[128];
    size_t k = 0;
    for (size_t i = 0; slug[i] && k < sizeof(s)-1; i++) {
        char c = (char)tolower((unsigned char)slug[i]);
        if (c == ' ' || c == '\t') continue; /* strip whitespace */
        s[k++] = c;
    }
    s[k] = '\0';
    if (!s[0]) return NULL;
    /* validate: ^[a-z0-9][a-z0-9\-_]{0,63}$ */
    if (!(isalnum((unsigned char)s[0]))) return NULL;
    for (size_t i = 1; i < strlen(s); i++)
        if (!(isalnum((unsigned char)s[i]) || s[i]=='-' || s[i]=='_')) return NULL;
    if (strlen(s) > 64) return NULL;
    return strdup(s);
}

/*
 * PoP: kanban_home @ hermes_cli/kanban_db.py:kanban_home
 * Returns malloc'd root dir. Caller frees. */
char *kanban_home(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    return strdup(root);
}

/*
 * PoP: boards_root @ hermes_cli/kanban_db.py:boards_root */
char *kanban_boards_root(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/boards", root);
    return strdup(out);
}

/*
 * PoP: current_board_path @ hermes_cli/kanban_db.py:current_board_path */
char *kanban_current_board_path(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/current", root);
    return strdup(out);
}

/* board_exists: directory under boards_root, or the "default" sentinel. */
static int kanban_board_exists(const char *slug)
{
    if (!slug) return 0;
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) return 1;
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/kanban/boards/%s", root, slug);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/*
 * PoP: get_current_board @ hermes_cli/kanban_db.py:get_current_board
 * Returns malloc'd active board slug (never NULL). Caller frees. */
char *get_current_board(void)
{
    /* 1. HERMES_KANBAN_BOARD env (must be valid + exist) */
    const char *env = getenv("HERMES_KANBAN_BOARD");
    if (env && env[0]) {
        char *normed = normalize_board_slug(env);
        if (normed) {
            if (kanban_board_exists(normed)) return normed;
            free(normed);
        }
    }
    /* 2. <root>/kanban/current on disk */
    char *cp = kanban_current_board_path();
    FILE *f = fopen(cp, "r");
    free(cp);
    if (f) {
        char buf[128];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1]=='\n'||buf[n-1]=='\r'||buf[n-1]==' '||buf[n-1]=='\t')) buf[--n]=0;
            char *normed = normalize_board_slug(buf);
            if (normed) {
                if (kanban_board_exists(normed)) return normed;
                free(normed);
            }
            return strdup(KB_DEFAULT_BOARD);
        }
        fclose(f);
    }
    /* 3. default */
    return strdup(KB_DEFAULT_BOARD);
}

/*
 * PoP: set_current_board @ hermes_cli/kanban_db.py:set_current_board
 * Writes the slug to <root>/kanban/current. Returns 0 on success. */
int set_current_board(const char *slug)
{
    char *normed = normalize_board_slug(slug);
    if (!normed) return -1;
    char *cp = kanban_current_board_path();
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", cp);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    mkdir(dir, 0755);
    FILE *f = fopen(cp, "w");
    free(cp);
    if (!f) { free(normed); return -1; }
    fprintf(f, "%s\n", normed);
    fclose(f);
    free(normed);
    return 0;
}

/* ===========================================================================
 *  Additional Kanban path / string / sqlite helpers
 *  Ported from hermes_cli/kanban_db.py (faithful; reuse kanban_home,
 *  kanban_boards_root, normalize_board_slug, kanban_board_exists,
 *  get_current_board already ported above).
 * =========================================================================== */

/*
 * PoP: board_dir @ hermes_cli/kanban_db.py:board_dir
 * Returns malloc'd "<root>/kanban/boards/<slug>" (slug defaulted). Caller frees. */
char *board_dir(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *br = kanban_boards_root();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/%s", br, slug);
    free(br);
    free(slug);
    return strdup(out);
}

/*
 * PoP: board_exists @ hermes_cli/kanban_db.py:board_exists */
int board_exists(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    int r = kanban_board_exists(slug);
    free(slug);
    return r;
}

/*
 * PoP: kanban_db_path @ hermes_cli/kanban_db.py:kanban_db_path
 * Returns malloc'd path to the kanban.db for board. Caller frees. */
char *kanban_db_path(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_DB");
    if (override && override[0]) return strdup(override);
    char *slug = normalize_board_slug(board);
    if (!slug) slug = get_current_board();   /* may return a fresh malloc */
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban.db", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/kanban.db", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/* PoP: profile_exists @ hermes_cli/profiles.py:profile_exists */
/* True iff a profile directory named `name` exists on disk. Shared by all
 * kanban concern modules (decompose, util) — promoted from a static helper so
 * there is a single source of truth. */
/* PoP: profile_exists @ hermes_cli/profiles.py:profile_exists
 * Python's kanban_db.py imports this from hermes_cli.profiles — it is
 * the SAME canonical profile-dir existence check, NOT a kanban-profile
 * listing. Delegate to the canonical implementation (port_cli_profiles.c
 * profile_dir_exists) so both spellings stay in lockstep with Python.
 * Returns -1 on empty name (Python raises ValueError there). */
extern int profile_dir_exists(const char *name); /* port_cli_profiles.c */
int profile_exists(const char *name)
{
    return profile_dir_exists(name);
}

/*
 * PoP: workspaces_root @ hermes_cli/kanban_db.py:workspaces_root
 * Returns malloc'd workspaces dir for board. Caller frees. */
char *workspaces_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_WORKSPACES_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/workspaces", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/workspaces", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * PoP: attachments_root @ hermes_cli/kanban_db.py:attachments_root
 * Returns malloc'd attachments dir for board. Caller frees. */
char *attachments_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_ATTACHMENTS_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/attachments", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/attachments", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * PoP: task_attachments_dir @ hermes_cli/kanban_db.py:task_attachments_dir
 * Returns malloc'd "<attachments_root>/<task_id>". Caller frees. */
char *task_attachments_dir(const char *task_id, const char *board)
{
    char *root = attachments_root(board);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/%s", root, task_id ? task_id : "");
    free(root);
    return strdup(out);
}

/*
 * PoP: worker_logs_dir @ hermes_cli/kanban_db.py:worker_logs_dir
 * Returns malloc'd worker-logs dir for board. Caller frees. */
char *worker_logs_dir(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = get_current_board();
    char out[PATH_MAX];
    if (strcmp(slug, KB_DEFAULT_BOARD) == 0) {
        char *home = kanban_home();
        snprintf(out, sizeof(out), "%s/kanban/logs", home);
        free(home);
    } else {
        char *bd = board_dir(slug);
        snprintf(out, sizeof(out), "%s/logs", bd);
        free(bd);
    }
    free(slug);
    return strdup(out);
}

/*
 * PoP: board_metadata_path @ hermes_cli/kanban_db.py:board_metadata_path
 * Returns malloc'd "<board_dir>/board.json". Caller frees. */
char *board_metadata_path(const char *board)
{
    char *slug = normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *bd = board_dir(slug);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/board.json", bd);
    free(bd);
    free(slug);
    return strdup(out);
}

/*
 * PoP: _default_board_display_name @ hermes_cli/kanban_db.py:_default_board_display_name
 * Turns "atm10-server" -> "Atm10 Server". Returns malloc'd string. Caller frees. */
char *default_board_display_name(const char *slug)
{
    /* Replace '_' with '-', split on '-', capitalize each part, join with space. */
    char tmp[256];
    size_t k = 0;
    for (size_t i = 0; slug && slug[i] && k < sizeof(tmp) - 1; i++) {
        char c = slug[i];
        if (c == '_') c = '-';
        tmp[k++] = c;
    }
    tmp[k] = '\0';
    /* tokenize on '-' */
    char *out = malloc(strlen(tmp) + 1);
    if (!out) return strdup(slug ? slug : "");
    out[0] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, "-", &save);
    int first = 1;
    while (tok) {
        if (tok[0]) {
            tok[0] = (char)toupper((unsigned char)tok[0]);
            if (!first) strcat(out, " ");
            strcat(out, tok);
            first = 0;
        }
        tok = strtok_r(NULL, "-", &save);
    }
    if (out[0] == '\0') {
        free(out);
        return strdup(slug ? slug : "");
    }
    return out;
}

/*
 * PoP: clear_current_board @ hermes_cli/kanban_db.py:clear_current_board
 * Removes <root>/kanban/current if present. Returns 0 on success / not-found. */
int clear_current_board(void)
{
    char *cp = kanban_current_board_path();
    int r = remove(cp);
    free(cp);
    return (r == 0 || errno == ENOENT) ? 0 : -1;
}

/*
 * PoP: _resolve_busy_timeout_ms @ hermes_cli/kanban_db.py:_resolve_busy_timeout_ms
 * Returns the SQLite busy timeout (ms) for Kanban connections. */
int resolve_busy_timeout_ms(void)
{
    const char *raw = getenv("HERMES_KANBAN_BUSY_TIMEOUT_MS");
    if (raw && raw[0]) {
        char *end = NULL;
        long parsed = strtol(raw, &end, 10);
        if (end != raw && *end == '\0' && parsed > 0) return (int)parsed;
    }
    return 120000;  /* default 120s, matches Python */
}

/*
 * PoP: _to_epoch @ hermes_cli/kanban_db.py:_to_epoch
 * Normalises int/float/numeric-string/ISO-8601 to unix epoch seconds.
 * Returns value, or -1 for None/empty/unparseable. */
long to_epoch(const char *val)
{
    if (!val) return -1;
    /* int / float pass-through */
    char *end = NULL;
    long as_long = strtol(val, &end, 10);
    if (end != val && *end == '\0') return as_long;
    double as_double = strtod(val, &end);
    if (end != val && *end == '\0') return (long)as_double;
    /* trim */
    while (*val && isspace((unsigned char)*val)) val++;
    size_t n = strlen(val);
    while (n > 0 && isspace((unsigned char)val[n - 1])) n--;
    if (n == 0) return -1;
    /* ISO-8601 fallback: replace trailing Z with +00:00 and parse */
    char buf[64];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, val, n);
    buf[n] = '\0';
    char *z = strchr(buf, 'Z');
    if (z) { *z = '+'; memmove(z + 1, z, strlen(z) + 1); buf[strlen(buf)] = '\0';
             /* re-insert ":00" after + : need "+00:00" form; simplify to "+00" */
             size_t len = strlen(buf);
             if (len + 2 < sizeof(buf)) { buf[len] = ':'; buf[len+1] = '0'; buf[len+2] = '0'; buf[len+3] = '\0'; } }
    /* Best-effort: only handle the common 'YYYY-MM-DDTHH:MM:SS' (no tz) and
       'YYYY-MM-DDTHH:MM:SS+00:00' forms via a minimal parser. */
    int Y, M, D, h = 0, m = 0, s = 0, tz_h = 0, tz_m = 0;
    char sep1, sep2;
    int got = sscanf(buf, "%d-%d-%d%c%d:%d:%d%c%d:%d",
                     &Y, &M, &D, &sep1, &h, &m, &s, &sep2, &tz_h, &tz_m);
    if (got >= 7) {
        /* days-from-civil (Howard Hinnant) */
        int yy = Y - (M <= 2 ? 1 : 0);
        int era = (yy >= 0 ? yy : yy - 399) / 400;
        long yoe = (long)(yy - era * 400);
        long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
        long doe = (era * 365L + yoe / 4 - yoe / 100) * 1L + doy;
        long days = era * 146097L + doe - 719468L;
        long secs = days * 86400L + h * 3600L + m * 60L + s - (tz_h * 3600L + tz_m * 60L);
        return (long)secs;
    }
    return -1;
}

/*
 * PoP: _looks_like_tls_record_at @ hermes_cli/kanban_db.py:_looks_like_tls_record_at
 * Returns 1 for a TLS record header at data[offset:]. */
int looks_like_tls_record_at(const unsigned char *data, size_t len, size_t offset)
{
    if (len < offset + 5) return 0;
    unsigned char content_type = data[offset];
    unsigned char major = data[offset + 1];
    unsigned char minor = data[offset + 2];
    unsigned int length = ((unsigned int)data[offset + 3] << 8) | data[offset + 4];
    int ct_ok = (content_type == 0x14 || content_type == 0x15 ||
                 content_type == 0x16 || content_type == 0x17);
    int ver_ok = (major == 0x03) && (minor <= 0x04);
    return (ct_ok && ver_ok && length > 0 && length <= 18432) ? 1 : 0;
}

/*
 * PoP: _validate_sqlite_header @ hermes_cli/kanban_db.py:_validate_sqlite_header
 * Returns 1 if path is absent/empty/sqlite-header, 0 if a non-empty
 * non-sqlite file (caller should error). */
int validate_sqlite_header(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 1;          /* absent => OK (sqlite creates it) */
    if (st.st_size == 0) return 1;               /* zero-byte => OK */
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char head[16];
    size_t n = fread(head, 1, 16, f);
    fclose(f);
    if (n < 16) return 1;
    return (memcmp(head, "SQLite format 3\0", 16) == 0) ? 1 : 0;
}

/*
 * PoP: _error_fingerprint @ hermes_cli/kanban_db.py:_error_fingerprint
 * Normalises an error string for grouping identical failures.
 * Returns malloc'd fingerprint (lowercased, host details stripped). Caller frees. */
char *error_fingerprint(const char *error_text)
{
    if (!error_text) return strdup("");
    char buf[128];
    strncpy(buf, error_text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* take first 80 chars */
    size_t n = strlen(buf);
    if (n > 80) buf[80] = '\0';
    /* strip "pid <digits>" -> "pid N" and "<10+ digit numbers>" -> "<TS>" */
    char out[256];
    out[0] = '\0';
    const char *p = buf;
    while (*p) {
        if (strncmp(p, "pid ", 4) == 0) {
            const char *q = p + 4;
            while (isdigit((unsigned char)*q)) q++;
            strcat(out, "pid N");
            p = q;
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            const char *q = p;
            int cnt = 0;
            while (isdigit((unsigned char)*q)) { q++; cnt++; }
            if (cnt >= 10) {
                strcat(out, "<TS>");
                p = q;
                continue;
            }
        }
        size_t L = strlen(out);
        out[L] = *p++;
        out[L + 1] = '\0';
    }
    /* lowercase */
    for (size_t i = 0; out[i]; i++)
        out[i] = (char)tolower((unsigned char)out[i]);
    /* strip trailing/leading whitespace */
    char *s = out;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t e = strlen(s);
    while (e > 0 && isspace((unsigned char)s[e - 1])) s[--e] = '\0';
    return strdup(s);
}

/*
 * PoP: _rotated_log_path @ hermes_cli/kanban_db.py:_rotated_log_path
 * Returns malloc'd "<log>.<generation>". Caller frees. */
char *rotated_log_path(const char *log_path, int generation)
{
    /* find suffix start (last '.'), append ".<gen>" after it */
    const char *dot = strrchr(log_path, '.');
    size_t base_len = dot ? (size_t)(dot - log_path) : strlen(log_path);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%.*s.%d", (int)base_len, log_path, generation);
    return strdup(out);
}

/*
 * PoP: _looks_like_path @ hermes_cli/kanban_db.py:_looks_like_path
 * Returns 1 when a command override is an explicit path, not a name. */
int looks_like_path(const char *value)
{
    if (!value || !value[0]) return 0;
    /* ~ expansion is best-effort; we check leading '~', absolute, has dirname,
       backslash, or "X:" windows drive. */
    if (value[0] == '~') return 1;
    if (value[0] == '/') return 1;
    if (strchr(value, '\\')) return 1;
    if (value[1] == ':' && isalpha((unsigned char)value[0])) return 1;
    /* dirname present if any '/' before the final component */
    const char *sl = strchr(value, '/');
    if (sl && sl != value + strlen(value) - 1) {
        /* ensure it's not just a trailing slash on a single name */
        const char *last = strrchr(value, '/');
        if (last != value) return 1;
    }
    return 0;
}

/*
 * PoP: _is_windows_batch_shim @ hermes_cli/kanban_db.py:_is_windows_batch_shim */
int is_windows_batch_shim(const char *path)
{
    if (!path) return 0;
    size_t n = strlen(path);
    if (n >= 4 && (strcasecmp(path + n - 4, ".cmd") == 0 ||
                   strcasecmp(path + n - 4, ".bat") == 0))
        return 1;
    return 0;
}

/*
 * PoP: _scratch_tip_sentinel_path @ hermes_cli/kanban_db.py:_scratch_tip_sentinel_path
 * Returns malloc'd "<kanban_home>/<SCRATCH_TIP_SENTINEL_NAME>". Caller frees. */
char *scratch_tip_sentinel_path(void)
{
    char *home = kanban_home();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.scratch_tip_shown", home);
    free(home);
    return strdup(out);
}
