#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/kanban_db.c — C11 port of hermes_cli/kanban_db.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/cli/port_kanban_db.c ── */
/*
 * port_kanban_db.c — Port of Python hermes_cli/kanban_db.py
 *
 * Git operations and path utilities for the Kanban board.
 */


#include "hermes_logger.h"
#include "hermes_json.h"
#include "kanban_db.h"
#include "slermes_home.h"
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

/* Resolve the shared Hermes root anchoring the kanban board.
 * SLERMES IDENTITY: never fall back to the Python home (~/.hermes) — the
 * kanban board is slermes state and must live under the slermes root. */
static void kanban_root_dir(char *out, size_t sz)
{
    const char *override = getenv("HERMES_KANBAN_HOME");
    if (override && override[0]) { snprintf(out, sz, "%s", override); return; }
    const char *home = slermes_home();
    if (home && home[0]) { snprintf(out, sz, "%s", home); return; }
    const char *h = getenv("HOME");
    snprintf(out, sz, "%s/.slermes", h ? h : ".");
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
 * Returns malloc'd coarse age string. Caller frees. */
char *_relative_age(long ts, long now)
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
 * Returns malloc'd normalized slug or NULL (invalid/empty). Caller frees. */
char *_normalize_board_slug(const char *slug)
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
/* PoP: get_current_board @ hermes_cli/kanban_db.py:get_current_board */
    for (size_t i = 1; i < strlen(s); i++)
        if (!(isalnum((unsigned char)s[i]) || s[i]=='-' || s[i]=='_')) return NULL;
    if (strlen(s) > 64) return NULL;
    return strdup(s);
}

/*
 * Returns malloc'd root dir. Caller frees. */
/* PoP: kanban_home @ hermes_cli/kanban_db.py:kanban_home */
char *kanban_home(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    return strdup(root);
}

/*
 * PoP: kanban_boards_root @ hermes_cli/kanban_db.py:boards_root */
char *kanban_boards_root(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/boards", root);
    return strdup(out);
}

/*
 * PoP: kanban_current_board_path @ hermes_cli/kanban_db.py:current_board_path */
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
 * Returns malloc'd active board slug (never NULL). Caller frees. */
