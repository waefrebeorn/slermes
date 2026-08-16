/*
 * ports/agent/lsp/servers.c — C11 port of agent/lsp/servers.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_lsp_servers.c ── */
/*
 * port_lsp_servers.c — C port of agent/lsp/servers.py helper surface.
 * Server spawning helpers: file extension matching, PATH lookup,
 * nearest-root resolution, python detection. PoP-annotated per function.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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


/* ── from src/agent/port_lsp_servers_ports.c ── */
/*
 * port_lsp_servers_remaining.c — Port of agent/lsp/servers.py spawn specs
 * and project-root detection. Each _spawn_* resolves the binary (override
 * or PATH), returning NULL when absent; root detection walks markers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static char *which(const char *bin) {
    if (!bin || !*bin) return NULL;
    if (strchr(bin, '/')) return access(bin, X_OK) == 0 ? strdup(bin) : NULL;
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *copy = strdup(path);
    char *out = NULL;
    char *tok = strtok(copy, ":");
    while (tok) {
        char *cand = NULL;
        asprintf(&cand, "%s/%s", tok, bin);
        if (cand && access(cand, X_OK) == 0) { out = cand; break; }
        free(cand);
        tok = strtok(NULL, ":");
    }
    free(copy);
    return out;
}

static char *root_or_workspace(const char *file_path, const char *workspace,
                               const char *markers_csv, const char *alt1, const char *alt2) {
    (void)file_path;
    if (!workspace) return NULL;
    char *markers = strdup(markers_csv ? markers_csv : "");
    char *found = NULL;
    char *tok = strtok(markers, ",");
    while (tok) {
        char *cand = NULL;
        asprintf(&cand, "%s/%s", workspace, tok);
        if (cand && access(cand, F_OK) == 0) { found = cand; break; }
        free(cand);
        tok = strtok(NULL, ",");
    }
    free(markers);
    if (found) return found;
    if (alt1) {
        char *cand = NULL;
        asprintf(&cand, "%s/%s", workspace, alt1);
        if (cand && access(cand, F_OK) == 0) { found = cand; }
        else free(cand);
        if (found) return found;
    }
    if (alt2) {
        char *cand = NULL;
        asprintf(&cand, "%s/%s", workspace, alt2);
        if (cand && access(cand, F_OK) == 0) { found = cand; }
        else free(cand);
        if (found) return found;
    }
    return strdup(workspace);
}

/* PoP: lsv_resolve_override @ agent/lsp/servers.py:_resolve_override */
char *lsv_resolve_override(const char *overrides_json, const char *server_id) {
    /* Python: config-pinned binary path. */
    if (!overrides_json || !server_id) return NULL;
    if (strstr(overrides_json, server_id)) {
        const char *p = strstr(overrides_json, server_id);
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"' || *q == '[') q++;
            const char *e = q;
            while (*e && *e != '"' && *e != ',' && *e != ']') e++;
            char *path = strndup(q, (size_t)(e - q));
            if (path && *path && access(path, X_OK) == 0) return path;
            free(path);
        }
    }
    return NULL;
}

