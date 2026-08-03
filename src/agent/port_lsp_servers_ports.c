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

/* PoP: _resolve_override @ agent/lsp/servers.py:_resolve_override */
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

/* PoP: _spawn_terraform_ls @ agent/lsp/servers.py:_spawn_terraform_ls */
char *lsv_spawn_terraform_ls(const char *override_path) {
    /* Python: terraform-ls is heavy to auto-install; require present. */
    char *bin = override_path ? strdup(override_path) : which("terraform-ls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s serve", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_dart @ agent/lsp/servers.py:_spawn_dart */
char *lsv_spawn_dart(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("dart");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s language-server --protocol=lsp", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_haskell_ls @ agent/lsp/servers.py:_spawn_haskell_ls */
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

/* PoP: _spawn_julia @ agent/lsp/servers.py:_spawn_julia */
char *lsv_spawn_julia(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("julia");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s --startup-file=no --history-file=no -e 'using LanguageServer; runserver()'",
             bin);
    free(bin);
    return out;
}

/* PoP: _spawn_clojure_lsp @ agent/lsp/servers.py:_spawn_clojure_lsp */
char *lsv_spawn_clojure_lsp(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("clojure-lsp");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_nixd @ agent/lsp/servers.py:_spawn_nixd */
char *lsv_spawn_nixd(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("nixd");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_zls @ agent/lsp/servers.py:_spawn_zls */
char *lsv_spawn_zls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("zls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_gleam @ agent/lsp/servers.py:_spawn_gleam */
char *lsv_spawn_gleam(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("gleam");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s lsp", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_elixir_ls @ agent/lsp/servers.py:_spawn_elixir_ls */
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

/* PoP: _spawn_prisma @ agent/lsp/servers.py:_spawn_prisma */
char *lsv_spawn_prisma(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("prisma-language-server");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s --stdio", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_kotlin_ls @ agent/lsp/servers.py:_spawn_kotlin_ls */
char *lsv_spawn_kotlin_ls(const char *override_path) {
    char *bin = override_path ? strdup(override_path) : which("kotlin-language-server");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_jdtls @ agent/lsp/servers.py:_spawn_jdtls */
char *lsv_spawn_jdtls(const char *override_path) {
    /* Python: complex install — wrapper script required. */
    char *bin = override_path ? strdup(override_path) : which("jdtls");
    if (!bin) return NULL;
    char *out = NULL;
    asprintf(&out, "%s", bin);
    free(bin);
    return out;
}

/* PoP: _spawn_vue @ agent/lsp/servers.py:_spawn_vue */
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

/* PoP: _spawn_svelte @ agent/lsp/servers.py:_spawn_svelte */
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

/* PoP: _spawn_astro @ agent/lsp/servers.py:_spawn_astro */
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

/* PoP: _find_pses_bundle @ agent/lsp/servers.py:_find_pses_bundle */
char *lsv_find_pses_bundle(const char *hermes_home) {
    /* Python: PSES GitHub release zip — no auto-install recipe. */
    if (!hermes_home) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/lsp/pses", hermes_home);
    return out;
}

/* PoP: _spawn_powershell_es @ agent/lsp/servers.py:_spawn_powershell_es */
char *lsv_spawn_powershell_es(const char *pwsh_path, const char *bundle_dir) {
    /* Python: bootstrap script over stdio. */
    if (!pwsh_path || !bundle_dir) return NULL;
    char *out = NULL;
    asprintf(&out, "%s -NoProfile -ExecutionPolicy Bypass -File %s/Start-EditorServices.ps1",
             pwsh_path, bundle_dir);
    return out;
}

/* PoP: hermes_lsp_session_dir @ agent/lsp/servers.py:hermes_lsp_session_dir */
char *lsv_hermes_lsp_session_dir(const char *hermes_home) {
    /* Python: $HERMES_HOME or ~/.hermes + /lsp-sessions, created. */
    char *out = NULL;
    asprintf(&out, "%s/lsp-sessions", hermes_home ? hermes_home : ".hermes");
    return out;
}

/* PoP: _root_python @ agent/lsp/servers.py:_root_python */
char *lsv_root_python(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "pyproject.toml,setup.py,setup.cfg,requirements.txt,Pipfile,pyrightconfig.json", NULL, NULL);
}

/* PoP: _root_typescript @ agent/lsp/servers.py:_root_typescript */
char *lsv_root_typescript(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "package-lock.json,bun.lockb,bun.lock,yarn.lock,pnpm-lock.yaml,tsconfig.json", NULL, NULL);
}

/* PoP: _root_go @ agent/lsp/servers.py:_root_go */
char *lsv_root_go(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "go.work,go.mod,go.sum", NULL, NULL);
}

/* PoP: _root_rust @ agent/lsp/servers.py:_root_rust */
char *lsv_root_rust(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Cargo.toml,Cargo.lock", NULL, NULL);
}

/* PoP: _root_ruby @ agent/lsp/servers.py:_root_ruby */
char *lsv_root_ruby(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Gemfile", NULL, NULL);
}

/* PoP: _root_clangd @ agent/lsp/servers.py:_root_clangd */
char *lsv_root_clangd(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "compile_commands.json,compile_flags.txt,.clangd", NULL, NULL);
}

/* PoP: _root_bash @ agent/lsp/servers.py:_root_bash */
char *lsv_root_bash(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: _root_yaml @ agent/lsp/servers.py:_root_yaml */
char *lsv_root_yaml(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: _root_lua @ agent/lsp/servers.py:_root_lua */
char *lsv_root_lua(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        ".luarc.json,.luarc.jsonc,.luacheckrc,.stylua.toml,stylua.toml,selene.toml,selene.toml", NULL, NULL);
}

/* PoP: _root_php @ agent/lsp/servers.py:_root_php */
char *lsv_root_php(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "composer.json,composer.lock,.php-version", NULL, NULL);
}

/* PoP: _root_ocaml @ agent/lsp/servers.py:_root_ocaml */
char *lsv_root_ocaml(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "dune-project,dune-workspace,.merlin,opam", NULL, NULL);
}

/* PoP: _root_docker @ agent/lsp/servers.py:_root_docker */
char *lsv_root_docker(const char *file_path, const char *workspace) {
    (void)file_path;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: _root_terraform @ agent/lsp/servers.py:_root_terraform */
char *lsv_root_terraform(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, ".terraform.lock.hcl,terraform.tfstate", NULL, NULL);
}

/* PoP: _root_dart @ agent/lsp/servers.py:_root_dart */
char *lsv_root_dart(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "pubspec.yaml,analysis_options.yaml", NULL, NULL);
}

/* PoP: _root_haskell @ agent/lsp/servers.py:_root_haskell */
char *lsv_root_haskell(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "stack.yaml,cabal.project,hie.yaml", NULL, NULL);
}

/* PoP: _root_julia @ agent/lsp/servers.py:_root_julia */
char *lsv_root_julia(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "Project.toml,Manifest.toml", NULL, NULL);
}

