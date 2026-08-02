/*
 * port_lsp_servers.c — C port of agent/lsp/servers.py helper surface.
 * Server spawning helpers: file extension matching, PATH lookup,
 * nearest-root resolution, python detection. PoP-annotated per function.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: lsp_file_ext_or_basename @ agent/lsp/servers.py:_file_ext_or_basename */
char *lsp_file_ext_or_basename(const char *path) {
    /* Lower-cased extension, or the full basename for extensionless files
     * (Dockerfile/Makefile match by basename). */
    if (!path || !*path) return strdup("");
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    if (dot && dot != base) {
        char *out = strdup(dot);
        for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
        return out;
    }
    return strdup(base);
}

/* PoP: lsp_which @ agent/lsp/servers.py:_which */
char *lsp_which(const char *names_arg) {
    /* First command found on PATH; names are tab-separated. */
    if (!names_arg || !*names_arg) return NULL;
    char *copy = strdup(names_arg);
    char *save = NULL;
    char *found = NULL;
    for (char *tok = strtok_r(copy, "\t", &save); tok && !found;
         tok = strtok_r(NULL, "\t", &save)) {
        if (!*tok) continue;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "command -v %s 2>/dev/null", tok);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), fp)) {
                size_t n = strlen(buf);
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
                if (n) found = strdup(buf);
            }
            pclose(fp);
        }
    }
    free(copy);
    return found;
}

/* PoP: lsp_detect_python @ agent/lsp/servers.py:_detect_python */
char *lsp_detect_python(const char *root) {
    /* VIRTUAL_ENV, then <root>/.venv, <root>/venv with bin/python,
     * bin/python3 or Scripts/python.exe. */
    const char *ve = getenv("VIRTUAL_ENV");
    if (ve && *ve) {
        char probe[1024];
        snprintf(probe, sizeof(probe), "%s/bin/python", ve);
        if (access(probe, F_OK) == 0) return strdup(probe);
        snprintf(probe, sizeof(probe), "%s/bin/python3", ve);
        if (access(probe, F_OK) == 0) return strdup(probe);
        snprintf(probe, sizeof(probe), "%s/Scripts/python.exe", ve);
        if (access(probe, F_OK) == 0) return strdup(probe);
    }
    if (!root) return NULL;
    static const char *const venvs[] = {".venv", "venv", NULL};
    for (int v = 0; venvs[v]; v++) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/%s", root, venvs[v]);
        static const char *const subs[] = {"bin/python", "bin/python3", "Scripts/python.exe", NULL};
        for (int s = 0; subs[s]; s++) {
            char probe[1200];
            snprintf(probe, sizeof(probe), "%s/%s", dir, subs[s]);
            if (access(probe, F_OK) == 0) return strdup(probe);
        }
    }
    return NULL;
}

