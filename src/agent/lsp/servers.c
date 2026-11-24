/*
 * servers.c - C11 port of upstream agent/lsp/servers.py
 * (gate w33_servers_a4580e).
 *
 * PoP: w33_servers_a4580e_ext_or_basename   @ agent/lsp/servers.py:_file_ext_or_basename
 * PoP: w33_servers_a4580e_language_id       @ agent/lsp/servers.py:language_id_for (+ LANGUAGE_BY_EXT)
 * PoP: w33_servers_a4580e_language_table    @ agent/lsp/servers.py:LANGUAGE_BY_EXT
 * PoP: w33_servers_a4580e_matches           @ agent/lsp/servers.py:ServerDef.matches
 * PoP: w33_servers_a4580e_find_server       @ agent/lsp/servers.py:find_server_for_file
 * PoP: w33_servers_a4580e_registry          @ agent/lsp/servers.py:SERVERS
 * PoP: w33_servers_a4580e_custom_servers    @ agent/lsp/servers.py:custom_servers
 * PoP: w33_servers_a4580e_root_decision     @ agent/lsp/servers.py:_root_or_workspace / _markers_root
 * PoP: w33_servers_a4580e_untrusted_safe    @ agent/lsp/servers.py:UNTRUSTED_SAFE_SERVERS
 * PoP: w33_servers_a4580e_simple_spawn      @ agent/lsp/servers.py:_simple_spawn (positional part)
 * PoP: w33_servers_a4580e_custom_argv       @ agent/lsp/servers.py:_custom_spawn (positional part)
 *
 * SEAMS (stay in the caller): shutil.which / os.path.exists / os.path.isfile
 * binary and bundle probing, nearest_root's directory walk (its two probe
 * answers are the inputs of root_decision), agent.lsp.install auto-install,
 * pm.env_for / installed_package, _detect_python's interpreter scan, the
 * package.json reads of _vue_server_major / _typescript_sdk_dir, the logging
 * behind _warn_once, os.makedirs in hermes_lsp_session_dir, and the psutil
 * style process/pid bits of the PowerShell spawn string.
 *
 * What is ported is everything that is a pure function of values: which
 * server claims a path, which languageId didOpen carries, what a
 * lsp.servers entry normalises to, which root a marker hit selects, which
 * servers may start untrusted, and how a resolved binary plus its static
 * arguments become argv.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_servers_a4580e.h"

typedef struct {
    const char *ext;
    const char *language;
} w33_lang;

typedef struct {
    const char *server_id;
    const char *const *exts;   /* NULL-terminated */
    int seed_first_push;
    int multi_root;
    const char *description;
} w33_srv;

/* ---- registry data (SERVERS, in source order) ------------------------- */

static const char *const W33_EXT_0[] = {".py", ".pyi", NULL};
static const char *const W33_EXT_1[] = {".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs", ".mts", ".cts", NULL};
static const char *const W33_EXT_2[] = {".vue", NULL};
static const char *const W33_EXT_3[] = {".svelte", NULL};
static const char *const W33_EXT_4[] = {".astro", NULL};
static const char *const W33_EXT_5[] = {".go", NULL};
static const char *const W33_EXT_6[] = {".rs", NULL};
static const char *const W33_EXT_7[] = {".c", ".cpp", ".cc", ".cxx", ".h", ".hh", ".hpp", ".hxx", NULL};
static const char *const W33_EXT_8[] = {".sh", ".bash", ".zsh", ".ksh", NULL};
static const char *const W33_EXT_9[] = {".yaml", ".yml", NULL};
static const char *const W33_EXT_10[] = {".lua", NULL};
static const char *const W33_EXT_11[] = {".blade.php", NULL};
static const char *const W33_EXT_12[] = {".php", NULL};
static const char *const W33_EXT_13[] = {".ml", ".mli", NULL};
static const char *const W33_EXT_14[] = {".dockerfile", "Dockerfile", NULL};
static const char *const W33_EXT_15[] = {".tf", ".tfvars", NULL};
static const char *const W33_EXT_16[] = {".dart", NULL};
static const char *const W33_EXT_17[] = {".hs", ".lhs", NULL};
static const char *const W33_EXT_18[] = {".jl", NULL};
static const char *const W33_EXT_19[] = {".clj", ".cljs", ".cljc", ".edn", NULL};
static const char *const W33_EXT_20[] = {".nix", NULL};
static const char *const W33_EXT_21[] = {".zig", ".zon", NULL};
static const char *const W33_EXT_22[] = {".gleam", NULL};
static const char *const W33_EXT_23[] = {".ex", ".exs", NULL};
static const char *const W33_EXT_24[] = {".prisma", NULL};
static const char *const W33_EXT_25[] = {".kt", ".kts", NULL};
static const char *const W33_EXT_26[] = {".java", NULL};
static const char *const W33_EXT_27[] = {".ps1", ".psm1", ".psd1", NULL};

