/*
 * port_update_cmd.c
 *
 * Pure helpers ported from hermes_cli/update_cmd.py (69 defs; most of the rest
 * are `stop_for_relaunch()` shims that must NOT be ported).
 *
 *   uc_cmd_parts       <- _called_process_error_cmd_parts
 *   uc_is_git          <- _called_process_error_is_git
 *   uc_is_dep_install  <- _called_process_error_is_python_dep_install
 *   uc_failure_stage   <- _format_update_failure_stage
 *   uc_zip_fallback    <- _should_zip_fallback_on_update_error
 *
 * The git/pip execution half stays in Python.
 */

#include "port_update_cmd.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _called_process_error_cmd_parts  (update_cmd.py:_called_process_error_cmd_parts)
 *
 *      cmd = exc.cmd
 *      if cmd is None: return []
 *      if isinstance(cmd, (str, bytes)):
 *          text = cmd.decode("utf-8", "replace") if isinstance(cmd, bytes) else cmd
 *          try: return shlex.split(text, posix=os.name != "nt")
 *          except ValueError: return text.split()
 *      return [str(part) for part in cmd]
 *
 *  `argv` is the already-tokenised form; `cmdline` is the single-string form
 *  (NULL when the error carried an argv). `posix` is 0 on Windows, 1 on POSIX.
 *  Returns a malloc'd NULL-terminated array of malloc'd strings.
 * =========================================================================== */

/* PoP: _called_process_error_cmd_parts @ hermes_cli/update_cmd.py:_called_process_error_cmd_parts */
char **uc_cmd_parts(const char *const *argv, const char *cmdline, int posix,
                    int *out_count)
{
    if (argv) {
        int n = 0;
        while (argv[n]) n++;
        char **out = (char **)calloc((size_t)n + 1, sizeof(char *));
        if (!out) { if (out_count) *out_count = 0; return NULL; }
        for (int i = 0; i < n; i++) out[i] = strdup(argv[i] ? argv[i] : "");
        if (out_count) *out_count = n;
        return out;
    }

    if (!cmdline) { if (out_count) *out_count = 0; return NULL; }

    /* shlex.split(text, posix=False): whitespace-separated, quotes kept.
     * The ValueError fallback is a plain whitespace split. */
    size_t cap = strlen(cmdline) + 1;
    char **out = (char **)calloc(cap / 2 + 8, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }
    int n = 0;

    const char *p = cmdline;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        int quoted = 0; char q = 0;
        while (*p) {
            if (quoted) { if (*p == q) { quoted = 0; p++; } else p++; }
            else if (*p == '"' || *p == '\'') { quoted = 1; q = *p; p++; }
            else if (isspace((unsigned char)*p)) break;
            else p++;
        }
        size_t len = (size_t)(p - start);
        char *tok = (char *)malloc(len + 1);
        if (tok) {
            memcpy(tok, start, len);
            tok[len] = '\0';
            /* posix=False keeps the quotes as characters */
            if (n < (int)(cap / 2 + 7)) out[n++] = tok; else free(tok);
        }
        if (!*p) break;
    }
    if (out_count) *out_count = n;
    return out;
}

/* ===========================================================================
 *  _called_process_error_is_git  (update_cmd.py:_called_process_error_is_git)
 *
 *      parts = _called_process_error_cmd_parts(exc)
 *      if not parts: return False
 *      name = os.path.basename(parts[0].replace("\\", "/")).lower()
 *      return name in {"git", "git.exe"}
 *
 *  Windows argv may use backslashes; basename() on POSIX would otherwise keep
 *  the whole path, so the separators are normalized BEFORE taking the name.
 * =========================================================================== */

/* PoP: _called_process_error_is_git @ hermes_cli/update_cmd.py:_called_process_error_is_git */
int uc_is_git(const char *const *argv)
{
    if (!argv || !argv[0] || !argv[0][0]) return 0;

    /* replace("\\", "/") then basename */
    size_t n = strlen(argv[0]);
    char *norm = (char *)malloc(n + 1);
    if (!norm) return 0;
    for (size_t i = 0; i < n; i++) norm[i] = (argv[0][i] == '\\') ? '/' : argv[0][i];
    norm[n] = '\0';
    const char *base = strrchr(norm, '/');
    base = base ? base + 1 : norm;

    /* .lower() then compare - lowercase IN PLACE from `base`, not from norm[0] */
    for (size_t i = 0; base[i]; i++)
        *(char *)(base + i) = (char)tolower((unsigned char)base[i]);
    int hit = (strcmp(base, "git") == 0 || strcmp(base, "git.exe") == 0);
    free(norm);
    return hit;
}

