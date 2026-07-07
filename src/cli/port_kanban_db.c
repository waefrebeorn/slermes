/*
 * port_kanban_db.c — Port of Python hermes_cli/kanban_db.py
 *
 * Git operations and path utilities for the Kanban board.
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
#include <errno.h>
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

/* PoP: _resolve_claim_ttl_seconds @ hermes_cli/kanban_db.py:_resolve_claim_ttl_seconds */
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

/* PoP: _resolve_crash_grace_seconds @ hermes_cli/kanban_db.py:_resolve_crash_grace_seconds */
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

/* PoP: _resolve_rate_limit_cooldown_seconds @ hermes_cli/kanban_db.py:_resolve_rate_limit_cooldown_seconds */
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

/* PoP: _relative_age @ hermes_cli/kanban_db.py:_relative_age
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

/* PoP: _normalize_board_slug @ hermes_cli/kanban_db.py:_normalize_board_slug
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

/* PoP: kanban_home @ hermes_cli/kanban_db.py:kanban_home
 * Returns malloc'd root dir. Caller frees. */
char *kanban_home(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    return strdup(root);
}

/* PoP: boards_root @ hermes_cli/kanban_db.py:boards_root */
char *kanban_boards_root(void)
{
    char root[PATH_MAX];
    kanban_root_dir(root, sizeof(root));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/kanban/boards", root);
    return strdup(out);
}

/* PoP: current_board_path @ hermes_cli/kanban_db.py:current_board_path */
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

/* PoP: get_current_board @ hermes_cli/kanban_db.py:get_current_board
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

/* PoP: set_current_board @ hermes_cli/kanban_db.py:set_current_board
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