static const w33_srv W33_REGISTRY[] = {
    {"pyright", W33_EXT_0, 0, 1, "Python — Microsoft pyright"},
    {"typescript", W33_EXT_1, 1, 0, "JavaScript/TypeScript — typescript-language-server"},
    {"vue-language-server", W33_EXT_2, 0, 0, "Vue.js — @vue/language-server"},
    {"svelte-language-server", W33_EXT_3, 0, 0, "Svelte — svelte-language-server"},
    {"astro-language-server", W33_EXT_4, 0, 0, "Astro — @astrojs/language-server"},
    {"gopls", W33_EXT_5, 0, 0, "Go — gopls"},
    {"rust-analyzer", W33_EXT_6, 0, 0, "Rust — rust-analyzer"},
    {"clangd", W33_EXT_7, 0, 0, "C/C++ — clangd"},
    {"bash-language-server", W33_EXT_8, 0, 0, "Bash — bash-language-server"},
    {"yaml-language-server", W33_EXT_9, 0, 0, "YAML — yaml-language-server"},
    {"lua-language-server", W33_EXT_10, 0, 0, "Lua — lua-language-server"},
    {"laravel-lsp", W33_EXT_11, 0, 0, "Laravel Blade — laravel-lsp (manual: composer global require laravel/lsp)"},
    {"intelephense", W33_EXT_12, 0, 0, "PHP — intelephense"},
    {"ocaml-lsp", W33_EXT_13, 0, 0, "OCaml — ocaml-lsp"},
    {"dockerfile-ls", W33_EXT_14, 0, 0, "Dockerfile — dockerfile-language-server-nodejs"},
    {"terraform-ls", W33_EXT_15, 0, 0, "Terraform — terraform-ls"},
    {"dart", W33_EXT_16, 0, 0, "Dart — built-in language server"},
    {"haskell-language-server", W33_EXT_17, 0, 0, "Haskell — haskell-language-server"},
    {"julia", W33_EXT_18, 0, 0, "Julia — LanguageServer.jl"},
    {"clojure-lsp", W33_EXT_19, 0, 0, "Clojure — clojure-lsp"},
    {"nixd", W33_EXT_20, 0, 0, "Nix — nixd"},
    {"zls", W33_EXT_21, 0, 0, "Zig — zls"},
    {"gleam", W33_EXT_22, 0, 0, "Gleam — built-in language server"},
    {"elixir-ls", W33_EXT_23, 0, 0, "Elixir — elixir-ls"},
    {"prisma", W33_EXT_24, 0, 0, "Prisma — built-in language server"},
    {"kotlin-language-server", W33_EXT_25, 0, 0, "Kotlin — kotlin-language-server"},
    {"jdtls", W33_EXT_26, 0, 0, "Java — Eclipse JDT Language Server"},
    {"powershell", W33_EXT_27, 0, 0, "PowerShell — PowerShellEditorServices (manual bundle)"},
};
enum { W33_REGISTRY_N = (int)(sizeof W33_REGISTRY / sizeof W33_REGISTRY[0]) };