/* PoP: lsp_root_or_workspace @ agent/lsp/servers.py:_root_or_workspace */
char *lsp_root_or_workspace(const char *file_path, const char *markers_arg,
                            const char *excludes_arg, const char *workspace) {
    /* Walk up from the file's dir (ceiling = workspace's parent) looking
     * for the first directory containing a marker. If an exclude marker
     * matches first, the server is gated off -> NULL. Markers and excludes
     * are tab-separated name lists. */
    if (!file_path || !*file_path) return NULL;
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s", file_path);
    char *slash = strrchr(dir, '/');
    if (!slash) return NULL;
    *slash = '\0';
    /* ceiling */
    char ceiling[2048];
    if (workspace && *workspace) {
        snprintf(ceiling, sizeof(ceiling), "%s", workspace);
        char *cs = strrchr(ceiling, '/');
        if (cs) *cs = '\0';
    } else {
        ceiling[0] = '\0';
    }
    char markers[64][256];
    int nm = 0;
    char excludes[64][256];
    int nx = 0;
    if (markers_arg && *markers_arg) {
        char *copy = strdup(markers_arg);
        char *save = NULL;
        for (char *tok = strtok_r(copy, "\t", &save); tok && nm < 64;
             tok = strtok_r(NULL, "\t", &save))
            if (*tok) snprintf(markers[nm++], 256, "%s", tok);
        free(copy);
    }
    if (excludes_arg && *excludes_arg) {
        char *copy = strdup(excludes_arg);
        char *save = NULL;
        for (char *tok = strtok_r(copy, "\t", &save); tok && nx < 64;
             tok = strtok_r(NULL, "\t", &save))
            if (*tok) snprintf(excludes[nx++], 256, "%s", tok);
        free(copy);
    }
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", dir);
    while (*cur) {
        /* exclude marker hit gates the server off */
        for (int e = 0; e < nx; e++) {
            char probe[2300];
            snprintf(probe, sizeof(probe), "%s/%s", cur, excludes[e]);
            if (access(probe, F_OK) == 0) return NULL;
        }
        for (int m = 0; m < nm; m++) {
            char probe[2300];
            snprintf(probe, sizeof(probe), "%s/%s", cur, markers[m]);
            if (access(probe, F_OK) == 0) return strdup(cur);
        }
        if (ceiling[0] && strcmp(cur, ceiling) == 0) break;
        char *s = strrchr(cur, '/');
        if (!s) break;
        *s = '\0';
    }
    if (workspace && *workspace) return strdup(workspace);
    return NULL;
}

/* ── spawners: resolve binary, emit the SpawnSpec JSON ─────────────────── */

/* shared spec builder: {"command": [...], "workspace_root": root,
 * "cwd": root, "seed_diagnostics_on_first_push": bool} */