/* PoP: _root_clojure @ agent/lsp/servers.py:_root_clojure */
char *lsv_root_clojure(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "deps.edn,project.clj,shadow-cljs.edn,bb.edn,build.boot", NULL, NULL);
}

/* PoP: _root_nix @ agent/lsp/servers.py:_root_nix */
char *lsv_root_nix(const char *file_path, const char *workspace) {
    char *found = root_or_workspace(file_path, workspace, "flake.nix", NULL, NULL);
    if (found) return found;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: _root_zig @ agent/lsp/servers.py:_root_zig */
char *lsv_root_zig(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "build.zig", NULL, NULL);
}

/* PoP: _root_elixir @ agent/lsp/servers.py:_root_elixir */
char *lsv_root_elixir(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "mix.exs,mix.lock", NULL, NULL);
}

/* PoP: _root_prisma @ agent/lsp/servers.py:_root_prisma */
char *lsv_root_prisma(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace, "schema.prisma,prisma/schema.prisma", NULL, NULL);
}

/* PoP: _root_kotlin @ agent/lsp/servers.py:_root_kotlin */
char *lsv_root_kotlin(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "settings.gradle,settings.gradle.kts,build.gradle,build.gradle.kts,pom.xml", NULL, NULL);
}

/* PoP: _root_java @ agent/lsp/servers.py:_root_java */
char *lsv_root_java(const char *file_path, const char *workspace) {
    return root_or_workspace(file_path, workspace,
        "pom.xml,build.gradle,build.gradle.kts,.project,.classpath,settings.gradle", NULL, NULL);
}

/* PoP: _root_powershell @ agent/lsp/servers.py:_root_powershell */
char *lsv_root_powershell(const char *file_path, const char *workspace) {
    /* Python: PSScriptAnalyzer settings or git root. */
    char *found = root_or_workspace(file_path, workspace, "PSScriptAnalyzerSettings.psd1", NULL, NULL);
    if (found) return found;
    return workspace ? strdup(workspace) : NULL;
}

/* PoP: find_server_for_file @ agent/lsp/servers.py:find_server_for_file */
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

/* PoP: language_id_for @ agent/lsp/servers.py:language_id_for */
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