/* LANGUAGE_BY_EXT, in the source's own insertion order. */
static const w33_lang W33_LANGUAGE_BY_EXT[] = {
    {".py", "python"},
    {".pyi", "python"},
    {".ts", "typescript"},
    {".mts", "typescript"},
    {".cts", "typescript"},
    {".tsx", "typescriptreact"},
    {".js", "javascript"},
    {".mjs", "javascript"},
    {".cjs", "javascript"},
    {".jsx", "javascriptreact"},
    {".vue", "vue"},
    {".svelte", "svelte"},
    {".astro", "astro"},
    {".go", "go"},
    {".rs", "rust"},
    {".rb", "ruby"},
    {".rake", "ruby"},
    {".gemspec", "ruby"},
    {".ru", "ruby"},
    {".c", "c"},
    {".h", "c"},
    {".cc", "cpp"},
    {".cpp", "cpp"},
    {".cxx", "cpp"},
    {".hh", "cpp"},
    {".hpp", "cpp"},
    {".hxx", "cpp"},
    {".cs", "csharp"},
    {".csx", "csharp"},
    {".fs", "fsharp"},
    {".fsi", "fsharp"},
    {".fsx", "fsharp"},
    {".swift", "swift"},
    {".java", "java"},
    {".kt", "kotlin"},
    {".kts", "kotlin"},
    {".yaml", "yaml"},
    {".yml", "yaml"},
    {".json", "json"},
    {".jsonc", "jsonc"},
    {".lua", "lua"},
    {".php", "php"},
    {".blade.php", "blade"},
    {".prisma", "prisma"},
    {".dart", "dart"},
    {".ml", "ocaml"},
    {".mli", "ocaml"},
    {".sh", "shellscript"},
    {".bash", "shellscript"},
    {".zsh", "shellscript"},
    {".tf", "terraform"},
    {".tfvars", "terraform"},
    {".tex", "latex"},
    {".bib", "bibtex"},
    {".gleam", "gleam"},
    {".clj", "clojure"},
    {".cljc", "clojure"},
    {".edn", "clojure"},
    {".cljs", "clojurescript"},
    {".nix", "nix"},
    {".typ", "typst"},
    {".typc", "typst"},
    {".hs", "haskell"},
    {".lhs", "haskell"},
    {".jl", "julia"},
    {".ex", "elixir"},
    {".exs", "elixir"},
    {".zig", "zig"},
    {".zon", "zig"},
    {".dockerfile", "dockerfile"},
    {".ps1", "powershell"},
    {".psm1", "powershell"},
    {".psd1", "powershell"},
};
enum { W33_LANGUAGE_N = (int)(sizeof W33_LANGUAGE_BY_EXT / sizeof W33_LANGUAGE_BY_EXT[0]) };

static const char *const W33_UNTRUSTED_SAFE[] = {
    "bash-language-server",
    "clangd",
    "dockerfile-ls",
    "intelephense",
    "pyright",
    "svelte-language-server",
    "typescript",
    "yaml-language-server",
    NULL
};

/* ---- value helpers ---------------------------------------------------- */