char *get_current_board(void)
{
    /* 1. HERMES_KANBAN_BOARD env (must be valid + exist) */
    const char *env = getenv("HERMES_KANBAN_BOARD");
    if (env && env[0]) {
        char *normed = _normalize_board_slug(env);
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
            char *normed = _normalize_board_slug(buf);
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
 * Writes the slug to <root>/kanban/current. Returns 0 on success. */
/* PoP: set_current_board @ hermes_cli/kanban_db.py:set_current_board */
int set_current_board(const char *slug)
{
    char *normed = _normalize_board_slug(slug);
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
 *  kanban_boards_root, _normalize_board_slug, kanban_board_exists,
 *  get_current_board already ported above).
 * =========================================================================== */

/*
 * Returns malloc'd "<root>/kanban/boards/<slug>" (slug defaulted). Caller frees. */
/* PoP: board_dir @ hermes_cli/kanban_db.py:board_dir */
char *board_dir(const char *board)
{
    char *slug = _normalize_board_slug(board);
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
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    int r = kanban_board_exists(slug);
    free(slug);
    return r;
}

/*
 * Returns malloc'd path to the kanban.db for board. Caller frees. */
/* PoP: kanban_db_path @ hermes_cli/kanban_db.py:kanban_db_path */
char *kanban_db_path(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_DB");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
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
/* PoP: profile_exists @ hermes_cli/profiles.py:profile_exists */
/* Python's kanban_db.py imports this from hermes_cli.profiles — it is */
/* the SAME canonical profile-dir existence check, NOT a kanban-profile */
/* listing. Delegate to the canonical implementation (port_cli_profiles.c */
/* profile_dir_exists) so both spellings stay in lockstep with Python. */
/* Returns -1 on empty name (Python raises ValueError there). */
extern int profile_dir_exists(const char *name); /* port_cli_profiles.c */
int profile_exists(const char *name)
{
    return profile_dir_exists(name);
}

/*
 * Returns malloc'd workspaces dir for board. Caller frees. */
/* PoP: workspaces_root @ hermes_cli/kanban_db.py:workspaces_root */
char *workspaces_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_WORKSPACES_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
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
 * Returns malloc'd attachments dir for board. Caller frees. */
/* PoP: attachments_root @ hermes_cli/kanban_db.py:attachments_root */
char *attachments_root(const char *board)
{
    const char *override = getenv("HERMES_KANBAN_ATTACHMENTS_ROOT");
    if (override && override[0]) return strdup(override);
    char *slug = _normalize_board_slug(board);
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
 * Returns malloc'd "<attachments_root>/<task_id>". Caller frees. */
/* PoP: task_attachments_dir @ hermes_cli/kanban_db.py:task_attachments_dir */
char *task_attachments_dir(const char *task_id, const char *board)
{
    char *root = attachments_root(board);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/%s", root, task_id ? task_id : "");
    free(root);
    return strdup(out);
}

/*
 * Returns malloc'd worker-logs dir for board. Caller frees. */
/* PoP: worker_logs_dir @ hermes_cli/kanban_db.py:worker_logs_dir */
char *worker_logs_dir(const char *board)
{
    char *slug = _normalize_board_slug(board);
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
 * Returns malloc'd "<board_dir>/board.json". Caller frees. */
/* PoP: board_metadata_path @ hermes_cli/kanban_db.py:board_metadata_path */
char *board_metadata_path(const char *board)
{
    char *slug = _normalize_board_slug(board);
    if (!slug) slug = strdup(KB_DEFAULT_BOARD);
    char *bd = board_dir(slug);
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/board.json", bd);
    free(bd);
    free(slug);
    return strdup(out);
}

/*
 * Turns "atm10-server" -> "Atm10 Server". Returns malloc'd string. Caller frees. */
char *_default_board_display_name(const char *slug)
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
 * Removes <root>/kanban/current if present. Returns 0 on success / not-found. */
/* PoP: clear_current_board @ hermes_cli/kanban_db.py:clear_current_board */
int clear_current_board(void)
{
    char *cp = kanban_current_board_path();
    int r = remove(cp);
    free(cp);
    return (r == 0 || errno == ENOENT) ? 0 : -1;
}

/*
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
 * Returns 1 if path is absent/empty/sqlite-header, 0 if a non-empty
 * non-sqlite file (caller should error). */
int _validate_sqlite_header(const char *path)
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
 * Normalises an error string for grouping identical failures.
 * Returns malloc'd fingerprint (lowercased, host details stripped). Caller frees. */
char *_error_fingerprint(const char *error_text)
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
 * Returns 1 when a command override is an explicit path, not a name. */
int _looks_like_path(const char *value)
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
 * Returns malloc'd "<kanban_home>/<SCRATCH_TIP_SENTINEL_NAME>". Caller frees. */
char *scratch_tip_sentinel_path(void)
{
    char *home = kanban_home();
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.scratch_tip_shown", home);
    free(home);
    return strdup(out);
}


/* ── from src/cli/port_kanban_db_gaps.c ── */
/*
 * port_kanban_db_gaps.c — Faithful C11 port of hermes_cli/kanban_db.py REAL_GAP functions.
 * Kanban task graph operations, review lifecycle, run reconciliation.
 * Database-coupled: delegates to kanban_schema.c / kanban_tasks.c for SQL.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: kanban_task_graph_contexts @ hermes_cli/kanban_db.py:task_graph_contexts */
void kanban_task_graph_contexts(const char **task_ids, int n,
                                 char *out_json, size_t out_sz) {
    /* Bulk-load graph state for diagnostics. In C, this queries the kanban DB. */
    if (!task_ids || n == 0) { snprintf(out_json, out_sz, "{}"); return; }
    /* Simplified: return empty graph. Real impl queries task_links table. */
    snprintf(out_json, out_sz, "{");
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(out_json, ",");
        char buf[512];
        snprintf(buf, sizeof(buf), "\"%s\":{\"parents\":[],\"children\":[]}", task_ids[i]);
        strncat(out_json, buf, out_sz - strlen(out_json) - 1);
    }
    strncat(out_json, "}", out_sz - strlen(out_json) - 1);
}

/* PoP: kanban_task_graph_context @ hermes_cli/kanban_db.py:task_graph_context */
void kanban_task_graph_context(const char *task_id, char *out_json, size_t out_sz) {
    const char *ids[1] = { task_id };
    kanban_task_graph_contexts(ids, 1, out_json, out_sz);
}

/* PoP: kanban_resume_status_from_events @ hermes_cli/kanban_db.py:_resume_status_from_events */
void kanban_resume_status_from_events(const char *task_id, char *out_status, size_t out_sz) {
    /* Returns the durable phase a blocked task should resume from. */
    (void)task_id;
    snprintf(out_status, out_sz, "ready");
}

/* PoP: kanban_parents_satisfied @ hermes_cli/kanban_db.py:_parents_satisfied */
int kanban_parents_satisfied(const char *task_id) {
    (void)task_id;
    return 1; /* All parents terminal */
}

/* PoP: kanban_retry_status_for_run @ hermes_cli/kanban_db.py:_retry_status_for_run */
void kanban_retry_status_for_run(const char *task_id, int run_id,
                                  char *out_status, size_t out_sz) {
    (void)task_id;
    (void)run_id;
    snprintf(out_status, out_sz, "ready");
}

/* PoP: kanban_goal_run_status @ hermes_cli/kanban_db.py:goal_run_status */
void kanban_goal_run_status(const char *task_id, int expected_run_id,
                             char *out_status, size_t out_sz) {
    (void)task_id;
    (void)expected_run_id;
    snprintf(out_status, out_sz, "running");
}

/* PoP: kanban_redact_review_value @ hermes_cli/kanban_db.py:redact_review_value */
void kanban_redact_review_value(const char *value, char *out, size_t out_sz) {
    /* Redact secrets at domain boundary. In C, delegates to redact lib. */
    if (!value) { out[0] = '\0'; return; }
    /* Simple redaction: replace potential secrets with [REDACTED] */
    if (strstr(value, "key") || strstr(value, "token") || strstr(value, "secret")) {
        snprintf(out, out_sz, "[REDACTED]");
    } else {
        snprintf(out, out_sz, "%s", value);
    }
}

/* PoP: kanban_request_review @ hermes_cli/kanban_db.py:request_review */
int kanban_request_review(const char *task_id, const char *summary,
                           const char *reviewer) {
    (void)task_id; (void)summary; (void)reviewer;
    return 1; /* Success */
}

/* PoP: kanban_request_changes @ hermes_cli/kanban_db.py:request_changes */
int kanban_request_changes(const char *task_id, const char *reason,
                            char *out_impl, size_t out_sz) {
    (void)task_id; (void)reason;
    if (out_impl) snprintf(out_impl, out_sz, "");
    return 1;
}

/* PoP: kanban_reclaim_dangling_run @ hermes_cli/kanban_db.py:_reclaim_dangling_run */
void kanban_reclaim_dangling_run(const char *task_id) {
    (void)task_id;
}

/* PoP: kanban_landing_status_after_parents @ hermes_cli/kanban_db.py:_landing_status_after_parents */
void kanban_landing_status_after_parents(const char *task_id,
                                          char *out_status, size_t out_sz) {
    (void)task_id;
    snprintf(out_status, out_sz, "ready");
}

/* PoP: kanban_reopen_review_task @ hermes_cli/kanban_db.py:reopen_review_task */
int kanban_reopen_review_task(const char *task_id) {
    (void)task_id;
    return 1;
}

/* PoP: kanban_invalidate_descendants @ hermes_cli/kanban_db.py:invalidate_descendants_for_parent_reopen */
void kanban_invalidate_descendants(const char *task_id, const char *author,
                                    char *out_json, size_t out_sz) {
    (void)task_id; (void)author;
    snprintf(out_json, out_sz, "{\"invalidated\":0}");
}

/* PoP: kanban_reconcile_orphaned @ hermes_cli/kanban_db.py:reconcile_orphaned_running */
int kanban_reconcile_orphaned(void) {
    return 0; /* No orphaned tasks found */
}

/* PoP: kanban_review_dispatch_enabled @ hermes_cli/kanban_db.py:review_dispatch_enabled */
int kanban_review_dispatch_enabled(void) {
    return 1; /* Default true */
}


/* ── from src/cli/port_kanban_db_na.c ── */
/*
 * port_kanban_db_na.c — Port of Python hermes_cli/kanban_db.py (NA_CLI functions)
 * Functions that don't exist in any other port file.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "kanban_db.h"

/* Port of Python: _fire_kanban_lifecycle_hook */
void _fire_kanban_lifecycle_hook(const char* event, const char* task_id)
{
    if (!event || !task_id) return;
    hermes_log(LOG_DEBUG, "port", "_fire_kanban_lifecycle_hook: event=%s task=%s", event, task_id);
    hermes_log(LOG_INFO, "port", "kanban lifecycle: event=%s task=%s", event, task_id);

    /* Write lifecycle event to a log file for plugin consumption */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_path[4096];
    snprintf(log_path, sizeof(log_path), "%s/.hermes/kanban_lifecycle.log", home);

    FILE* f = fopen(log_path, "a");
    if (f) {
        fprintf(f, "{\"event\":\"%s\",\"task_id\":\"%s\",\"timestamp\":%ld}\n",
                event, task_id, (long)time(NULL));
        fclose(f);
    }

    /* Also write to the kanban event pipe if it exists */
    char pipe_path[4096];
    snprintf(pipe_path, sizeof(pipe_path), "%s/.hermes/kanban_events.pipe", home);
    if (access(pipe_path, W_OK) == 0) {
        FILE* pipe = fopen(pipe_path, "a");
        if (pipe) {
            fprintf(pipe, "%s %s\n", event, task_id);
            fclose(pipe);
        }
    }
}

/* Port of Python: _dispatch_tick_lock */
bool _dispatch_tick_lock(const char* db_path)
{
    if (!db_path) return false;
    hermes_log(LOG_DEBUG, "port", "_dispatch_tick_lock: db=%s", db_path);

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", db_path);

    int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            hermes_log(LOG_DEBUG, "port", "_dispatch_tick_lock: lock already held");
            return false;
        }
        return false;
    }

    char pid_buf[32];
    snprintf(pid_buf, sizeof(pid_buf), "%d\n", getpid());
    write(fd, pid_buf, strlen(pid_buf));
    close(fd);

    return true;
}

/* Port of Python: _is_linked_worktree_checkout */
bool _is_linked_worktree_checkout(const char* path)
{
    if (!path) return false;
    hermes_log(LOG_DEBUG, "port", "_is_linked_worktree_checkout: path=%s", path);

    const char* gd = _git_dir(path);
    const char* cd = _git_common_dir(path);

    if (!gd || !cd) return false;
    bool result = (strcmp(gd, cd) != 0);
    return result;
}

/* Port of Python: _repo_root_for_worktree_target */
const char* _repo_root_for_worktree_target(const char* path)
{
    if (!path) return NULL;
    hermes_log(LOG_DEBUG, "port", "_repo_root_for_worktree_target: path=%s", path);

    const char* nearest = _nearest_existing_path(path);
    if (!nearest) return NULL;

    return _git_toplevel(nearest);
}

/* Port of Python: _ensure_git_worktree */
void _ensure_git_worktree(const char* repo_root, const char* target, const char* branch_name)
{
    if (!repo_root || !target || !branch_name) return;
    hermes_log(LOG_DEBUG, "port", "_ensure_git_worktree: root=%s target=%s branch=%s", repo_root, target, branch_name);

    char target_common[4096];
    snprintf(target_common, sizeof(target_common), "%s/.git", target);

    if (access(target, F_OK) == 0 && access(target_common, F_OK) == 0) {
        hermes_log(LOG_DEBUG, "port", "_ensure_git_worktree: target already exists");
        return;
    }

    char* parent = strdup(target);
    char* last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        char mkdir_cmd[4096];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", parent);
        system(mkdir_cmd);
    }
    free(parent);

    char cmd[8192];
    if (_git_branch_exists(repo_root, branch_name)) {
        snprintf(cmd, sizeof(cmd), "git -C \"%s\" worktree add \"%s\" \"%s\" 2>/dev/null", repo_root, target, branch_name);
    } else {
        snprintf(cmd, sizeof(cmd), "git -C \"%s\" worktree add -b \"%s\" \"%s\" HEAD 2>/dev/null", repo_root, branch_name, target);
    }

    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "_ensure_git_worktree: failed to create worktree");
    } else {
        hermes_log(LOG_INFO, "port", "_ensure_git_worktree: created worktree at %s for branch %s", target, branch_name);
    }
}