/* ===========================================================================
 *  _called_process_error_is_python_dep_install
 *  (update_cmd.py:_called_process_error_is_python_dep_install)
 *
 *      parts = [part.lower() for part in _called_process_error_cmd_parts(exc)]
 *      if not parts: return False
 *      exe = os.path.basename(parts[0].replace("\\", "/"))
 *      if "ensurepip" in parts: return True
 *      if "install" in parts and ("pip" in parts or exe in {"pip","pip.exe",
 *                                                        "pip3","pip3.exe","uv","uv.exe"}):
 *          return True
 *      return False
 * =========================================================================== */

/* PoP: _called_process_error_is_python_dep_install @ hermes_cli/update_cmd.py:_called_process_error_is_python_dep_install */
int uc_is_dep_install(const char *const *argv)
{
    if (!argv || !argv[0]) return 0;

    int n = 0;
    while (argv[n]) n++;
    if (n == 0) return 0;

    /* lowercase copies */
    char **parts = (char **)calloc((size_t)n + 1, sizeof(char *));
    if (!parts) return 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(argv[i] ? argv[i] : "");
        parts[i] = (char *)malloc(l + 1);
        if (parts[i]) {
            for (size_t j = 0; j < l; j++)
                parts[i][j] = (char)tolower((unsigned char)(argv[i][j]));
            parts[i][l] = '\0';
        }
    }

    /* exe = basename(parts[0] with \ -> /) */
    const char *p0 = parts[0] ? parts[0] : "";
    const char *slash = strrchr(p0, '/');
    const char *exe = slash ? slash + 1 : p0;

    int hit = 0;
    int has_install = 0, has_pip = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i]) continue;
        if (strcmp(parts[i], "ensurepip") == 0) hit = 1;
        if (strcmp(parts[i], "install") == 0) has_install = 1;
        if (strcmp(parts[i], "pip") == 0) has_pip = 1;
    }
    if (!hit && has_install) {
        if (has_pip) hit = 1;
        else if (strcmp(exe, "pip") == 0 || strcmp(exe, "pip.exe") == 0 ||
                 strcmp(exe, "pip3") == 0 || strcmp(exe, "pip3.exe") == 0 ||
                 strcmp(exe, "uv") == 0 || strcmp(exe, "uv.exe") == 0)
            hit = 1;
    }

    for (int i = 0; i < n; i++) free(parts[i]);
    free(parts);
    return hit;
}

/* ===========================================================================
 *  _format_update_failure_stage  (update_cmd.py:_format_update_failure_stage)
 *
 *      if _called_process_error_is_python_dep_install(exc):
 *          return "Python dependency install failed"
 *      if _called_process_error_is_git(exc): return "Git update failed"
 *      return "Update step failed"
 *
 *  The git pull and the Python-dependency install share one try block; calling
 *  every CalledProcessError a git failure sent users hunting in the wrong place
 *  (#87304, #85840). Caller frees.
 * =========================================================================== */

/* PoP: _format_update_failure_stage @ hermes_cli/update_cmd.py:_format_update_failure_stage */
char *uc_failure_stage(const char *const *argv)
{
    if (uc_is_dep_install(argv)) return strdup("Python dependency install failed");
    if (uc_is_git(argv)) return strdup("Git update failed");
    return strdup("Update step failed");
}

/* ===========================================================================
 *  _should_zip_fallback_on_update_error
 *  (update_cmd.py:_should_zip_fallback_on_update_error)
 *
 *      return (isinstance(exc, subprocess.CalledProcessError)
 *              and _m()._is_windows()
 *              and _called_process_error_is_git(exc)
 *              and _commit.run_checkout_untouched(_m().PROJECT_ROOT))
 *
 *  ZIP fallback is for Windows git file-I/O breakage ONLY. A dependency-install
 *  failure is not a git failure, and re-downloading the source ZIP would replace
 *  every top-level entry except venv / node_modules / .git / .env - permanently
 *  deleting uncommitted edits and untracked files. `is_cpe`, `is_windows` and
 *  `checkout_untouched` stand in for the three externals.
 * =========================================================================== */

/* PoP: _should_zip_fallback_on_update_error @ hermes_cli/update_cmd.py:_should_zip_fallback_on_update_error */
int uc_zip_fallback(const char *const *argv, int is_cpe, int is_windows,
                    int checkout_untouched)
{
    return is_cpe && is_windows && uc_is_git(argv) && checkout_untouched;
}
