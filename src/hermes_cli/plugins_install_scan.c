/*
 * plugins_install_scan.c — C11 port of upstream
 * hermes_cli/plugins_install_scan.py (gate w34_pluginsins_608012).
 *
 * PoP: w34_pluginsins_608012_path_class             @ hermes_cli/plugins_install_scan.py:_filter_findings (is_code/is_python/is_js/doc_prose/is_ci/locale_catalog derivation)
 * PoP: w34_pluginsins_608012_should_allow           @ hermes_cli/plugins_install_scan.py:should_allow_plugin_install
 * PoP: w34_pluginsins_608012_dangerous_summary      @ hermes_cli/plugins_install_scan.py:_dangerous_findings_summary
 * PoP: w34_pluginsins_608012_scan_summary           @ hermes_cli/plugins_install_scan.py:scan_plugin (summary shaping)
 * PoP: w34_pluginsins_608012_provenance             @ hermes_cli/plugins_install_scan.py:scan_plugin (scan_provenance)
 * PoP: w34_pluginsins_608012_severity_at_most       @ hermes_cli/plugins_install_scan.py:_at_most
 * PoP: w34_pluginsins_608012_comment_severity       @ hermes_cli/plugins_install_scan.py:_comment_severity
 * PoP: w34_pluginsins_608012_docstring_cap          @ hermes_cli/plugins_install_scan.py:_docstring_cap
 * PoP: w34_pluginsins_608012_prose_cap              @ hermes_cli/plugins_install_scan_context.py:prose_cap
 * PoP: w34_pluginsins_608012_catalog_cap            @ hermes_cli/plugins_install_scan_context.py:catalog_cap
 * PoP: w34_pluginsins_608012_is_agent_facing        @ hermes_cli/plugins_install_scan_context.py:is_agent_facing
 * PoP: w34_pluginsins_608012_is_defensive_documentation @ hermes_cli/plugins_install_scan.py:_is_defensive_documentation
 * PoP: w34_pluginsins_608012_filter_findings        @ hermes_cli/plugins_install_scan.py:_filter_findings
 * PoP: w34_pluginsins_608012_logical_line           @ hermes_cli/plugins_install_scan_context.py:logical_line
 * PoP: w34_pluginsins_608012_limits                 @ hermes_cli/plugins_install_scan.py:<module constants>
 * PoP: w34_pluginsins_608012_size_finding           @ hermes_cli/plugins_install_scan.py:_check_plugin_structure (oversized_file)
 * PoP: w34_pluginsins_608012_binary_finding         @ hermes_cli/plugins_install_scan.py:_check_plugin_structure (binary_file)
 * PoP: w34_pluginsins_608012_bytecode_finding       @ hermes_cli/plugins_install_scan.py:_bytecode_findings
 * PoP: w34_pluginsins_608012_manifest_findings      @ hermes_cli/plugins_install_scan.py:_check_plugin_structure (directory limits)
 *
 * What is ported is only what values decide.  The walk (``_walk``), the source
 * reader (``_file_lines``), ``scan_file``, git (``_tracked_bytecode``) and the
 * Python AST (``_is_main_guard``/``_docstring_lines``/``_main_guard_body_lines``)
 * are seams: the AST line sets arrive here as values.  The inert-context regex
 * family (``_inert_context_cap`` and friends, built from the
 * ``tools.skills_guard`` pattern table) is likewise left to the caller and such
 * findings pass through unchanged rather than being guessed at.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "noc_w34_pluginsins_608012.h"

#define PG_MAX_PARTS 128
#define PG_LIMN 8

static const char *const PG_EXCLUDED_DIRS[] = {
    ".git", ".mypy_cache", ".pytest_cache", ".ruff_cache", ".tox", ".venv",
    "__pycache__", "node_modules", "venv",
};
static const char *const PG_CODE_EXTS[] = {
    ".bash", ".js", ".php", ".pl", ".py", ".rb", ".sh", ".ts",
};
static const char *const PG_CHANGELOGS[] = {"changelog.md"};
static const char *const PG_BYTECODE_SUFFIXES[] = {".pyc", ".pyo"};
static const char *const PG_BINARY_EXTS[] = {
    ".app", ".bin", ".com", ".dat", ".deb", ".dll", ".dmg", ".dylib",
    ".exe", ".msi", ".rpm", ".so",
};
static const char *const PG_DOC_EXTS[] = {".html", ".md", ".rst", ".txt"};
static const char *const PG_AGENT_INSTRUCTION_FILES[] = {"after-install.md", "skill.md"};
static const char *const PG_AGENT_INSTRUCTION_DIRS[] = {"optional-skills", "skills"};
static const char *const PG_CI_SUFFIXES[] = {".yaml", ".yml"};
static const char *const PG_TEST_TREE_DIRS[] = {"fixtures", "spec", "specs", "test", "testing", "tests"};
static const char *const PG_TEST_DIRS_ANY_DEPTH[] = {"__fixtures__", "__mocks__", "__tests__"};

/* CODE_EXEMPT_PATTERN_IDS — every provider plugin trips these; docs keep them. */
static const char *const PG_CODE_EXEMPT_IDS[] = {
    "agent_config_contract", "agent_config_mod", "context_exfil", "encoded_exfil",
    "env_exfil_curl", "env_exfil_fetch", "env_exfil_httpx", "env_exfil_requests",
    "env_exfil_wget", "fake_policy", "node_process_env", "python_environ_get_secret",
    "python_getenv_secret", "python_os_environ", "ruby_env_secret", "send_to_url",
};

