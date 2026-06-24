/*
 * port_main.c — Port of Python hermes_cli/main.py
 *
 * C implementations for name parity.
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

/*
 * _electron_dir — Return the Electron package directory.
 *
 * Python: def _electron_dir(project_root: Path) -> Path:
 *   desktop_local = project_root / "apps" / "desktop" / "node_modules" / "electron"
 *   if desktop_local.exists(): return desktop_local
 *   return project_root / "node_modules" / "electron"
 */
/* Port of Python: _electron_dir */
const char* _electron_dir(const char* project_root)
{
    if (!project_root || !project_root[0]) {
        project_root = ".";
    }

    static char path[4096];

    /* Try workspace-local first: apps/desktop/node_modules/electron */
    snprintf(path, sizeof(path), "%s/apps/desktop/node_modules/electron", project_root);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return path;
    }

    /* Fallback: root hoist node_modules/electron */
    snprintf(path, sizeof(path), "%s/node_modules/electron", project_root);
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return path;
    }

    /* Not found — return the workspace-local path anyway (caller checks exists) */
    snprintf(path, sizeof(path), "%s/apps/desktop/node_modules/electron", project_root);
    return path;
}

/*
 * _electron_dist_binary — Return the path to the Electron main binary.
 *
 * Python: def _electron_dist_binary(project_root: Path) -> Path:
 *   dist = _electron_dir(project_root) / "dist"
 *   if sys.platform == "darwin": return dist / "Electron.app" / "Contents" / "MacOS" / "Electron"
 *   if sys.platform == "win32": return dist / "electron.exe"
 *   return dist / "electron"
 */
/* Port of Python: _electron_dist_binary */
const char* _electron_dist_binary(const char* project_root)
{
    const char* electron = _electron_dir(project_root);
    static char path[4096];

#if defined(__APPLE__)
    snprintf(path, sizeof(path), "%s/dist/Electron.app/Contents/MacOS/Electron", electron);
#elif defined(_WIN32)
    snprintf(path, sizeof(path), "%s/dist/electron.exe", electron);
#else
    snprintf(path, sizeof(path), "%s/dist/electron", electron);
#endif

    return path;
}

/*
 * _electron_dist_ok — Check if electron dist has a usable binary.
 *
 * Python: def _electron_dist_ok(project_root: Path) -> bool:
 *   try: return _electron_dist_binary(project_root).exists()
 *   except OSError: return false
 */
/* Port of Python: _electron_dist_ok */
bool _electron_dist_ok(const char* project_root)
{
    const char* binary = _electron_dist_binary(project_root);
    struct stat st;
    if (stat(binary, &st) != 0) {
        return false;
    }
    /* Check it's executable */
    return (st.st_mode & S_IXUSR) != 0;
}

/*
 * _build_provider_choices — Build the --provider choices list.
 *
 * Python: def _build_provider_choices() -> list[str]:
 *   try:
 *       from hermes_cli.models import CANONICAL_PROVIDERS as _cp
 *       return ["auto"] + [p.slug for p in _cp]
 *   except Exception:
 *       return ["auto", "openrouter", "nous", ...]
 *
 * In C: return a JSON array of provider slug strings.
 */
/* Port of Python: _build_provider_choices */
json_t* _build_provider_choices(void)
{
    json_t* choices = json_new_array();
    if (!choices) return NULL;

    /* Static fallback list — matches the Python fallback */
    static const char* providers[] = {
        "auto", "openrouter", "nous", "openai-codex", "xai-oauth", "copilot-acp",
        "copilot", "anthropic", "gemini", "google-gemini-cli", "google-antigravity",
        "xai", "bedrock", "azure-foundry", "ollama-cloud", "huggingface", "zai",
        "kimi-coding", "kimi-coding-cn", "stepfun", "minimax", "minimax-cn",
        "kilocode", "novita", "xiaomi", "arcee", "nvidia", "deepseek", "alibaba",
        "qwen-oauth", "opencode-zen", "opencode-go", NULL
    };

    for (int i = 0; providers[i]; i++) {
        json_array_append(choices, json_new_string(providers[i]));
    }

    return choices;
}