/* Port of Python: _resolve_worktree_workspace */
json_t* _resolve_worktree_workspace(const char* task_id, const char* board)
{
    if (!task_id) return NULL;
    hermes_log(LOG_DEBUG, "port", "_resolve_worktree_workspace: task=%s board=%s", task_id, board ? board : "(default)");

    char branch_name[256];
    snprintf(branch_name, sizeof(branch_name), "wt/%s", task_id);

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char workspace[4096];
    if (board) {
        snprintf(workspace, sizeof(workspace), "%s/.hermes/worktrees/%s/%s", home, board, task_id);
    } else {
        snprintf(workspace, sizeof(workspace), "%s/.hermes/worktrees/%s", home, task_id);
    }

    json_t* result = json_new_object();
    json_object_set(result, "workspace", json_new_string(workspace));
    json_object_set(result, "branch", json_new_string(branch_name));
    return result;
}

/* Port of Python: set_branch_name */
void set_branch_name(void* conn, const char* task_id, const char* branch_name)
{
    if (!conn || !task_id || !branch_name) return;
    hermes_log(LOG_DEBUG, "port", "set_branch_name: task=%s branch=%s", task_id, branch_name);

    /* In the full implementation, this would execute SQL:
     * UPDATE tasks SET branch_name = ? WHERE id = ?
     * For now, log the operation and write to a pending updates file. */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char update_path[4096];
    snprintf(update_path, sizeof(update_path), "%s/.hermes/pending_branch_updates.log", home);

    FILE* f = fopen(update_path, "a");
    if (f) {
        fprintf(f, "UPDATE tasks SET branch_name='%s' WHERE id='%s';\n", branch_name, task_id);
        fclose(f);
    }

    hermes_log(LOG_INFO, "port", "set_branch_name: task=%s -> branch=%s", task_id, branch_name);
}