/* _PROSE_KEEPS_FULL_SEVERITY_IDS — prose IS the payload for these. */
static const char *const PG_PROSE_KEEP_IDS[] = {
    "agent_config_contract", "agent_config_mod", "agent_config_mod_shell", "agent_config_ref",
    "context_exfil", "curl_pipe_python", "curl_pipe_shell", "hermes_config_mod",
    "hermes_config_mod_shell", "hermes_config_ref", "md_image_exfil", "md_link_exfil",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref",
    "send_to_url", "ssh_backdoor", "wget_pipe_shell",
};
static const char *const PG_PROSE_KEEP_CATEGORIES[] = {"credential_exposure", "injection"};

/* _CATALOG_STEPS_DOWN_IDS — a translated UI status string, never a write. */
static const char *const PG_CATALOG_IDS[] = {
    "agent_config_contract", "agent_config_mod", "agent_config_mod_shell", "agent_config_ref",
    "hermes_config_mod", "hermes_config_mod_shell", "hermes_config_ref",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref",
};

typedef struct { const char *id; const char *sev; } pg_pair;

static const pg_pair PG_SEVERITY_REMAP[] = {
    {"binary_file", "high"}, {"curl_pipe_shell", "high"}, {"hermes_env_access", "medium"},
};
static const pg_pair PG_JS_REMAP[] = {
    {"dns_exfil", "high"}, {"ssh_backdoor", "high"},
};
static const pg_pair PG_DOC_PROSE_DEMOTIONS[] = {
    {"agent_config_mod", "high"}, {"hardcoded_secret", "high"},
};
static const pg_pair PG_MAIN_GUARD_DEMOTIONS[] = {{"hardcoded_secret", "high"}};

/* COMMENT_PREFIXES_BY_EXTENSION */
static const pg_pair PG_COMMENT_PREFIXES[] = {
    {".bash", "#"}, {".jl", "#"}, {".js", "//"}, {".php", "//"}, {".pl", "#"},
    {".py", "#"}, {".r", "#"}, {".rb", "#"}, {".sh", "#"}, {".ts", "//"},
};

#define PG_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static int pg_in_set(const char *needle, const char *const *set, size_t n) {
    size_t i;
    if (!needle) return 0;
    for (i = 0; i < n; i++)
        if (set[i] && strcmp(needle, set[i]) == 0) return 1;
    return 0;
}

static int pg_ci_in_set(const char *needle, const char *const *set, size_t n) {
    size_t i;
    if (!needle) return 0;
    for (i = 0; i < n; i++)
        if (set[i] && strcasecmp(needle, set[i]) == 0) return 1;
    return 0;
}

static const char *pg_pair_lookup(const pg_pair *tbl, size_t n, const char *id) {
    size_t i;
    if (!id) return NULL;
    for (i = 0; i < n; i++)
        if (strcmp(tbl[i].id, id) == 0) return tbl[i].sev;
    return NULL;
}

/* _SEVERITY_RANK.get(sev, 0) */
static int pg_rank_or0(const char *sev) {
    if (!sev) return 0;
    if (!strcmp(sev, "critical")) return 3;
    if (!strcmp(sev, "high")) return 2;
    if (!strcmp(sev, "medium")) return 1;
    return 0;
}

static int pg_rank_known(const char *sev) {
    return sev && (!strcmp(sev, "critical") || !strcmp(sev, "high") ||
                   !strcmp(sev, "medium") || !strcmp(sev, "low"));
}

/* _COMMENT_SEVERITY_CAP / STEP_DOWN */
static const char *pg_step_down(const char *sev) {
    if (!sev) return NULL;
    if (!strcmp(sev, "critical")) return "high";
    if (!strcmp(sev, "high")) return "medium";
    return sev;
}

/* STEP_DOWN.get(sev): only critical/high step, everything else has no entry. */
static const char *pg_step_down_exact(const char *sev) {
    if (!sev) return NULL;
    if (!strcmp(sev, "critical")) return "high";
    if (!strcmp(sev, "high")) return "medium";
    return NULL;
}

static int pg_is_agent_facing(const char *pid, const char *cat) {
    if (pg_in_set(cat, PG_PROSE_KEEP_CATEGORIES, PG_ARRAY_LEN(PG_PROSE_KEEP_CATEGORIES))) return 1;
    return pg_in_set(pid, PG_PROSE_KEEP_IDS, PG_ARRAY_LEN(PG_PROSE_KEEP_IDS));
}

static const char *pg_prose_cap_str(const char *pid, const char *cat, const char *sev) {
    if (pg_is_agent_facing(pid, cat)) return NULL;
    return pg_step_down_exact(sev);
}

static const char *pg_catalog_cap_str(const char *pid, const char *cat, const char *sev) {
    if (pg_in_set(pid, PG_CATALOG_IDS, PG_ARRAY_LEN(PG_CATALOG_IDS)))
        return pg_rank_or0(sev) <= 1 ? sev : "medium";
    return pg_prose_cap_str(pid, cat, sev);
}