/*
 * _restore_tui_workspace — Try to restore a missing ui-tui/ from git.
 *
 * Python: def _restore_tui_workspace(tui_dir: Path) -> bool:
 *   git = shutil.which("git")
 *   if not git or not (tui_dir.parent / ".git").exists(): return False
 *   try:
 *       subprocess.run([git, "restore", str(tui_dir)], cwd=tui_dir.parent, ...)
 *       return tui_dir.is_dir()
 *   except: return False
 */
/* Port of Python: _restore_tui_workspace */
bool _restore_tui_workspace(const char* tui_dir)
{
    if (!tui_dir || !tui_dir[0]) return false;

    /* Check if .git exists in parent dir */
    char git_dir[4096];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", tui_dir);
    /* tui_dir is the ui-tui dir itself — check parent */
    char* parent = strdup(tui_dir);
    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        snprintf(git_dir, sizeof(git_dir), "%s/.git", parent);
    }
    free(parent);

    struct stat st;
    if (stat(git_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return false;
    }

    /* Check git is available */
    if (access("/usr/bin/git", X_OK) != 0 && access("/usr/local/bin/git", X_OK) != 0) {
        return false;
    }

    /* Run git restore */
    char ui_tui_name[256];
    snprintf(ui_tui_name, sizeof(ui_tui_name), "%s/ui-tui", tui_dir);

    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        execlp("git", "git", "restore", "--", "ui-tui", NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            /* Check if directory was restored */
            if (stat(ui_tui_name, &st) == 0 && S_ISDIR(st.st_mode)) {
                return true;
            }
        }
    }

    return false;
}

/*
 * _ensure_tui_workspace — Ensure ui-tui/ exists before npm/node uses it.
 *
 * Python: def _ensure_tui_workspace(tui_dir: Path) -> None:
 *   if tui_dir.is_dir(): return
 *   if _restore_tui_workspace(tui_dir):
 *       if not os.environ.get("HERMES_QUIET"):
 *           print(f"Restored missing TUI workspace: {tui_dir}")
 *       return
 *   raise SystemExit(...)
 */
/* Port of Python: _ensure_tui_workspace */
void _ensure_tui_workspace(const char* tui_dir)
{
    if (!tui_dir || !tui_dir[0]) return;

    struct stat st;
    if (stat(tui_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return; /* Already exists */
    }

    if (_restore_tui_workspace(tui_dir)) {
        hermes_log(LOG_INFO, "port", "Restored missing TUI workspace: %s", tui_dir);
        return;
    }

    hermes_log(LOG_ERROR, "port", "TUI workspace missing: %s", tui_dir);
    return;
}

/*
 * _atomic_replace_dir — Atomically replace a directory.
 *
 * Python: def atomic_replace_dir(src: Path, dst: Path) -> None:
 *   tmp = dst.with_suffix('.tmp')
 *   shutil.copytree(src, tmp)
 *   tmp.replace(dst)
 */
/* Port of Python: _atomic_replace_dir */
void _atomic_replace_dir(const char* src, const char* dst)
{
    if (!src || !dst) return;

    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return;
    }

    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);

    /* Remove old tmp if it exists */
    char rm_cmd[4096];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", tmp);
    system(rm_cmd);

    /* Copy to tmp */
    char cp_cmd[8192];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp -a '%s' '%s'", src, tmp);
    int ret = system(cp_cmd);
    if (ret != 0) {
        return;
    }

    /* Atomic rename */
    if (rename(tmp, dst) != 0) {
        /* Fallback: rm dst, mv tmp dst */
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", dst);
        system(rm_cmd);
        if (rename(tmp, dst) != 0) {
            return;
        }
    }

    return;
}