/* PoP: lsv_spawn_terraform_ls @ agent/lsp/servers.py:_spawn_terraform_ls */
char *lsv_spawn_terraform_ls(const char *override_path) {
    /* Python: terraform-ls is heavy to auto-install; require present. */
    char *bin = override_path ? strdup(override_path) : which("terraform-ls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s serve", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_dart @ agent/lsp/servers.py:_spawn_dart */
char *lsv_spawn_dart(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("dart");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s language-server --protocol=lsp", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_haskell_ls @ agent/lsp/servers.py:_spawn_haskell_ls */
char *lsv_spawn_haskell_ls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("haskell-language-server-wrapper");
    if (!bin) {
        bin = which("haskell-language-server");
        if (!bin) return NULL;
    }
    char *out = NULL;
    asprintf(&out, "%s --lsp", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_julia @ agent/lsp/servers.py:_spawn_julia */
char *lsv_spawn_julia(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("julia");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s --startup-file=no --history-file=no -e 'using LanguageServer; runserver()'",
             bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_clojure_lsp @ agent/lsp/servers.py:_spawn_clojure_lsp */
char *lsv_spawn_clojure_lsp(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("clojure-lsp");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_nixd @ agent/lsp/servers.py:_spawn_nixd */
char *lsv_spawn_nixd(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("nixd");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_zls @ agent/lsp/servers.py:_spawn_zls */
char *lsv_spawn_zls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("zls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_gleam @ agent/lsp/servers.py:_spawn_gleam */
char *lsv_spawn_gleam(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("gleam");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s lsp", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_elixir_ls @ agent/lsp/servers.py:_spawn_elixir_ls */
char *lsv_spawn_elixir_ls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("elixir-ls");
    if (!bin) {
        bin = which("language_server.sh");
        if (!bin) return NULL;
    }
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_prisma @ agent/lsp/servers.py:_spawn_prisma */
char *lsv_spawn_prisma(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("prisma-language-server");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s --stdio", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_kotlin_ls @ agent/lsp/servers.py:_spawn_kotlin_ls */
char *lsv_spawn_kotlin_ls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("kotlin-language-server");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_jdtls @ agent/lsp/servers.py:_spawn_jdtls */
char *lsv_spawn_jdtls(const char *override_path) {
    /* Python: complex install — wrapper script required. */
    char *bin = override_path ? strdup(override_path) : which("jdtls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_vue @ agent/lsp/servers.py:_spawn_vue */
char *lsv_spawn_vue(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("vue-language-server");
    if (!bin) {
        bin = which("vls");
        if (!bin) return NULL;
    }
    char *out = NULL;
    asprintf(&out, "%s --stdio", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_svelte @ agent/lsp/servers.py:_spawn_svelte */
char *lsv_spawn_svelte(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("svelteserver");
    if (!bin) {
        bin = which("svelte-language-server");
        if (!bin) return NULL;
    }
    char *out = NULL;
    asprintf(&out, "%s --stdio", bin);
    free(bin);
    return out;
}

/* PoP: lsv_spawn_astro @ agent/lsp/servers.py:_spawn_astro */
char *lsv_spawn_astro(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("astro-ls");
    if (!bin) {
        bin = which("astro-language-server");
        if (!bin) return NULL;
    }
    char *out = NULL;
    asprintf(&out, "%s --stdio", bin);
    free(bin);
    return out;
}

/* PoP: lsv_find_pses_bundle @ agent/lsp/servers.py:_find_pses_bundle */
char *lsv_find_pses_bundle(const char *hermes_home) {
    /* Python: PSES GitHub release zip — no auto-install recipe. */
    if (!hermes_home) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/lsp/pses", hermes_home);
    return out;
}

/* PoP: lsv_spawn_powershell_es @ agent/lsp/servers.py:_spawn_powershell_es */
char *lsv_spawn_powershell_es(const char *pwsh_path, const char *bundle_dir) {
    /* Python: bootstrap script over stdio. */
    if (!pwsh_path || !bundle_dir) return NULL;
    char *out = NULL;
    asprintf(&out, "%s -NoProfile -ExecutionPolicy Bypass -File %s/Start-EditorServices.ps1",
             pwsh_path, bundle_dir);
    return out;
}

/* PoP: lsv_hermes_lsp_session_dir @ agent/lsp/servers.py:hermes_lsp_session_dir */
char *lsv_hermes_lsp_session_dir(const char *hermes_home) {
    /* Python: $HERMES_HOME or ~/.hermes + /lsp-sessions, created. */
    char *out = NULL;
    asprintf(&out, "%s/lsp-sessions", hermes_home ? hermes_home : ".hermes");
    return out;
}

/* PoP: lsv_root_python @ agent/lsp/servers.py:_root_python */
char *lsv_root_python(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "pyproject.toml,setup.py,setup.cfg,requirements.txt,Pipfile,pyrightconfig.json", NULL, NULL);
}

/* PoP: lsv_root_typescript @ agent/lsp/servers.py:_root_typescript */
char *lsv_root_typescript(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "package-lock.json,bun.lockb,bun.lock,yarn.lock,pnpm-lock.yaml,tsconfig.json", NULL, NULL);
}

/* PoP: lsv_root_go @ agent/lsp/servers.py:_root_go */
char *lsv_root_go(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "go.work,go.mod,go.sum", NULL, NULL);
}

/* PoP: lsv_root_rust @ agent/lsp/servers.py:_root_rust */
char *lsv_root_rust(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Cargo.toml,Cargo.lock", NULL, NULL);
}

/* PoP: lsv_root_ruby @ agent/lsp/servers.py:_root_ruby */
char *lsv_root_ruby(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Gemfile", NULL, NULL);
}

/* PoP: lsv_root_clangd @ agent/lsp/servers.py:_root_clangd */
char *lsv_root_clangd(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "compile_commands.json,compile_flags.txt,.clangd", NULL, NULL);
}

/* PoP: lsv_root_bash @ agent/lsp/servers.py:_root_bash */
char *lsv_root_bash(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: lsv_root_yaml @ agent/lsp/servers.py:_root_yaml */
char *lsv_root_yaml(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: lsv_root_lua @ agent/lsp/servers.py:_root_lua */
char *lsv_root_lua(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        ".luarc.json,.luarc.jsonc,.luacheckrc,.stylua.toml,stylua.toml,selene.toml,selene.toml", NULL, NULL);
}

/* PoP: lsv_root_php @ agent/lsp/servers.py:_root_php */
char *lsv_root_php(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "composer.json,composer.lock,.php-version", NULL, NULL);
}

/* PoP: lsv_root_ocaml @ agent/lsp/servers.py:_root_ocaml */
char *lsv_root_ocaml(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "dune-project,dune-workspace,.merlin,opam", NULL, NULL);
}

/* PoP: lsv_root_docker @ agent/lsp/servers.py:_root_docker */
char *lsv_root_docker(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: lsv_root_terraform @ agent/lsp/servers.py:_root_terraform */
char *lsv_root_terraform(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, ".terraform.lock.hcl,terraform.tfstate", NULL, NULL);
}

/* PoP: lsv_root_dart @ agent/lsp/servers.py:_root_dart */
char *lsv_root_dart(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "pubspec.yaml,analysis_options.yaml", NULL, NULL);
}

/* PoP: lsv_root_haskell @ agent/lsp/servers.py:_root_haskell */
char *lsv_root_haskell(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "stack.yaml,cabal.project,hie.yaml", NULL, NULL);
}

/* PoP: lsv_root_julia @ agent/lsp/servers.py:_root_julia */
char *lsv_root_julia(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Project.toml,Manifest.toml", NULL, NULL);
}

/* PoP: lsv_root_clojure @ agent/lsp/servers.py:_root_clojure */
char *lsv_root_clojure(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "deps.edn,project.clj,shadow-cljs.edn,bb.edn,build.boot", NULL, NULL);
}

/* PoP: lsv_root_nix @ agent/lsp/servers.py:_root_nix */
char *lsv_root_nix(const char *file_path, const char *workspace) {
    char *found = root_or_workspace(file_path, workspace, "flake.nix", NULL, NULL);
    if (found) return found;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: lsv_root_zig @ agent/lsp/servers.py:_root_zig */
char *lsv_root_zig(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "build.zig", NULL, NULL);
}

/* PoP: lsv_root_elixir @ agent/lsp/servers.py:_root_elixir */
char *lsv_root_elixir(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "mix.exs,mix.lock", NULL, NULL);
}

/* PoP: lsv_root_prisma @ agent/lsp/servers.py:_root_prisma */
char *lsv_root_prisma(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "schema.prisma,prisma/schema.prisma", NULL, NULL);
}

/* PoP: lsv_root_kotlin @ agent/lsp/servers.py:_root_kotlin */
char *lsv_root_kotlin(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "settings.gradle,settings.gradle.kts,build.gradle,build.gradle.kts,pom.xml", NULL, NULL);
}

/* PoP: lsv_root_java @ agent/lsp/servers.py:_root_java */
char *lsv_root_java(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "pom.xml,build.gradle,build.gradle.kts,.project,.classpath,settings.gradle", NULL, NULL);
}

/* PoP: lsv_root_powershell @ agent/lsp/servers.py:_root_powershell */
char *lsv_root_powershell(const char *file_path, const char *workspace) {
    /* Python: PSScriptAnalyzer settings or git root. */
    char *found = root_or_workspace(file_path, workspace, "PSScriptAnalyzerSettings.psd1", NULL, NULL);
    if (found) return found;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: lsv_find_server_for_file @ agent/lsp/servers.py:find_server_for_file */
char *lsv_find_server_for_file(const char *file_path) {
    /* Python: first registry entry matching the path. */
    if (!file_path) return NULL;
    const char *dot = strrchr(file_path, '.');
    if (!dot) return NULL;
    static const struct { const char *ext, *server; } map[] = {
        {".py", "python"}, {".ts", "typescript"}, {".tsx", "typescript"},
        {".js", "typescript"}, {".go", "go"}, {".rs", "rust"}, {".rb", "ruby"},
        {".c", "clangd"}, {".h", "clangd"}, {".cpp", "clangd"}, {".sh", "bash"},
        {".yaml", "yaml"}, {".yml", "yaml"}, {".lua", "lua"}, {".php", "php"},
        {".ml", "ocaml"}, {".mli", "ocaml"}, {".tf", "terraform"},
        {".dart", "dart"}, {".hs", "haskell"}, {".jl", "julia"},
        {".clj", "clojure"}, {".nix", "nix"}, {".zig", "zig"},
        {".ex", "elixir"}, {".exs", "elixir"}, {".prisma", "prisma"},
        {".kt", "kotlin"}, {".kts", "kotlin"}, {".java", "java"},
        {".ps1", "powershell"}, {".vue", "vue"}, {".svelte", "svelte"},
        {".astro", "astro"}, {"" , ""},
    };
    for (int i = 0; map[i].ext[0]; i++)
        if (strcmp(dot, map[i].ext) == 0) return strdup(map[i].server);
    return NULL;
}

/* PoP: lsv_language_id_for @ agent/lsp/servers.py:language_id_for */
char *lsv_language_id_for(const char *path) {
    /* Python: LANGUAGE_BY_EXT or plaintext. */
    if (!path) return strdup("plaintext");
    const char *dot = strrchr(path, '.');
    if (!dot) return strdup("plaintext");
    static const struct { const char *ext, *lang; } map[] = {
        {".py", "python"}, {".ts", "typescript"}, {".tsx", "typescriptreact"},
        {".js", "javascript"}, {".jsx", "javascriptreact"}, {".go", "go"},
        {".rs", "rust"}, {".rb", "ruby"}, {".c", "c"}, {".h", "c"},
        {".cpp", "cpp"}, {".sh", "shellscript"}, {".yaml", "yaml"},
        {".yml", "yaml"}, {".lua", "lua"}, {".php", "php"}, {".ml", "ocaml"},
        {".tf", "terraform"}, {".dart", "dart"}, {".hs", "haskell"},
        {".jl", "julia"}, {".clj", "clojure"}, {".nix", "nix"},
        {".zig", "zig"}, {".ex", "elixir"}, {".exs", "elixir"},
        {".kt", "kotlin"}, {".kts", "kotlin"}, {".java", "java"},
        {".ps1", "powershell"}, {".vue", "vue"}, {".svelte", "svelte"},
        {".astro", "astro"}, {".md", "markdown"}, {".json", "json"},
        {"" , ""},
    };
    for (int i = 0; map[i].ext[0]; i++)
        if (strcmp(dot, map[i].ext) == 0) return strdup(map[i].lang);
    return strdup("plaintext");
}