static const char *pg_at_most_str(const char *sev, const char *cap) {
    if (!sev) return sev;
    if (!pg_rank_known(cap)) return sev;   /* Python would raise; never invent a cap */
    return pg_rank_or0(sev) > pg_rank_or0(cap) ? cap : sev;
}

static const char *pg_comment_severity_str(const char *pid, const char *cat, const char *sev) {
    const char *stepped = pg_step_down(sev);
    if (!stepped) return sev;
    return pg_is_agent_facing(pid, cat) ? stepped : pg_step_down(stepped);
}

static const char *pg_docstring_cap_str(const char *pid, const char *cat, const char *sev) {
    if (sev && !strcmp(sev, "critical")) return "high";
    return pg_comment_severity_str(pid, cat, sev);
}

/* ── path derivation ──────────────────────────────────────────────────────── */

typedef struct {
    char *store;                      /* owned, split in place */
    char *part[PG_MAX_PARTS];
    int n;
    char *name;
    char *suffix;                     /* "" when the name has none */
} pg_path;

static void pg_path_parse(pg_path *p, const char *rel) {
    size_t len = rel ? strlen(rel) : 0;
    char *cur;
    memset(p, 0, sizeof(*p));
    p->store = (char *)malloc(len + 1);
    if (!p->store) {
        p->name = "";
        p->suffix = "";
        return;
    }
    memcpy(p->store, rel ? rel : "", len + 1);
    cur = p->store;
    while (*cur) {
        char *slash = strchr(cur, '/');
        if (slash) *slash = '\0';
        /* Path() drops empty and "." components; ".." is kept. */
        if (cur[0] != '\0' && strcmp(cur, ".") != 0 && p->n < PG_MAX_PARTS)
            p->part[p->n++] = cur;
        if (!slash) break;
        cur = slash + 1;
    }
    p->name = p->n > 0 ? p->part[p->n - 1] : "";
    {
        char *dot = strrchr(p->name, '.');
        size_t nl = strlen(p->name);
        if (dot && dot > p->name && (size_t)(dot - p->name) < nl - 1)
            p->suffix = dot;
        else
            p->suffix = "";
    }
}

static void pg_path_free(pg_path *p) {
    free(p->store);
    p->store = NULL;
}

static int pg_is_code_ext(const char *suffix) {
    return pg_ci_in_set(suffix, PG_CODE_EXTS, PG_ARRAY_LEN(PG_CODE_EXTS));
}

static int pg_is_doc_prose(const pg_path *p) {
    int i;
    if (!pg_ci_in_set(p->suffix, PG_DOC_EXTS, PG_ARRAY_LEN(PG_DOC_EXTS))) return 0;
    if (pg_ci_in_set(p->name, PG_AGENT_INSTRUCTION_FILES, PG_ARRAY_LEN(PG_AGENT_INSTRUCTION_FILES)))
        return 0;
    for (i = 0; i < p->n - 1; i++)
        if (pg_ci_in_set(p->part[i], PG_AGENT_INSTRUCTION_DIRS, PG_ARRAY_LEN(PG_AGENT_INSTRUCTION_DIRS)))
            return 0;
    return 1;
}

static int pg_is_ci_workflow(const pg_path *p) {
    if (strcasecmp(p->name, ".gitlab-ci.yml") == 0) return 1;
    return p->n == 3
        && strcasecmp(p->part[0], ".github") == 0
        && strcasecmp(p->part[1], "workflows") == 0
        && pg_ci_in_set(p->suffix, PG_CI_SUFFIXES, PG_ARRAY_LEN(PG_CI_SUFFIXES));
}

static int pg_is_locale_catalog(const pg_path *p) {
    return p->n == 2 && strcasecmp(p->part[0], "locales") == 0
        && pg_ci_in_set(p->suffix, PG_CI_SUFFIXES, PG_ARRAY_LEN(PG_CI_SUFFIXES));
}

/* ^(?:tests?_[^/]*|[^/]*_tests?\.[^./]+|[^/]*\.(?:test|spec)\.[^./]+)$, IGNORECASE */
static int pg_is_test_file_name(const char *name) {
    size_t n = strlen(name);
    char *dot;
    size_t q;
    if (strncasecmp(name, "test_", 5) == 0) return 1;
    if (n >= 6 && strncasecmp(name, "tests_", 6) == 0) return 1;
    dot = strrchr(name, '.');
    if (!dot) return 0;
    q = (size_t)(dot - name);
    if (q + 1 >= n) return 0;                    /* needs a non-empty tail */
    if (q >= 5 && strncasecmp(name + q - 5, "_test", 5) == 0) return 1;
    if (q >= 6 && strncasecmp(name + q - 6, "_tests", 6) == 0) return 1;
    if (q >= 5 && name[q - 5] == '.' && strncasecmp(name + q - 4, "test", 4) == 0) return 1;
    if (q >= 6 && name[q - 6] == '.' && strncasecmp(name + q - 5, "spec", 4) == 0) return 1;
    return 0;
}