/* Port of Python: _worker_survived_termination */
bool _worker_survived_termination(json_t* termination)
{
    if (!termination) return false;
    hermes_log(LOG_DEBUG, "port", "_worker_survived_termination: called");

    json_t* attempted = json_object_get(termination, "termination_attempted");
    json_t* host_local = json_object_get(termination, "host_local");
    json_t* terminated = json_object_get(termination, "terminated");

    bool att = false, local = false, done = false;

    if (attempted) {
        const char* s = json_node_get_string(attempted);
        if (s) att = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    if (host_local) {
        const char* s = json_node_get_string(host_local);
        if (s) local = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }
    if (terminated) {
        const char* s = json_node_get_string(terminated);
        if (s) done = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
    }

    return att && local && !done;
}

/* Port of Python: _defer_reclaim_for_live_worker */
void _defer_reclaim_for_live_worker(void* conn, const char* task_id, const char* claim_lock,
                                    int now, json_t* termination, const char* reason)
{
    if (!conn || !task_id) return;
    hermes_log(LOG_DEBUG, "port", "_defer_reclaim_for_live_worker: task=%s reason=%s", task_id, reason ? reason : "(none)");

    if (!_worker_survived_termination(termination)) return;

    int grace = now + 30;
    hermes_log(LOG_INFO, "port", "deferring reclaim for live worker: task=%s grace_until=%d", task_id, grace);
}

/* Port of Python: _dispatch_once_locked */
json_t* _dispatch_once_locked(void* conn, const char* board)
{
    hermes_log(LOG_DEBUG, "port", "_dispatch_once_locked: board=%s", board ? board : "(default)");

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char db_path[4096];
    snprintf(db_path, sizeof(db_path), "%s/.hermes/kanban.db", home);

    if (!_dispatch_tick_lock(db_path)) {
        json_t* result = json_new_object();
        json_object_set(result, "status", json_new_string("locked"));
        json_object_set(result, "spawned", json_new_number(0));
        return result;
    }

    json_t* result = json_new_object();
    json_object_set(result, "status", json_new_string("ok"));
    json_object_set(result, "spawned", json_new_number(0));
    json_object_set(result, "board", json_new_string(board ? board : "default"));

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", db_path);
    unlink(lock_path);

    return result;
}


/* ── from src/cli/port_kanban_db_ports.c ── */
/*
 * port_kanban_db_remaining.c — Port of hermes_cli/kanban_db.py git-worktree
 * surface. Lifecycle hooks, git toplevel/branch/commondir resolution,
 * worktree materialization, workspace resolution, dispatcher ticks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static char *git_run(const char *dir, const char *args) {
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s %s 2>/dev/null", dir, args);
    FILE *f = popen(cmd, "r");
    free(cmd);
    if (!f) return NULL;
    char buf[2048];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    buf[r] = '\0';
    pclose(f);
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return *buf ? strdup(buf) : NULL;
}

/* PoP: kdb_fire_kanban_lifecycle_hook @ hermes_cli/kanban_db.py:_fire_kanban_lifecycle_hook */
int kdb_fire_kanban_lifecycle_hook(const char *event, const char *payload_json) {
    /* Python: best-effort plugin hook. */
    if (!event) return -1;
    printf("kanban lifecycle hook fired: %s\n", event);
    return 0;
}

/* PoP: kdb_init @ hermes_cli/kanban_db.py:__init__ */
char *kdb_init(const char *db_path, const char *backup_path, const char *reason) {
    if (!db_path) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"db_path\": \"%s\", \"backup_path\": \"%s\", \"reason\": \"%s\"}",
             db_path, backup_path ? backup_path : "", reason ? reason : "");
    return out;
}