static char *w33_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s) + 1;
    out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static char *w33_lower_dup(const char *s) {
    char *out = w33_dup(s);
    char *p;
    if (!out) return NULL;
    for (p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

/* Python truthiness over the JSON shapes a config value can hold. */
static int w33_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python str() over the scalars a description/language_id can hold.
 * Container values would need repr(); no fixture carries one. */
static char *w33_py_str(const json_t *v) {
    char buf[64];
    if (!v) return w33_dup("");
    switch (v->type) {
    case JSON_STRING: return w33_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return w33_dup(v->bool_val ? "True" : "False");
    case JSON_NULL:   return w33_dup("None");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e15 && v->num_val < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return w33_dup(buf);
    default: return w33_dup("");
    }
}

/* os.path.basename: after the last '/' or '\\' (ntpath on this host). */
static const char *w33_base(const char *path) {
    const char *base = path ? path : "";
    const char *p;
    for (p = base; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}

/* os.path.splitext(base)[1]: last dot after the last separator, unless every
 * character before it is a leading dot (".dockerfile" has no extension). */
static char *w33_splitext_ext(const char *base) {
    const char *dot = strrchr(base, '.');
    size_t i, di;
    if (!dot) return w33_dup("");
    di = (size_t)(dot - base);
    for (i = 0; i < di; i++)
        if (base[i] != '.') return w33_lower_dup(dot);
    return w33_dup("");
}

/* _COMPOUND_EXTS: "_COMPOUND_EXTS" extensions win over splitext's last
 * segment, so home.blade.php stays a Blade template. */
static char *w33_ext_or_basename(const char *path) {
    const char *base = w33_base(path);
    size_t lb = strlen(base), lc = strlen(".blade.php");
    char *lower, *ext;
    lower = w33_lower_dup(base);
    if (lower && lb >= lc && !strcmp(lower + lb - lc, ".blade.php")) {
        free(lower);
        return w33_dup(".blade.php");
    }
    free(lower);
    ext = w33_splitext_ext(base);
    if (ext && ext[0]) return ext;
    free(ext);
    return w33_dup(base);   /* extensionless: the basename, original case */
}

static const char *w33_lang_for(const char *ext) {
    int i;
    for (i = 0; i < W33_LANGUAGE_N; i++)
        if (!strcmp(W33_LANGUAGE_BY_EXT[i].ext, ext)) return W33_LANGUAGE_BY_EXT[i].language;
    return NULL;
}

static int w33_ext_in(const char *ext, const json_t *extensions) {
    size_t i, n;
    if (!ext || !extensions || extensions->type != JSON_ARRAY) return 0;
    n = json_len(extensions);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(extensions, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(ext, e->str_val)) return 1;
    }
    return 0;
}

static int w33_builtin_id(const char *server_id) {
    int i;
    if (!server_id) return 0;
    for (i = 0; i < W33_REGISTRY_N; i++)
        if (!strcmp(W33_REGISTRY[i].server_id, server_id)) return 1;
    return 0;
}

static json_t *w33_srv_desc(const w33_srv *s) {
    json_t *row = json_object();
    json_t *exts = json_array();
    const char *const *e;
    for (e = s->exts; *e; e++) json_append(exts, json_string(*e));
    json_set(row, "server_id", json_string(s->server_id));
    json_set(row, "extensions", exts);
    json_set(row, "seed_first_push", json_bool(s->seed_first_push ? true : false));
    json_set(row, "multi_root", json_bool(s->multi_root ? true : false));
    json_set(row, "description", json_string(s->description));
    json_set(row, "language_id", json_string(""));
    return row;
}

/* command: [bin, ...args] / extensions: [.ext, ...] are both non-empty lists
 * of non-empty strings. */
static int w33_str_list_ok(const json_t *v) {
    size_t i, n;
    if (!v || v->type != JSON_ARRAY) return 0;
    n = json_len(v);
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(v, i);
        if (!e || e->type != JSON_STRING || !e->str_val || !e->str_val[0]) return 0;
    }
    return 1;
}

/* custom_servers(): descriptors in config order, custom entries ahead of the
 * built-ins in find_server. */
static json_t *w33_custom_descs(const json_t *cfg) {
    json_t *out = json_array();
    size_t n, i;
    if (!cfg || cfg->type != JSON_OBJECT) return out;
    n = json_object_size(cfg);
    for (i = 0; i < n; i++) {
        const char *server_id = json_object_get_key_at(cfg, i);
        const json_t *c = json_object_get_at(cfg, i);
        const json_t *cmd, *exts, *desc, *lid;
        json_t *row, *extarr;
        char *text;
        size_t j;
        if (!server_id || w33_builtin_id(server_id)) continue;
        if (!c || c->type != JSON_OBJECT) continue;
        if (!json_obj_get(c, "extensions")) continue;   /* "extensions" in cfg */
        cmd = json_obj_get(c, "command");
        exts = json_obj_get(c, "extensions");
        if (!w33_str_list_ok(cmd) || !w33_str_list_ok(exts)) continue;

        row = json_object();
        json_set(row, "server_id", json_string(server_id));
        extarr = json_array();
        for (j = 0; j < json_len(exts); j++) {
            const json_t *node = json_get(exts, j);
            const char *raw = node && node->str_val ? node->str_val : "";
            if (raw[0] == '.') {
                char *low = w33_lower_dup(raw);
                json_append(extarr, json_string(low ? low : ""));
                free(low);
            } else {
                json_append(extarr, json_string(raw));   /* dotted-only lower() */
            }
        }
        json_set(row, "extensions", extarr);

        desc = json_obj_get(c, "description");
        if (w33_truthy(desc)) {
            text = w33_py_str(desc);
        } else {
            size_t need = strlen(server_id) + strlen(" — custom (lsp.servers)") + 1;
            text = (char *)malloc(need);
            if (text) snprintf(text, need, "%s — custom (lsp.servers)", server_id);
        }
        json_set(row, "description", json_string(text ? text : ""));
        free(text);

        lid = json_obj_get(c, "language_id");
        if (w33_truthy(lid)) {
            text = w33_py_str(lid);
            json_set(row, "language_id", json_string(text ? text : ""));
            free(text);
        } else {
            json_set(row, "language_id", json_string(""));
        }
        json_set(row, "seed_first_push", json_bool(false));
        json_set(row, "multi_root", json_bool(false));
        json_append(out, row);
    }
    return out;
}

static int w33_desc_matches(const char *path, const json_t *desc) {
    char *ext = w33_ext_or_basename(path);
    int hit = w33_ext_in(ext, json_obj_get(desc, "extensions"));
    free(ext);
    return hit;
}

/* ---- the ported symbols ---------------------------------------------- */

json_t *w33_servers_a4580e_ext_or_basename(const char *path) {
    char *ext = w33_ext_or_basename(path);
    json_t *out = json_string(ext ? ext : "");
    free(ext);
    return out;
}

json_t *w33_servers_a4580e_language_table(void) {
    json_t *o = json_object();
    int i;
    for (i = 0; i < W33_LANGUAGE_N; i++)
        json_set(o, W33_LANGUAGE_BY_EXT[i].ext, json_string(W33_LANGUAGE_BY_EXT[i].language));
    return o;
}

json_t *w33_servers_a4580e_language_id(const char *path, const char *srv_language_id) {
    char *ext;
    const char *lang;
    json_t *out;
    if (srv_language_id && srv_language_id[0]) return json_string(srv_language_id);
    ext = w33_ext_or_basename(path);
    lang = ext ? w33_lang_for(ext) : NULL;
    out = json_string(lang ? lang : "plaintext");
    free(ext);
    return out;
}

json_t *w33_servers_a4580e_matches(const char *path, const json_t *extensions) {
    char *ext = w33_ext_or_basename(path);
    int hit = w33_ext_in(ext, extensions);
    free(ext);
    return json_bool(hit ? true : false);
}

json_t *w33_servers_a4580e_registry(void) {
    json_t *out = json_array();
    int i;
    for (i = 0; i < W33_REGISTRY_N; i++) json_append(out, w33_srv_desc(&W33_REGISTRY[i]));
    return out;
}

json_t *w33_servers_a4580e_custom_servers(const json_t *servers_cfg) {
    return w33_custom_descs(servers_cfg);
}

json_t *w33_servers_a4580e_find_server(const char *path, const json_t *custom_cfg) {
    json_t *customs = w33_custom_descs(custom_cfg);
    size_t i, n = json_len(customs);
    json_t *hit = NULL;
    for (i = 0; i < n; i++) {
        json_t *d = json_get(customs, i);
        if (d && w33_desc_matches(path, d)) { hit = json_copy(d); break; }
    }
    json_free(customs);
    if (hit) return hit;
    for (i = 0; i < (size_t)W33_REGISTRY_N; i++) {
        const char *const *e;
        char *ext = w33_ext_or_basename(path);
        int matched = 0;
        for (e = W33_REGISTRY[i].exts; *e; e++)
            if (ext && !strcmp(ext, *e)) { matched = 1; break; }
        free(ext);
        if (matched) return w33_srv_desc(&W33_REGISTRY[i]);
    }
    return json_null();
}

json_t *w33_servers_a4580e_root_decision(const json_t *req) {
    const json_t *markers = json_obj_get(req, "markers");
    const json_t *found = json_obj_get(req, "found");
    const json_t *plain = json_obj_get(req, "found_plain");
    const json_t *excludes = json_obj_get(req, "excludes");
    const char *ws = json_get_str(req, "workspace", "");
    const char *f = (found && found->type == JSON_STRING) ? found->str_val : NULL;
    const char *p = (plain && plain->type == JSON_STRING) ? plain->str_val : NULL;
    if (!ws) ws = "";
    /* markers=None means "always the workspace root": the walk never runs. */
    if (!markers || markers->type == JSON_NULL) return json_string(ws);
    if (!f && w33_truthy(excludes) && p) return json_null();  /* exclude fired */
    if (f && f[0]) return json_string(f);
    return json_string(ws);
}

json_t *w33_servers_a4580e_untrusted_safe(const char *server_id) {
    const char *const *p;
    if (!server_id) return json_bool(false);
    for (p = W33_UNTRUSTED_SAFE; *p; p++)
        if (!strcmp(*p, server_id)) return json_bool(true);
    return json_bool(false);
}

json_t *w33_servers_a4580e_simple_spawn(const json_t *req) {
    const json_t *bin = json_obj_get(req, "bin");
    const json_t *args = json_obj_get(req, "args");
    const json_t *base_init = json_obj_get(req, "base_init");
    const json_t *untrusted_init = json_obj_get(req, "untrusted_init");
    const json_t *init;
    json_t *out, *cmd;
    size_t i, n;
    if (!bin || bin->type != JSON_STRING) return json_null();   /* binary unresolved: skip */
    /* init = base_init if ctx.trusted or untrusted_init is None else untrusted_init */
    if (w33_truthy(json_obj_get(req, "trusted")) || !untrusted_init || untrusted_init->type == JSON_NULL)
        init = base_init;
    else
        init = untrusted_init;
    cmd = json_array();
    json_append(cmd, json_string(bin->str_val ? bin->str_val : ""));
    n = (args && args->type == JSON_ARRAY) ? json_len(args) : 0;
    for (i = 0; i < n; i++) json_append(cmd, json_copy(json_get(args, i)));
    out = json_object();
    json_set(out, "command", cmd);
    json_set(out, "init", init ? json_copy(init) : json_null());
    json_set(out, "seed", json_bool(w33_truthy(json_obj_get(req, "seed")) ? true : false));
    return out;
}

json_t *w33_servers_a4580e_custom_argv(const json_t *req) {
    const json_t *bin = json_obj_get(req, "bin");
    const json_t *command = json_obj_get(req, "command");
    json_t *out;
    size_t i, n;
    if (!bin || bin->type != JSON_STRING) return json_null();   /* not on PATH: skip */
    out = json_array();
    json_append(out, json_string(bin->str_val ? bin->str_val : ""));
    n = (command && command->type == JSON_ARRAY) ? json_len(command) : 0;
    for (i = 1; i < n; i++) json_append(out, json_copy(json_get(command, i)));
    return out;
}