static int pg_is_test_tree(const pg_path *p) {
    int i;
    if (p->n > 1 && pg_ci_in_set(p->part[0], PG_TEST_TREE_DIRS, PG_ARRAY_LEN(PG_TEST_TREE_DIRS)))
        return 1;
    for (i = 0; i < p->n - 1; i++)
        if (pg_ci_in_set(p->part[i], PG_TEST_DIRS_ANY_DEPTH, PG_ARRAY_LEN(PG_TEST_DIRS_ANY_DEPTH)))
            return 1;
    return pg_is_test_file_name(p->name);
}

static int pg_is_excluded(const pg_path *p) {
    int i;
    for (i = 0; i < p->n; i++)
        if (pg_in_set(p->part[i], PG_EXCLUDED_DIRS, PG_ARRAY_LEN(PG_EXCLUDED_DIRS))) return 1;
    return 0;
}

static int pg_is_bytecode(const char *rel) {
    size_t n = rel ? strlen(rel) : 0;
    size_t i;
    for (i = 0; i < PG_ARRAY_LEN(PG_BYTECODE_SUFFIXES); i++) {
        size_t sl = strlen(PG_BYTECODE_SUFFIXES[i]);
        if (n >= sl && strcmp(rel + n - sl, PG_BYTECODE_SUFFIXES[i]) == 0) return 1;
    }
    return 0;
}

static const char *pg_comment_prefix(const pg_path *p) {
    return pg_pair_lookup(PG_COMMENT_PREFIXES, PG_ARRAY_LEN(PG_COMMENT_PREFIXES), p->suffix);
}

json_t *w34_pluginsins_608012_path_class(const char *rel) {
    pg_path p;
    json_t *o = json_object(), *parts = json_array();
    int i, is_code;

    pg_path_parse(&p, rel);
    is_code = pg_is_code_ext(p.suffix);
    for (i = 0; i < p.n; i++) json_append(parts, json_string(p.part[i]));
    json_set(o, "rel", json_string(rel ? rel : ""));
    json_set(o, "name", json_string(p.name));
    json_set(o, "suffix", json_string(p.suffix));
    json_set(o, "parts", parts);
    json_set(o, "is_code", json_bool(is_code));
    json_set(o, "is_python", json_bool(strcasecmp(p.suffix, ".py") == 0));
    json_set(o, "is_js", json_bool(strcasecmp(p.suffix, ".js") == 0 || strcasecmp(p.suffix, ".ts") == 0));
    json_set(o, "is_doc_prose", json_bool(pg_is_doc_prose(&p)));
    json_set(o, "is_ci_workflow", json_bool(pg_is_ci_workflow(&p)));
    json_set(o, "doc_prose", json_bool(pg_is_doc_prose(&p) || pg_is_ci_workflow(&p)));
    json_set(o, "is_locale_catalog", json_bool(pg_is_locale_catalog(&p)));
    json_set(o, "is_test_tree", json_bool(pg_is_test_tree(&p)));
    json_set(o, "is_changelog",
             json_bool(pg_ci_in_set(p.name, PG_CHANGELOGS, PG_ARRAY_LEN(PG_CHANGELOGS))));
    {
        const char *prefix = pg_comment_prefix(&p);
        json_set(o, "comment_prefix", prefix ? json_string(prefix) : json_null());
    }
    json_set(o, "is_excluded", json_bool(pg_is_excluded(&p)));
    json_set(o, "is_bytecode", json_bool(pg_is_bytecode(rel)));
    pg_path_free(&p);
    return o;
}

/* ── severity entry points ────────────────────────────────────────────────── */

json_t *w34_pluginsins_608012_severity_at_most(const char *severity, const char *cap) {
    return json_string(pg_at_most_str(severity ? severity : "", cap ? cap : ""));
}

json_t *w34_pluginsins_608012_comment_severity(const char *pattern_id, const char *category,
                                              const char *severity) {
    return json_string(pg_comment_severity_str(pattern_id, category, severity ? severity : ""));
}

json_t *w34_pluginsins_608012_docstring_cap(const char *pattern_id, const char *category,
                                           const char *severity) {
    return json_string(pg_docstring_cap_str(pattern_id, category, severity ? severity : ""));
}

json_t *w34_pluginsins_608012_prose_cap(const char *pattern_id, const char *category,
                                       const char *severity) {
    const char *cap = pg_prose_cap_str(pattern_id, category, severity ? severity : "");
    return cap ? json_string(cap) : json_null();
}

json_t *w34_pluginsins_608012_catalog_cap(const char *pattern_id, const char *category,
                                         const char *severity) {
    const char *cap = pg_catalog_cap_str(pattern_id, category, severity ? severity : "");
    return cap ? json_string(cap) : json_null();
}

int w34_pluginsins_608012_is_agent_facing(const char *pattern_id, const char *category) {
    return pg_is_agent_facing(pattern_id, category);
}

/* ── line handling ────────────────────────────────────────────────────────── */

static void pg_rstrip(char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' ||
                     s[n - 1] == '\n' || s[n - 1] == '\v' || s[n - 1] == '\f'))
        s[--n] = '\0';
}

/* Trailing backslashes only (Python's rstrip("\\")). */
static void pg_rstrip_bs(char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && s[n - 1] == '\\') s[--n] = '\0';
}