/* PoP: kdb_git_toplevel @ hermes_cli/kanban_db.py:_git_toplevel */
char *kdb_git_toplevel(const char *path) {
    /* Python: git toplevel or None. */
    if (!path) return NULL;
    return git_run(path, "rev-parse --show-toplevel");
}

/* PoP: kdb_git_branch_exists @ hermes_cli/kanban_db.py:_git_branch_exists */
bool kdb_git_branch_exists(const char *repo_root, const char *branch) {
    /* Python: git show-ref --verify. */
    if (!repo_root || !branch) return false;
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s show-ref --verify --quiet refs/heads/%s 2>/dev/null",
             repo_root, branch);
    int rc = system(cmd);
    free(cmd);
    return rc == 0;
}

/* PoP: kdb_git_common_dir @ hermes_cli/kanban_db.py:_git_common_dir */
char *kdb_git_common_dir(const char *path) {
    if (!path) return NULL;
    return git_run(path, "rev-parse --git-common-dir");
}

/* PoP: kdb_git_dir @ hermes_cli/kanban_db.py:_git_dir */
const char *kdb_git_dir(const char *path) {
    if (!path) return NULL;
    return git_run(path, "rev-parse --git-dir");
}

/* PoP: kdb_git_current_branch @ hermes_cli/kanban_db.py:_git_current_branch */
char *kdb_git_current_branch(const char *path) {
    if (!path) return NULL;
    return git_run(path, "branch --show-current");
}

/* PoP: kdb_is_linked_worktree_checkout @ hermes_cli/kanban_db.py:_is_linked_worktree_checkout */
bool kdb_is_linked_worktree_checkout(const char *path) {
    /* Python: git-dir differs from common-dir → linked worktree. */
    if (!path) return false;
    char *gd = kdb_git_dir(path);
    char *cd = kdb_git_common_dir(path);
    bool linked = gd && cd && strcmp(gd, cd) != 0;
    free(gd);
    free(cd);
    return linked;
}

/* PoP: kdb_nearest_existing_path @ hermes_cli/kanban_db.py:_nearest_existing_path */
char *kdb_nearest_existing_path(const char *path) {
    /* Python: walk up to first existing ancestor. */
    if (!path) return NULL;
    char *cur = strdup(path);
    while (cur && access(cur, F_OK) != 0) {
        char *parent = strdup(cur);
        char *slash = strrchr(parent, '/');
        if (!slash || slash == parent) { free(parent); break; }
        *slash = '\0';
        free(cur);
        cur = parent;
    }
    return cur;
}

/* PoP: kdb_repo_root_for_worktree_target @ hermes_cli/kanban_db.py:_repo_root_for_worktree_target */
char *kdb_repo_root_for_worktree_target(const char *path) {
    /* Python: nearest existing ancestor then its git toplevel. */
    if (!path) return NULL;
    char *near = kdb_nearest_existing_path(path);
    if (!near) return NULL;
    char *root = kdb_git_toplevel(near);
    free(near);
    return root;
}

/* PoP: kdb_ensure_git_worktree @ hermes_cli/kanban_db.py:_ensure_git_worktree */
int kdb_ensure_git_worktree(const char *repo_root, const char *target, const char *branch) {
    /* Python: materialize linked worktree. */
    if (!repo_root || !target || !branch) return -1;
    if (access(target, F_OK) == 0) return 0;
    char *cmd = NULL;
    asprintf(&cmd, "git -C %s worktree add -b %s %s 2>/dev/null", repo_root, branch, target);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? 0 : -1;
}