static char *lsp_spec(const char *bin, const char *root, const char *const *args,
                      int nargs, bool seed) {
    json_t *o = json_object();
    json_t *cmd = json_array();
    json_append(cmd, json_string(bin));
    for (int i = 0; i < nargs; i++) json_append(cmd, json_string(args[i]));
    json_set(o, "command", cmd);
    json_set(o, "workspace_root", json_string(root ? root : ""));
    json_set(o, "cwd", json_string(root ? root : ""));
    if (seed) json_set(o, "seed_diagnostics_on_first_push", json_bool(true));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

static char *lsp_spawn(const char *server_key, const char *root,
                       const char *const *which_names, int nwhich,
                       const char *const *args, int nargs, bool seed) {
    /* _resolve_override(ctx, key) or _which(names...); NULL when absent. */
    char names[1024];
    names[0] = '\0';
    for (int i = 0; i < nwhich; i++) {
        if (i) strcat(names, "\t");
        strcat(names, which_names[i]);
    }
    char *bin = lsp_which(names);
    if (!bin) return NULL;
    char *spec = lsp_spec(bin, root, args, nargs, seed);
    free(bin);
    return spec;
}

/* PoP: lsp_spawn_pyright @ agent/lsp/servers.py:_spawn_pyright */
char *lsp_spawn_pyright(const char *root) {
    char *bin = lsp_which("pyright-langserver\tpyright");
    if (!bin) return NULL;
    /* CLI pyright: the langserver is its sibling */
    const char *base = strrchr(bin, '/');
    base = base ? base + 1 : bin;
    if (strcmp(base, "pyright") == 0 || strcmp(base, "pyright.exe") == 0) {
        char *slash = strrchr(bin, '/');
        char sibling[1024];
        if (slash) {
            snprintf(sibling, sizeof(sibling), "%.*s/pyright-langserver",
                     (int)(slash - bin), bin);
            if (access(sibling, F_OK) == 0) { free(bin); bin = strdup(sibling); }
        }
    }
    static const char *const args[] = {"--stdio"};
    char *spec = lsp_spec(bin, root, args, 1, true);
    free(bin);
    return spec;
}

/* PoP: lsp_spawn_typescript @ agent/lsp/servers.py:_spawn_typescript */
char *lsp_spawn_typescript(const char *root) {
    static const char *const names[] = {"typescript-language-server"};
    static const char *const args[] = {"--stdio"};
    return lsp_spawn("typescript", root, names, 1, args, 1, true);
}

/* PoP: lsp_spawn_gopls @ agent/lsp/servers.py:_spawn_gopls */
char *lsp_spawn_gopls(const char *root) {
    static const char *const names[] = {"gopls"};
    return lsp_spawn("gopls", root, names, 1, NULL, 0, false);
}

/* PoP: lsp_spawn_rust_analyzer @ agent/lsp/servers.py:_spawn_rust_analyzer */
char *lsp_spawn_rust_analyzer(const char *root) {
    static const char *const names[] = {"rust-analyzer"};
    return lsp_spawn("rust-analyzer", root, names, 1, NULL, 0, false);
}

/* PoP: lsp_spawn_clangd @ agent/lsp/servers.py:_spawn_clangd */
char *lsp_spawn_clangd(const char *root) {
    static const char *const names[] = {"clangd"};
    static const char *const args[] = {"--background-index", "--clang-tidy"};
    return lsp_spawn("clangd", root, names, 1, args, 2, false);
}

/* PoP: lsp_spawn_bash_ls @ agent/lsp/servers.py:_spawn_bash_ls */
char *lsp_spawn_bash_ls(const char *root) {
    static const char *const names[] = {"bash-language-server"};
    static const char *const args[] = {"start"};
    return lsp_spawn("bash-language-server", root, names, 1, args, 1, false);
}

/* PoP: lsp_spawn_yaml_ls @ agent/lsp/servers.py:_spawn_yaml_ls */
char *lsp_spawn_yaml_ls(const char *root) {
    static const char *const names[] = {"yaml-language-server"};
    static const char *const args[] = {"--stdio"};
    return lsp_spawn("yaml-language-server", root, names, 1, args, 1, false);
}

/* PoP: lsp_spawn_lua_ls @ agent/lsp/servers.py:_spawn_lua_ls */
char *lsp_spawn_lua_ls(const char *root) {
    static const char *const names[] = {"lua-language-server"};
    return lsp_spawn("lua-language-server", root, names, 1, NULL, 0, false);
}

/* PoP: lsp_spawn_intelephense @ agent/lsp/servers.py:_spawn_intelephense */
char *lsp_spawn_intelephense(const char *root) {
    static const char *const names[] = {"intelephense"};
    static const char *const args[] = {"--stdio"};
    return lsp_spawn("intelephense", root, names, 1, args, 1, false);
}

/* PoP: lsp_spawn_ocamllsp @ agent/lsp/servers.py:_spawn_ocamllsp */
char *lsp_spawn_ocamllsp(const char *root) {
    static const char *const names[] = {"ocamllsp"};
    return lsp_spawn("ocaml-lsp", root, names, 1, NULL, 0, false);
}

/* PoP: lsp_spawn_dockerfile_ls @ agent/lsp/servers.py:_spawn_dockerfile_ls */
char *lsp_spawn_dockerfile_ls(const char *root) {
    static const char *const names[] = {"docker-langserver"};
    static const char *const args[] = {"--stdio"};
    return lsp_spawn("dockerfile-ls", root, names, 1, args, 1, false);
}

/* PoP: lsp_server_matches @ agent/lsp/servers.py:matches */
bool lsp_server_matches(const char *extensions_json, const char *file_path) {
    /* Python: ext = _file_ext_or_basename(file_path); ext in extensions. */
    if (!extensions_json || !file_path) return false;
    char *ext = lsp_file_ext_or_basename(file_path);
    json_t *arr = json_parse(extensions_json, NULL);
    bool found = false;
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *e = json_get(arr, i);
            if (e && json_is_string(e) && strcmp(json_string_value(e), ext) == 0) {
                found = true;
                break;
            }
        }
    }
    json_free(arr);
    free(ext);
    return found;
}