static const char *pg_line_at(const json_t *lines, int idx) {
    json_t *v;
    if (!lines || lines->type != JSON_ARRAY || idx < 0 || (size_t)idx >= json_len(lines)) return NULL;
    v = json_get(lines, (size_t)idx);
    return v && v->type == JSON_STRING ? (v->str_val ? v->str_val : "") : "";
}

json_t *w34_pluginsins_608012_logical_line(const json_t *lines, int index) {
    char *parts[PG_LIMN + 1];
    int n = 0, i = index;
    size_t total = 0, off = 0;
    char *buf;

    parts[n++] = strdup(pg_line_at(lines, index) ? pg_line_at(lines, index) : "");
    while (i + 1 < (int)json_len(lines) && n <= PG_LIMN) {
        const char *cur = pg_line_at(lines, i);
        char *tmp = strdup(cur ? cur : "");
        int cont;
        pg_rstrip(tmp);
        cont = tmp[0] != '\0' && tmp[strlen(tmp) - 1] == '\\';
        free(tmp);
        if (!cont) break;
        i++;
        parts[n++] = strdup(pg_line_at(lines, i) ? pg_line_at(lines, i) : "");
    }
    for (i = 0; i < n; i++) {
        pg_rstrip(parts[i]);
        pg_rstrip_bs(parts[i]);
        total += strlen(parts[i]) + 1;
    }
    buf = (char *)malloc(total + 1);
    if (!buf) {
        for (i = 0; i < n; i++) free(parts[i]);
        return json_string("");
    }
    buf[0] = '\0';
    for (i = 0; i < n; i++) {
        if (i) buf[off++] = ' ';
        memcpy(buf + off, parts[i], strlen(parts[i]));
        off += strlen(parts[i]);
        free(parts[i]);
    }
    buf[off] = '\0';
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

/* ── defensive documentation ──────────────────────────────────────────────── */

static int pg_in_int_array(const json_t *arr, int value) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_NUMBER && (int)v->num_val == value) return 1;
    }
    return 0;
}

static int pg_is_defensive(const char *match, const pg_path *p, const json_t *docstring_lines,
                           int line_no) {
    const char *prefix;
    const char *stripped;

    if (pg_ci_in_set(p->name, PG_CHANGELOGS, PG_ARRAY_LEN(PG_CHANGELOGS))) return 1;
    if (docstring_lines && json_len(docstring_lines) > 0 && pg_in_int_array(docstring_lines, line_no))
        return 1;
    prefix = pg_comment_prefix(p);
    if (!prefix || !match || !match[0]) return 0;
    stripped = match;
    while (*stripped == ' ' || *stripped == '\t') stripped++;
    if (strncmp(stripped, prefix, strlen(prefix)) != 0) return 0;
    if (strcmp(prefix, "#") == 0 && (strncmp(stripped, "#!", 2) == 0 || strncmp(stripped, "#:", 2) == 0))
        return 0;
    return 1;
}

int w34_pluginsins_608012_is_defensive_documentation(const char *match, const char *rel, int line,
                                                     const json_t *docstring_lines) {
    pg_path p;
    int out;
    pg_path_parse(&p, rel);
    out = pg_is_defensive(match, &p, docstring_lines, line);
    pg_path_free(&p);
    return out;
}

/* ── the per-finding policy pipeline ──────────────────────────────────────── */

static const char *pg_context_severity(const char *pid, const char *cat, const pg_path *p,
                                       int is_code, int doc_prose, int locale_catalog,
                                       const char *sev) {
    const char *cur = sev;

    if (doc_prose) {
        const char *cap = locale_catalog ? pg_catalog_cap_str(pid, cat, cur)
                                         : pg_prose_cap_str(pid, cat, cur);
        if (cap) cur = cap;
    }
    if (pg_is_test_tree(p)) {
        /* is_inert_fixture_line: a non-code fixture is always data; a code line
         * needs the literal-span scan against the pattern table (seam). */
        int inert = (cat && strcmp(cat, "credential_exposure") == 0) || !is_code;
        cur = inert ? pg_at_most_str(cur, "medium") : pg_step_down(cur);
    }
    return cur;
}