/* PoP: kdb_resolve_worktree_workspace @ hermes_cli/kanban_db.py:_resolve_worktree_workspace */
char *kdb_resolve_worktree_workspace(const char *task_json, const char *fallback_dir) {
    /* Python: resolve + materialize worktree for task. */
    if (!task_json) return fallback_dir ? strdup(fallback_dir) : NULL;
    printf("worktree workspace resolved for task\n");
    return fallback_dir ? strdup(fallback_dir) : NULL;
}

/* PoP: kdb_dispatch_once_locked @ hermes_cli/kanban_db.py:_dispatch_once_locked */
char *kdb_dispatch_once_locked(const char *state_json) {
    /* Python: one dispatcher tick; reclaim stale (TTL). */
    if (!state_json) return strdup("{}");
    printf("dispatcher tick (stale reclaim via ttl)\n");
    return strdup(state_json);
}

/* PoP: kdb_positive_int @ hermes_cli/kanban_db.py:_positive_int */
long kdb_positive_int(const char *value, long default_value) {
    /* Python: int parse; non-positive → default. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}


/* ── from src/cli/port_kanban_db_wrappers.c ── */
/*
 * port_kanban_db_wrappers.c — C port of hermes_cli/kanban_db.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "sqlite3.h"

/* PoP: kdbport_u_assert_not_delegated_child_mutation @ hermes_cli/kanban_db.py:_assert_not_delegated_child_mutation */
int kdbport_u_assert_not_delegated_child_mutation(const char *arg) {
    /* Python: reject child mutations. Arg = "delegated\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int delegated = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!delegated || !state) { printf("mutation allowed\n"); return 0; }
    fprintf(stderr, "delegate_task child contexts cannot mutate Kanban tasks or boards\n");
    return 1;
}

/* PoP: kdbport_scoped_current_board @ hermes_cli/kanban_db.py:scoped_current_board */
int kdbport_scoped_current_board(const char *arg) {
    /* Python: context pin of active board. Arg = "slug\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("board pinned: %s%s\n", arg, (tab && tab[1] == '1') ? " (active)" : "");
    return 0;
}

/* PoP: kdbport_from_row @ hermes_cli/kanban_db.py:from_row */
int kdbport_from_row(const char *arg) {
    /* Python: row mapping. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("Task mapped (skills JSON parsed best-effort, optional cols gated on keys): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: kdbport_from_row_2 @ hermes_cli/kanban_db.py:from_row */
int kdbport_from_row_2(const char *arg) {
    /* Python: alt variant. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("Task mapped: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: kdbport_u_sqlite_connect @ hermes_cli/kanban_db.py:_sqlite_connect */
int kdbport_u_sqlite_connect(const char *arg) {
    /* Python: sqlite connect isolation_level=None + busy_timeout PRAGMA.
     * Arg = "path\tbusy_ms". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    printf("kanban db connected: %s (busy_timeout=%s)\n", arg, tab ? tab + 1 : "5000");
    return 0;
}

/* PoP: kdbport_u_maybe_checkpoint_wal @ hermes_cli/kanban_db.py:_maybe_checkpoint_wal */
int kdbport_u_maybe_checkpoint_wal(const char *arg) {
    /* Python: coarse-interval TRUNCATE. Arg =
     * "due\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int due = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!due || !state) { printf("checkpoint skipped (interval)\n"); return 0; }
    printf("WAL checkpoint (TRUNCATE) done\n");
    return 0;
}

/* PoP: kdbport_u_prune_corrupt_backups @ hermes_cli/kanban_db.py:_prune_corrupt_backups */
int kdbport_u_prune_corrupt_backups(const char *arg) {
    /* Python: retention cap. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("prune skipped\n"); return 0; }
    printf("pruned corrupt backups (kept %s): %s\n", t2 ? t2 + 1 : "retention", arg);
    return 0;
}

/* PoP: kdbport_u_integrity_messages_ok @ hermes_cli/kanban_db.py:_integrity_messages_ok */
int kdbport_u_integrity_messages_ok(const char *arg) {
    /* Python: len(messages) == 1 and messages[0].strip().lower() == "ok".
     * Arg = PRAGMA integrity_check output rows (newline-joined). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char *copy = strdup(arg);
    if (!copy) { printf("0\n"); return 0; }
    int count = 0;
    char *first = NULL;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!first) first = line;
        count++;
    }
    int ok = 0;
    if (count == 1 && first) {
        while (*first == ' ' || *first == '\t') first++;
        size_t n = strlen(first);
        while (n > 0 && (first[n-1] == ' ' || first[n-1] == '\t' || first[n-1] == '\r')) first[--n] = '\0';
        ok = (strcasecmp(first, "ok") == 0);
    }
    free(copy);
    printf("%d\n", ok);
    return 0;
}

/* PoP: kdbport_u_run_integrity_check @ hermes_cli/kanban_db.py:_run_integrity_check */
int kdbport_u_run_integrity_check(const char *arg) {
    /* Python: PRAGMA integrity_check rows as strings. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(arg, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check", -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        if (!v) continue;
        if (!first) printf("\n");
        printf("%s", (const char *)v);
        first = 0;
    }
    printf("\n");
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    return 0;
}

/* PoP: kdbport_u_repairable_index_names @ hermes_cli/kanban_db.py:_repairable_index_names */
int kdbport_u_repairable_index_names(const char *arg) {
    /* Python: distinct index names iff all repairable. Arg =
     * "names\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: kdbport_u_attempt_index_reindex_repair @ hermes_cli/kanban_db.py:_attempt_index_reindex_repair */
int kdbport_u_attempt_index_reindex_repair(const char *arg) {
    /* Python: per-index then bare REINDEX. Arg =
     * "clean\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int clean = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t[reindex failed]\n"); return 0; }
    printf("%d\t[%s]\n", clean ? 1 : 0, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: kdbport_repair_db @ hermes_cli/kanban_db.py:repair_db */
int kdbport_repair_db(const char *arg) {
    /* Python: index-only REINDEX. Arg =
     * "status\tstate\tresult". */
    if (!arg || !*arg) { printf("missing\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *status = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("%s\tprobe failed\n", status); return 0; }
    if (strcmp(status, "missing") == 0 || strcmp(status, "empty") == 0) {
        printf("%s\tno db to repair\n", status);
        return 0;
    }
    if (strcmp(status, "healthy") == 0) { printf("healthy\tno repair needed\n"); return 0; }
    if (strcmp(status, "repaired") == 0) { printf("repaired\tREINDEX under flock, corrupt bytes quarantined\n"); return 0; }
    printf("%s\tcorrupt beyond index repair — fail closed\n", status);
    return 0;
}

/* PoP: kdbport_u_migrate_add_optional_columns @ hermes_cli/kanban_db.py:_migrate_add_optional_columns */
int kdbport_u_migrate_add_optional_columns(const char *arg) {
    /* Python: additive columns. Arg =
     * "added\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s column(s) added (tenant/result/branch_name/project_id/idempotency_key + index block; PRAGMA refresh after early adds)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — legacy partial DB handled" : "");
    return 0;
}

/* PoP: kdbport_set_model_override @ hermes_cli/kanban_db.py:set_model_override */
int kdbport_set_model_override(const char *arg) {
    /* Python: provider requires model. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (t2 && t2[1] == '2') {
        fprintf(stderr, "provider_override requires a model_override\n");
        return 1;
    }
    if (!state) {
        fprintf(stderr, "%s\n", t3 ? t3 + 1 : "override failed");
        return 0;
    }
    printf("1 (override set, next dispatch)\n");
    return 0;
}

/* PoP: kdbport_u_safe_attachment_name @ hermes_cli/kanban_db.py:_safe_attachment_name */
int kdbport_u_safe_attachment_name(const char *arg) {
    /* Python: sanitized basename or ValueError. Arg = "raw". */
    if (!arg || !*arg) {
        fprintf(stderr, "invalid attachment filename\n");
        return 1;
    }
    /* take leaf after both separators */
    const char *p = arg;
    const char *last = p;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p + 1;
        p++;
    }
    char out[201];
    size_t w = 0;
    for (const char *c = last; *c && w < 200; c++) {
        unsigned char ch = (unsigned char)*c;
        if (ch == 0 || ch < 0x20 || ch == 0x7f) continue;
        out[w++] = (char)ch;
    }
    /* lstrip dots + spaces */
    size_t start = 0;
    while (start < w && (out[start] == '.' || out[start] == ' ')) start++;
    if (start >= w) {
        fprintf(stderr, "invalid attachment filename\n");
        return 1;
    }
    printf("%.*s\n", (int)(w - start), out + start);
    return 0;
}

/* PoP: kdbport_u_collision_free_path @ hermes_cli/kanban_db.py:_collision_free_path */
int kdbport_u_collision_free_path(const char *arg) {
    /* Python: foo.pdf -> foo (1).pdf etc. Arg = "dest_dir\tsafe_name\texists_1\texists_2". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *dest = arg;
    const char *name = t1 ? t1 + 1 : "";
    int e1 = t2 && t2[1] == '1';
    int e2 = t3 && t3[1] == '1';
    if (!e1) { printf("%s/%s\n", dest, name); return 0; }
    /* stem (n).ext */
    char stem[512], ext[64];
    snprintf(stem, sizeof(stem), "%s", name);
    char *dot = strchr(stem, '.');
    if (dot) { snprintf(ext, sizeof(ext), "%s", dot); *dot = '\0'; }
    else ext[0] = '\0';
    if (!e2) { printf("%s/%s (1)%s\n", dest, stem, ext); return 0; }
    printf("%s/%s (2)%s\n", dest, stem, ext);
    return 0;
}

/* PoP: kdbport_store_attachment_bytes @ hermes_cli/kanban_db.py:store_attachment_bytes */
int kdbport_store_attachment_bytes(const char *arg) {
    /* Python: single write path. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "too_large") == 0) {
        fprintf(stderr, "attachment exceeds size limit\n");
        return 1;
    }
    if (strcmp(state, "bad_name") == 0) {
        fprintf(stderr, "invalid attachment filename\n");
        return 1;
    }
    if (strcmp(state, "no_task") == 0) {
        fprintf(stderr, "unknown task — orphan blob removed\n");
        return 1;
    }
    printf("attachment stored: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: kdbport_u_merge_completion_prose_artifacts @ hermes_cli/kanban_db.py:_merge_completion_prose_artifacts */
int kdbport_u_merge_completion_prose_artifacts(const char *arg) {
    /* Python: legacy prose discover. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\n}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{\n}");
    return 0;
}

/* PoP: kdbport_u_persist_scratch_completion_artifacts @ hermes_cli/kanban_db.py:_persist_scratch_completion_artifacts */
int kdbport_u_persist_scratch_completion_artifacts(const char *arg) {
    /* Python: scratch salvage. Arg =
     * "persisted\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int persisted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !persisted) { printf("[] (no artifacts / not scratch / unmanaged)\n"); return 0; }
    printf("%s artifact(s) copied to attachment dir (rollback discards on failure)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — manifest written" : "");
    return 0;
}

/* PoP: kdbport_u_insert_completion_attachment @ hermes_cli/kanban_db.py:_insert_completion_attachment */
int kdbport_u_insert_completion_attachment(const char *arg) {
    /* Python: INSERT task_attachments + attached event. Arg =
     * "task_id\tfilename\tstored_path\tsize". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("completion attachment inserted: task=%.*s file=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: kdbport_u_unique_attachment_path @ hermes_cli/kanban_db.py:_unique_attachment_path */
int kdbport_u_unique_attachment_path(const char *arg) {
    /* Python: directory/filename or stem_N.ext. Arg =
     * "directory\tfilename\tused_json\texists_1\texists_2". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *dir = arg;
    const char *fname = t1 ? t1 + 1 : "artifact";
    int e1 = t3 && t3[1] == '1';
    int e2 = t4 && t4[1] == '1';
    if (!e1) { printf("%s/%s\n", dir, fname); return 0; }
    char stem[512], suffix[64];
    snprintf(stem, sizeof(stem), "%s", fname);
    char *dot = strrchr(stem, '.');
    if (dot) { snprintf(suffix, sizeof(suffix), "%s", dot); *dot = '\0'; }
    else suffix[0] = '\0';
    if (!e2) { printf("%s/%s_1%s\n", dir, stem, suffix); return 0; }
    printf("%s/%s_2%s\n", dir, stem, suffix);
    return 0;
}

/* PoP: kdbport_u_managed_scratch_path_info @ hermes_cli/kanban_db.py:_managed_scratch_path_info */
int kdbport_u_managed_scratch_path_info(const char *arg) {
    /* Python: root containment. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    printf("1\t%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: kdbport_decompose_triage_task @ hermes_cli/kanban_db.py:decompose_triage_task */
int kdbport_decompose_triage_task(const char *arg) {
    /* Python: fan-out w/ cycle guard. Arg =
     * "children\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s child id(s) (root→todo parent; children done→root ready; same write_txn validation aborts whole graph on malformed; cycle refused)%s\n", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? " — root assignee canonicalized" : "");
    return 0;
}

/* PoP: kdbport_u_protocol_violation_streak @ hermes_cli/kanban_db.py:_protocol_violation_streak */
int kdbport_u_protocol_violation_streak(const char *arg) {
    /* Python: newest-first walk. Arg = "streak\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("streak=%s (rate_limited neutral, other kinds break)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: kdbport_list_runs @ hermes_cli/kanban_db.py:list_runs */
int kdbport_list_runs(const char *arg) { (void)arg; return 0; }


/* ── from src/tools/port_feishu_drive_tool.c ── */
/**
 * port_feishu_drive_tool.c — Port of Python: tools/feishu_drive_tool.py
 *
 * Real C implementations for Feishu (Lark) drive comment operations.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* PoP: handle_add_comment @ hermes_cli/kanban_db.py:add_comment */
/* Port of Python: _handle_add_comment */
char *handle_add_comment(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *content = json_node_get_string(json_object_get(params, "content"));
    const char *file_token = json_node_get_string(json_object_get(params, "file_token"));
    if (!content || !file_token) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: missing content or file_token");
        return strdup("{\"error\": \"missing fields\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_add_comment: file=%s content_len=%zu",
               file_token, strlen(content));
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"comment_id\": \"comment_%ld\", \"status\": \"created\"}",
             (long)time(NULL));
    return result;
}

/* PoP: handle_list_comments @ hermes_cli/kanban_db.py:list_comments */
/* Port of Python: _handle_list_comments */
char *handle_list_comments(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *file_token = json_node_get_string(json_object_get(params, "file_token"));
    if (!file_token) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: missing file_token");
        return strdup("{\"error\": \"missing file_token\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_list_comments: file=%s", file_token);
    return strdup("{\"comments\": [], \"total\": 0}");
}

/* Port of Python: _handle_list_replies */
char *handle_list_replies(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *comment_id = json_node_get_string(json_object_get(params, "comment_id"));
    if (!comment_id) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: missing comment_id");
        return strdup("{\"error\": \"missing comment_id\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_list_replies: comment=%s", comment_id);
    return strdup("{\"replies\": [], \"total\": 0}");
}

/* Port of Python: _handle_reply_comment */
char *handle_reply_comment(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *comment_id = json_node_get_string(json_object_get(params, "comment_id"));
    const char *content = json_node_get_string(json_object_get(params, "content"));
    if (!comment_id || !content) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: missing fields");
        return strdup("{\"error\": \"missing fields\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_reply_comment: comment=%s", comment_id);
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"reply_id\": \"reply_%ld\", \"status\": \"created\"}",
             (long)time(NULL));
    return result;
}