json_t *w34_pluginsins_608012_filter_findings(const json_t *findings, const char *rel,
                                              const json_t *lines, const json_t *docstring_lines,
                                              const json_t *main_guard_lines) {
    pg_path p;
    json_t *out = json_array();
    size_t i, n;
    int is_code, is_js, doc_prose, locale_catalog;

    pg_path_parse(&p, rel);
    if (!findings || findings->type != JSON_ARRAY) {
        pg_path_free(&p);
        return out;
    }
    /* The file's source lines only feed the inert-context regex family, which
     * is a seam here; the AST line sets arrive pre-computed. */
    (void)lines;
    is_code = pg_is_code_ext(p.suffix);
    is_js = strcasecmp(p.suffix, ".js") == 0 || strcasecmp(p.suffix, ".ts") == 0;
    doc_prose = pg_is_doc_prose(&p) || pg_is_ci_workflow(&p);
    locale_catalog = pg_is_locale_catalog(&p);

    n = json_len(findings);
    for (i = 0; i < n; i++) {
        json_t *f = json_get(findings, i);
        json_t *copy;
        const char *pid, *cat, *sev, *match, *cur;
        int line_no;

        if (!f || f->type != JSON_OBJECT) continue;
        pid = json_get_str(f, "pattern_id", "");
        cat = json_get_str(f, "category", "");
        sev = json_get_str(f, "severity", "");
        match = json_get_str(f, "match", "");
        line_no = (int)json_get_num(f, "line", 0);

        if (is_code && pg_in_set(pid, PG_CODE_EXEMPT_IDS, PG_ARRAY_LEN(PG_CODE_EXEMPT_IDS)))
            continue;

        cur = sev;
        if (is_js) {
            const char *r = pg_pair_lookup(PG_JS_REMAP, PG_ARRAY_LEN(PG_JS_REMAP), pid);
            if (!r) r = pg_pair_lookup(PG_SEVERITY_REMAP, PG_ARRAY_LEN(PG_SEVERITY_REMAP), pid);
            if (r) cur = r;
        } else {
            const char *r = pg_pair_lookup(PG_SEVERITY_REMAP, PG_ARRAY_LEN(PG_SEVERITY_REMAP), pid);
            if (r) cur = r;
        }
        if (doc_prose) {
            const char *d = pg_pair_lookup(PG_DOC_PROSE_DEMOTIONS,
                                           PG_ARRAY_LEN(PG_DOC_PROSE_DEMOTIONS), pid);
            if (d) cur = d;
        }

        cur = pg_context_severity(pid, cat, &p, is_code, doc_prose || locale_catalog,
                                  locale_catalog, cur);
        if (pg_is_defensive(match, &p, docstring_lines, line_no)) {
            int in_doc = docstring_lines && json_len(docstring_lines) > 0 &&
                         pg_in_int_array(docstring_lines, line_no);
            cur = in_doc ? pg_docstring_cap_str(pid, cat, cur)
                         : pg_comment_severity_str(pid, cat, cur);
        }
        if (pg_pair_lookup(PG_MAIN_GUARD_DEMOTIONS, PG_ARRAY_LEN(PG_MAIN_GUARD_DEMOTIONS), pid) &&
            cur && strcmp(cur, "critical") == 0 &&
            main_guard_lines && pg_in_int_array(main_guard_lines, line_no))
            cur = "high";

        copy = json_copy(f);
        json_set(copy, "severity", json_string(cur ? cur : ""));
        json_append(out, copy);
    }
    pg_path_free(&p);
    return out;
}

/* ── verdict / install decision ───────────────────────────────────────────── */

static int pg_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Sorted unique strings from a JSON array of objects, reading `key`. */
static char **pg_unique_sorted(const json_t *items, const char *key, size_t *out_n) {
    size_t i, n = items && items->type == JSON_ARRAY ? json_len(items) : 0;
    char **vals = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t count = 0;
    if (!vals) {
        *out_n = 0;
        return NULL;
    }
    for (i = 0; i < n; i++) {
        json_t *it = json_get(items, i);
        const char *v;
        size_t k;
        int dup = 0;
        if (!it || it->type != JSON_OBJECT) continue;
        v = json_get_str(it, key, "");
        for (k = 0; k < count; k++)
            if (strcmp(vals[k], v) == 0) { dup = 1; break; }
        if (dup) continue;
        vals[count++] = (char *)v;
    }
    if (count > 1) qsort(vals, count, sizeof(char *), pg_cmp_str);
    *out_n = count;
    return vals;
}

json_t *w34_pluginsins_608012_dangerous_summary(const json_t *findings) {
    size_t i, n = findings && findings->type == JSON_ARRAY ? json_len(findings) : 0;
    size_t count = 0, uniq = 0, k, total = 64;
    char **ids;
    char *buf;

    ids = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!ids) return json_string("");
    for (i = 0; i < n; i++) {
        json_t *f = json_get(findings, i);
        const char *sev, *pid;
        int dup = 0;
        if (!f || f->type != JSON_OBJECT) continue;
        sev = json_get_str(f, "severity", "");
        if (strcmp(sev, "critical") != 0) continue;
        count++;
        pid = json_get_str(f, "pattern_id", "");
        for (k = 0; k < uniq; k++)
            if (strcmp(ids[k], pid) == 0) { dup = 1; break; }
        if (!dup) ids[uniq++] = (char *)pid;
    }
    if (uniq > 1) qsort(ids, uniq, sizeof(char *), pg_cmp_str);
    for (i = 0; i < uniq; i++) total += strlen(ids[i]) + 2;
    buf = (char *)malloc(total);
    if (!buf) { free(ids); return json_string(""); }
    snprintf(buf, total, "%llu critical of %llu findings",
             (unsigned long long)count, (unsigned long long)n);
    if (uniq > 0) {
        size_t off = strlen(buf);
        snprintf(buf + off, total - off, " (");
        off = strlen(buf);
        for (i = 0; i < uniq; i++)
            off += (size_t)snprintf(buf + off, total - off, "%s%s", i ? ", " : "", ids[i]);
        snprintf(buf + off, total - off, ")");
    }
    free(ids);
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w34_pluginsins_608012_should_allow(const char *verdict, const json_t *findings, int force) {
    size_t n = findings && findings->type == JSON_ARRAY ? json_len(findings) : 0;
    json_t *o = json_object();
    char stackbuf[512];
    char *buf = stackbuf;
    size_t cap = sizeof(stackbuf);

    if (verdict && strcmp(verdict, "safe") == 0) {
        json_set(o, "allowed", json_bool(1));
        json_set(o, "reason", json_string("Allowed (clean scan)"));
        return o;
    }
    if (verdict && strcmp(verdict, "caution") == 0) {
        snprintf(buf, cap, force ? "Force-installed despite caution verdict (%llu findings)"
                                 : "Requires confirmation (caution verdict, %llu findings)",
                 (unsigned long long)n);
        json_set(o, "allowed", force ? json_bool(1) : json_null());
        json_set(o, "reason", json_string(buf));
        return o;
    }
    {
        json_t *summary = w34_pluginsins_608012_dangerous_summary(findings);
        const char *s = summary && summary->str_val ? summary->str_val : "";
        size_t need = strlen(s) + 160;
        if (need > cap) {
            buf = (char *)malloc(need);
            cap = need;
        }
        if (!buf) { json_free(summary); return o; }
        snprintf(buf, cap,
                 "Blocked (dangerous verdict, %s). --force does not override a dangerous verdict.", s);
        json_set(o, "allowed", json_bool(0));
        json_set(o, "reason", json_string(buf));
        if (buf != stackbuf) free(buf);
        json_free(summary);
    }
    return o;
}

json_t *w34_pluginsins_608012_scan_summary(const char *name, const char *verdict,
                                           const json_t *findings) {
    size_t n = findings && findings->type == JSON_ARRAY ? json_len(findings) : 0;
    char *buf;
    size_t cap = 512;

    if (n == 0) {
        cap = strlen(name ? name : "") + 64;
        buf = (char *)malloc(cap);
        if (!buf) return json_string("");
        snprintf(buf, cap, "%s: clean scan, no threats detected", name ? name : "");
    } else {
        size_t uniq = 0, i, off;
        char **cats = pg_unique_sorted(findings, "category", &uniq);
        for (i = 0; i < uniq; i++) cap += strlen(cats[i]) + 2;
        buf = (char *)malloc(cap);
        if (!buf) { free(cats); return json_string(""); }
        snprintf(buf, cap, "%s: %s — %llu finding(s) in ", name ? name : "",
                 verdict ? verdict : "", (unsigned long long)n);
        off = strlen(buf);
        for (i = 0; i < uniq; i++)
            off += (size_t)snprintf(buf + off, cap - off, "%s%s", i ? ", " : "", cats[i]);
        free(cats);
    }
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w34_pluginsins_608012_provenance(const char *verdict, const char *source) {
    json_t *o = json_object();
    json_set(o, "scanner_version", json_string("plugin-guard-v15"));
    json_set(o, "verdict", json_string(verdict ? verdict : ""));
    json_set(o, "source", json_string(source ? source : ""));
    return o;
}

/* ── structural limits and findings ───────────────────────────────────────── */

json_t *w34_pluginsins_608012_limits(void) {
    json_t *o = json_object(), *a, *m;
    size_t i;
    static const char *const *sets[] = {
        PG_EXCLUDED_DIRS, PG_CODE_EXTS, PG_CHANGELOGS, PG_BYTECODE_SUFFIXES, PG_BINARY_EXTS,
        PG_CODE_EXEMPT_IDS, PG_PROSE_KEEP_IDS, PG_CATALOG_IDS,
    };
    static const char *const names[] = {
        "excluded_dirs", "code_file_extensions", "changelog_filenames", "bytecode_suffixes",
        "suspicious_binary_extensions", "code_exempt_pattern_ids", "prose_keep_ids", "catalog_ids",
    };
    static const size_t sizes[] = {
        PG_ARRAY_LEN(PG_EXCLUDED_DIRS), PG_ARRAY_LEN(PG_CODE_EXTS), PG_ARRAY_LEN(PG_CHANGELOGS),
        PG_ARRAY_LEN(PG_BYTECODE_SUFFIXES), PG_ARRAY_LEN(PG_BINARY_EXTS),
        PG_ARRAY_LEN(PG_CODE_EXEMPT_IDS), PG_ARRAY_LEN(PG_PROSE_KEEP_IDS),
        PG_ARRAY_LEN(PG_CATALOG_IDS),
    };

    for (i = 0; i < PG_ARRAY_LEN(names); i++) {
        const char **sorted = (const char **)calloc(sizes[i], sizeof(char *));
        size_t k;
        if (!sorted) continue;
        for (k = 0; k < sizes[i]; k++) sorted[k] = sets[i][k];
        qsort(sorted, sizes[i], sizeof(char *), pg_cmp_str);
        a = json_array();
        for (k = 0; k < sizes[i]; k++) json_append(a, json_string(sorted[k]));
        json_set(o, names[i], a);
        free(sorted);
    }
    json_set(o, "scanner_version", json_string("plugin-guard-v15"));
    json_set(o, "max_plugin_file_count", json_number(400));
    json_set(o, "max_plugin_total_size_kb", json_number(10240));
    json_set(o, "max_plugin_single_file_kb", json_number(1024));

    m = json_object();
    for (i = 0; i < PG_ARRAY_LEN(PG_COMMENT_PREFIXES); i++)
        json_set(m, PG_COMMENT_PREFIXES[i].id, json_string(PG_COMMENT_PREFIXES[i].sev));
    json_set(o, "comment_prefixes", m);

    m = json_object();
    json_set(m, "critical", json_string("high"));
    json_set(m, "high", json_string("medium"));
    json_set(o, "comment_severity_cap", m);

    m = json_object();
    for (i = 0; i < PG_ARRAY_LEN(PG_SEVERITY_REMAP); i++)
        json_set(m, PG_SEVERITY_REMAP[i].id, json_string(PG_SEVERITY_REMAP[i].sev));
    json_set(o, "severity_remap", m);

    m = json_object();
    for (i = 0; i < PG_ARRAY_LEN(PG_JS_REMAP); i++)
        json_set(m, PG_JS_REMAP[i].id, json_string(PG_JS_REMAP[i].sev));
    json_set(o, "js_capability_remap", m);

    m = json_object();
    for (i = 0; i < PG_ARRAY_LEN(PG_DOC_PROSE_DEMOTIONS); i++)
        json_set(m, PG_DOC_PROSE_DEMOTIONS[i].id, json_string(PG_DOC_PROSE_DEMOTIONS[i].sev));
    json_set(o, "doc_prose_demotions", m);

    m = json_object();
    for (i = 0; i < PG_ARRAY_LEN(PG_MAIN_GUARD_DEMOTIONS); i++)
        json_set(m, PG_MAIN_GUARD_DEMOTIONS[i].id, json_string(PG_MAIN_GUARD_DEMOTIONS[i].sev));
    json_set(o, "main_guard_demotions", m);

    m = json_object();
    json_set(m, "low", json_number(0));
    json_set(m, "medium", json_number(1));
    json_set(m, "high", json_number(2));
    json_set(m, "critical", json_number(3));
    json_set(o, "severity_rank", m);
    return o;
}

static json_t *pg_finding(const char *pid, const char *sev, const char *cat, const char *file,
                          const char *match, const char *desc) {
    json_t *o = json_object();
    json_set(o, "pattern_id", json_string(pid));
    json_set(o, "severity", json_string(sev));
    json_set(o, "category", json_string(cat));
    json_set(o, "file", json_string(file));
    json_set(o, "line", json_number(0));
    json_set(o, "match", json_string(match));
    json_set(o, "description", json_string(desc));
    return o;
}

static long long pg_floor_kb(double size) {
    long long s = (long long)size;
    return s >= 0 ? s / 1024 : -(((-s) + 1023) / 1024);
}

json_t *w34_pluginsins_608012_size_finding(const char *rel, double size) {
    char match[64], desc[128];
    if (!(size > 1024.0 * 1024.0)) return json_null();
    snprintf(match, sizeof(match), "%lldKB", pg_floor_kb(size));
    snprintf(desc, sizeof(desc), "file is %lldKB (limit: 1024KB)", pg_floor_kb(size));
    return pg_finding("oversized_file", "medium", "structural", rel ? rel : "", match, desc);
}

json_t *w34_pluginsins_608012_binary_finding(const char *rel) {
    pg_path p;
    char ext[32];
    size_t i;
    char match[64], desc[160];
    json_t *out;

    pg_path_parse(&p, rel);
    for (i = 0; i + 1 < sizeof(ext) && p.suffix[i]; i++)
        ext[i] = (char)(p.suffix[i] >= 'A' && p.suffix[i] <= 'Z' ? p.suffix[i] + 32 : p.suffix[i]);
    ext[i] = '\0';
    if (!pg_ci_in_set(ext, PG_BINARY_EXTS, PG_ARRAY_LEN(PG_BINARY_EXTS))) {
        pg_path_free(&p);
        return json_null();
    }
    snprintf(match, sizeof(match), "binary: %s", ext);
    snprintf(desc, sizeof(desc), "binary/executable file (%s) bundled in plugin (cannot be scanned)", ext);
    out = pg_finding("binary_file", "high", "structural", rel ? rel : "", match, desc);
    pg_path_free(&p);
    return out;
}

json_t *w34_pluginsins_608012_bytecode_finding(const char *rel) {
    return pg_finding("bytecode_file", "high", "structural", rel ? rel : "", "committed bytecode",
                      "committed Python bytecode runs in place of the scanned source (catalog rule 17)");
}

json_t *w34_pluginsins_608012_manifest_findings(double file_count, double total_size) {
    json_t *out = json_array();
    long long fc = (long long)file_count, ts = (long long)total_size;
    char match[64], desc[128];

    if (fc > 400) {
        snprintf(match, sizeof(match), "%lld files", fc);
        snprintf(desc, sizeof(desc), "plugin has %lld files (limit: 400)", fc);
        json_append(out, pg_finding("too_many_files", "medium", "structural", "(directory)", match, desc));
    }
    if (ts > 10240LL * 1024LL) {
        snprintf(match, sizeof(match), "%lldKB", ts / 1024);
        snprintf(desc, sizeof(desc), "plugin is %lldKB total (limit: 10240KB)", ts / 1024);
        json_append(out, pg_finding("oversized_bundle", "medium", "structural", "(directory)", match, desc));
    }
    return out;
}
