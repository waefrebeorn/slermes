/*
 * skill_utils.c — Lightweight skill metadata utilities for C Hermes.
 *
 * Port of Python agent/skill_utils.py (566 lines).
 * Zero external dependencies beyond libyaml and stdlib.
 *
 * MIT License — WuBu Hermes Project
 */

#include "skill_utils.h"
#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Excluded directories ─────────────────────────────────── */

static const char *EXCLUDED_DIRS[] = {
    ".git", ".github", ".hub", ".archive", ".venv", "venv",
    "node_modules", "site-packages", "__pycache__",
    ".tox", ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache",
    NULL
};

/* Port of Python agent/skill_utils.py:is_excluded_skill_path */
bool is_excluded_skill_path(const char *path) {
    if (!path || !path[0]) return false;

    /* Tokenize path by / and check each component */
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", path);

    char *save;
    const char *part = strtok_r(buf, "/", &save);
    while (part) {
        for (int i = 0; EXCLUDED_DIRS[i]; i++) {
            if (strcmp(part, EXCLUDED_DIRS[i]) == 0)
                return true;
        }
        part = strtok_r(NULL, "/", &save);
    }
    return false;
}

/* ── Frontmatter parsing ──────────────────────────────────── */

/* Port of Python tools/skills_hub.py:_parse_frontmatter(). */
/* Port of Python agent/skill_utils.py:parse_frontmatter */
int parse_frontmatter(const char *content, skill_frontmatter_t *fm) {
    if (!content || !fm) return -1;
    memset(fm, 0, sizeof(*fm));

    /* Must start with --- */
    if (strncmp(content, "---", 3) != 0)
        return 0;

    /* Find closing --- */
    const char *body_start = content + 3;
    const char *end_marker = strstr(body_start, "\n---");
    if (!end_marker) {
        end_marker = strstr(body_start, "\n---\n");
        if (!end_marker) return 0;
    }

    /* Extract YAML content between markers */
    size_t yaml_len = (size_t)(end_marker - body_start);
    if (yaml_len == 0 || yaml_len > 16384) return 0;

    char yaml_buf[16384];
    memcpy(yaml_buf, body_start, yaml_len);
    yaml_buf[yaml_len] = '\0';

    /* Try libyaml parser first */
    char *yaml_err = NULL;
    yaml_doc_t *doc = yaml_parse(yaml_buf, &yaml_err);
    if (doc) {
        /* Iterate top-level keys */
        int idx = 0;
        /* We need to get specific keys we know about */
        const char *known_keys[] = {
            "name", "description", "category",
            "platforms", "metadata", NULL
        };
        for (int k = 0; known_keys[k] && idx < 64; k++) {
            const char *val = yaml_get_string(doc, known_keys[k]);
            if (val && val[0]) {
                snprintf(fm->entries[idx].key, sizeof(fm->entries[idx].key), "%s", known_keys[k]);
                snprintf(fm->entries[idx].value, sizeof(fm->entries[idx].value), "%s", val);
                idx++;
            }
        }

        /* Handle lists: platforms is a list, not a string */
        size_t plat_count = yaml_list_count(doc, "platforms");
        if (plat_count > 0) {
            char plat_buf[1024] = "";
            for (size_t i = 0; i < plat_count; i++) {
                const char *p = yaml_list_get(doc, "platforms", i);
                if (p) {
                    if (plat_buf[0]) strncat(plat_buf, ",", sizeof(plat_buf) - strlen(plat_buf) - 1);
                    strncat(plat_buf, p, sizeof(plat_buf) - strlen(plat_buf) - 1);
                }
            }
            snprintf(fm->entries[idx].key, sizeof(fm->entries[idx].key), "platforms");
            snprintf(fm->entries[idx].value, sizeof(fm->entries[idx].value), "%s", plat_buf);
            idx++;
        }

        /* Handle nested metadata.hermes config fields via yaml_get_string */
        const char *hermes_paths[][2] = {
            {"metadata", "metadata"},
            {"metadata.hermes", "metadata.hermes"},
            {"metadata.hermes.fallback_for_toolsets", "metadata.hermes.fallback_for_toolsets"},
            {"metadata.hermes.requires_toolsets", "metadata.hermes.requires_toolsets"},
            {"metadata.hermes.fallback_for_tools", "metadata.hermes.fallback_for_tools"},
            {"metadata.hermes.requires_tools", "metadata.hermes.requires_tools"},
            {"", ""}
        };
        for (int p = 0; hermes_paths[p][0][0] && idx < 64; p++) {
            const char *v = yaml_get_string(doc, hermes_paths[p][0]);
            if (v && v[0]) {
                snprintf(fm->entries[idx].key, sizeof(fm->entries[idx].key), "%s", hermes_paths[p][1]);
                snprintf(fm->entries[idx].value, sizeof(fm->entries[idx].value), "%s", v);
                idx++;
            }
        }

        /* Try to get config vars from metadata.hermes.config */
        size_t cfg_count = yaml_list_count(doc, "metadata.hermes.config");
        if (cfg_count > 0 && idx < 64) {
            snprintf(fm->entries[idx].key, sizeof(fm->entries[idx].key), "metadata.hermes.config._count");
            char cnt[32];
            snprintf(cnt, sizeof(cnt), "%zu", cfg_count);
            snprintf(fm->entries[idx].value, sizeof(fm->entries[idx].value), "%s", cnt);
            idx++;
        }

        fm->count = idx;
        yaml_free(doc);
        return idx;
    }
    free(yaml_err);

    /* Fallback: simple key:value parsing for malformed YAML */
    int idx = 0;
    char *line_save;
    char *line = strtok_r(yaml_buf, "\n", &line_save);
    while (line && idx < 64) {
        /* Skip leading whitespace */
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '#' || *line == '\0') { line = strtok_r(NULL, "\n", &line_save); continue; }

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
            char *val = colon + 1;
            /* Trim key */
            char *ke = key + strlen(key) - 1;
            while (ke >= key && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
            /* Trim value */
            while (*val == ' ' || *val == '\t') val++;
            if (key[0] && val[0]) {
                snprintf(fm->entries[idx].key, sizeof(fm->entries[idx].key), "%s", key);
                snprintf(fm->entries[idx].value, sizeof(fm->entries[idx].value), "%s", val);
                idx++;
            }
        }
        line = strtok_r(NULL, "\n", &line_save);
    }

    fm->count = idx;
    return idx;
}

const char *skill_fm_get(const skill_frontmatter_t *fm, const char *key) {
    if (!fm || !key) return NULL;
    for (int i = 0; i < fm->count; i++) {
        if (strcmp(fm->entries[i].key, key) == 0)
            return fm->entries[i].value;
    }
    return NULL;
}

bool skill_fm_get_bool(const skill_frontmatter_t *fm, const char *key, bool def) {
    const char *v = skill_fm_get(fm, key);
    if (!v) return def;
    if (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "1") == 0)
        return true;
    return false;
}

/* ── Platform matching ────────────────────────────────────── */

static const char *PLATFORM_MAP[][2] = {
    {"macos", "darwin"},
    {"linux", "linux"},
    {"windows", "win32"},
    {"", ""}
};

/* Port of Python agent/skill_utils.py:skill_matches_platform */
bool skill_matches_platform(const skill_frontmatter_t *fm) {
    if (!fm) return true;

    const char *platforms_str = skill_fm_get(fm, "platforms");
    if (!platforms_str || !platforms_str[0])
        return true; /* no platform constraint = compatible with all */

#ifdef __linux__
    const char *current_platform = "linux";
#elif __APPLE__
    const char *current_platform = "darwin";
#elif _WIN32
    const char *current_platform = "win32";
#else
    const char *current_platform = "linux"; /* fallback */
#endif

    /* Parse comma-separated platform list */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", platforms_str);

    int running_in_termux = 0;
    const char *termux_env = getenv("TERMUX_VERSION");
    if (termux_env && termux_env[0]) running_in_termux = 1;

    char *save;
    const char *plat = strtok_r(buf, ", ", &save);
    while (plat) {
        /* Trim */
        while (*plat == ' ' || *plat == '\t') plat++;
        const char *mapped = plat;
        for (int i = 0; PLATFORM_MAP[i][0][0]; i++) {
            if (strcmp(plat, PLATFORM_MAP[i][0]) == 0) {
                mapped = PLATFORM_MAP[i][1];
                break;
            }
        }
        if (strncmp(current_platform, mapped, strlen(mapped)) == 0)
            return true;
        /* Termux compat */
        if (running_in_termux && strcmp(mapped, "linux") == 0)
            return true;
        if (running_in_termux && (strcmp(mapped, "termux") == 0 || strcmp(mapped, "android") == 0))
            return true;
        plat = strtok_r(NULL, ", ", &save);
    }

    return false;
}

/* ── Disabled skills ──────────────────────────────────────── */

/* Port of Python agent/skill_utils.py:get_disabled_skill_names */
char *skill_get_disabled_names(const char *config_path, const char *platform) {
    if (!config_path || !config_path[0]) return NULL;

    struct stat st;
    if (stat(config_path, &st) != 0 || !S_ISREG(st.st_mode))
        return NULL;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &err);
    if (!doc) {
        free(err);
        return NULL;
    }

    /* Collect disabled names from skills.disabled */
    size_t disabled_count = yaml_list_count(doc, "skills.disabled");
    /* Also from skills.platform_disabled.<platform> */
    size_t plat_disabled_count = 0;
    char plat_path[256];
    char *result = NULL;
    size_t result_len = 0;
    size_t result_cap = 0;

    /* Helper: add a name to result if not already present */
    #define ADD_NAME(name) do { \
        const char *_n = (name); \
        if (_n && _n[0] && !strstr(result ? result : "", _n)) { \
            size_t _len = strlen(_n); \
            if (result_len + _len + 2 > result_cap) { \
                result_cap = result_cap ? result_cap * 2 : 1024; \
                char *_nb = realloc(result, result_cap); \
                if (!_nb) { free(result); yaml_free(doc); return NULL; } \
                result = _nb; \
            } \
            if (result_len > 0) result[result_len++] = ','; \
            memcpy(result + result_len, _n, _len); \
            result_len += _len; \
            result[result_len] = '\0'; \
        } \
    } while(0)

    for (size_t i = 0; i < disabled_count; i++) {
        const char *name = yaml_list_get(doc, "skills.disabled", i);
        ADD_NAME(name);
    }

    /* Check platform-specific disabled */
    if (platform && platform[0]) {
        snprintf(plat_path, sizeof(plat_path), "skills.platform_disabled.%s", platform);
        plat_disabled_count = yaml_list_count(doc, plat_path);
        for (size_t i = 0; i < plat_disabled_count; i++) {
            const char *name = yaml_list_get(doc, plat_path, i);
            ADD_NAME(name);
        }
    }

    #undef ADD_NAME

    yaml_free(doc);

    if (result && result[0]) return result;
    free(result);
    return NULL;
}

/* ── External skills directories ──────────────────────────── */

/* Port of Python agent/skill_utils.py:get_external_skills_dirs */
char **skill_get_external_dirs(const char *config_path,
                               const char *hermes_home,
                               const char *local_skills_dir,
                               size_t *count) {
    *count = 0;
    if (!config_path || !config_path[0]) return NULL;

    struct stat st;
    if (stat(config_path, &st) != 0 || !S_ISREG(st.st_mode))
        return NULL;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &err);
    if (!doc) {
        free(err);
        return NULL;
    }

    size_t raw_count = yaml_list_count(doc, "skills.external_dirs");
    if (raw_count == 0) {
        yaml_free(doc);
        return NULL;
    }

    /* Allocate max possible */
    char **result = calloc(raw_count + 1, sizeof(char *));
    if (!result) { yaml_free(doc); return NULL; }

    size_t idx = 0;
    for (size_t i = 0; i < raw_count; i++) {
        const char *entry = yaml_list_get(doc, "skills.external_dirs", i);
        if (!entry || !entry[0]) continue;

        /* Expand ~ and ${VAR} */
        char expanded[4096];
        size_t epos = 0;
        for (size_t j = 0; entry[j] && epos < sizeof(expanded) - 1; j++) {
            if (entry[j] == '~') {
                const char *home = getenv("HOME");
                if (home) {
                    size_t hlen = strlen(home);
                    if (epos + hlen < sizeof(expanded) - 1) {
                        memcpy(expanded + epos, home, hlen);
                        epos += hlen;
                    }
                }
            } else if (entry[j] == '$' && entry[j+1] == '{') {
                /* Find closing } */
                const char *close = strchr(entry + j + 2, '}');
                if (close) {
                    size_t vlen = (size_t)(close - entry - j - 2);
                    char vname[256];
                    if (vlen < sizeof(vname)) {
                        memcpy(vname, entry + j + 2, vlen);
                        vname[vlen] = '\0';
                        const char *vval = getenv(vname);
                        if (vval) {
                            size_t vvlen = strlen(vval);
                            if (epos + vvlen < sizeof(expanded) - 1) {
                                memcpy(expanded + epos, vval, vvlen);
                                epos += vvlen;
                            }
                        }
                    }
                    j = (size_t)(close - entry);
                }
            } else {
                expanded[epos++] = entry[j];
            }
        }
        expanded[epos] = '\0';

        /* Resolve relative paths against hermes_home */
        char resolved[8192];
        if (expanded[0] != '/' && hermes_home) {
            int n = snprintf(resolved, sizeof(resolved), "%s/%s", hermes_home, expanded);
            if (n < 0 || (size_t)n >= sizeof(resolved)) continue;
        } else {
            snprintf(resolved, sizeof(resolved), "%s", expanded);
        }

        /* Must exist and be a directory */
        if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        /* Skip if it matches the local skills dir */
        if (local_skills_dir && strcmp(resolved, local_skills_dir) == 0)
            continue;

        /* Check for duplicates */
        bool dup = false;
        for (size_t d = 0; d < idx; d++) {
            if (strcmp(result[d], resolved) == 0) { dup = true; break; }
        }
        if (dup) continue;

        result[idx] = strdup(resolved);
        if (result[idx]) idx++;
    }

    yaml_free(doc);
    *count = idx;
    return result;
}

/* ── All skills directories ───────────────────────────────── */

/* Port of Python agent/skill_utils.py:get_all_skills_dirs */
char **skill_get_all_dirs(const char *config_path,
                          const char *hermes_home,
                          const char *local_skills_dir,
                          size_t *count) {
    /* Count: local + external */
    size_t ext_count = 0;
    char **ext = NULL;
    if (config_path && config_path[0]) {
        ext = skill_get_external_dirs(config_path, hermes_home, local_skills_dir, &ext_count);
    }

    size_t total = 1 + ext_count;
    char **result = calloc(total + 1, sizeof(char *));
    if (!result) { free(ext); *count = 0; return NULL; }

    size_t idx = 0;
    /* Local first */
    if (local_skills_dir) {
        result[idx++] = strdup(local_skills_dir);
    }
    /* External follow */
    for (size_t i = 0; i < ext_count; i++) {
        result[idx++] = ext[i]; /* transfer ownership */
    }
    free(ext); /* free the array, not the strings */

    *count = idx;
    return result;
}

/* ── Condition extraction ─────────────────────────────────── */

/* Port of Python agent/skill_utils.py:extract_skill_conditions */
void extract_skill_conditions(const skill_frontmatter_t *fm,
                              skill_conditions_t *conds) {
    if (!fm || !conds) return;
    memset(conds, 0, sizeof(*conds));

    const char *v;

    v = skill_fm_get(fm, "metadata.hermes.fallback_for_toolsets");
    if (v) snprintf(conds->fallback_for_toolsets, sizeof(conds->fallback_for_toolsets), "%s", v);

    v = skill_fm_get(fm, "metadata.hermes.requires_toolsets");
    if (v) snprintf(conds->requires_toolsets, sizeof(conds->requires_toolsets), "%s", v);

    v = skill_fm_get(fm, "metadata.hermes.fallback_for_tools");
    if (v) snprintf(conds->fallback_for_tools, sizeof(conds->fallback_for_tools), "%s", v);

    v = skill_fm_get(fm, "metadata.hermes.requires_tools");
    if (v) snprintf(conds->requires_tools, sizeof(conds->requires_tools), "%s", v);
}

/* ── Config variable extraction ───────────────────────────── */

/* Port of Python agent/skill_utils.py:extract_skill_config_vars */
int extract_skill_config_vars(const skill_frontmatter_t *fm,
                              skill_config_var_t *vars, int max_vars) {
    if (!fm || !vars || max_vars <= 0) return 0;

    int idx = 0;

    /* Read config entries from frontmatter's metadata.hermes.config.
     * Since our simple frontmatter parser doesn't handle nested YAML lists
     * of objects, we check the config._count marker and try yaml_get_string
     * paths for indexed access. For simplicity, try common patterns. */
    const char *cfg_count_str = skill_fm_get(fm, "metadata.hermes.config._count");
    (void)cfg_count_str; /* we know config entries exist */

    /* Simple approach: check for known config patterns in the raw content.
     * The Python code expects a list of objects with key/description/default/prompt.
     * For now return empty — the full YAML-based approach needs doc access,
     * which requires the caller to pass the raw content or yaml_doc. */
    return idx;
}

/* ── Description extraction ───────────────────────────────── */

/* Port of Python agent/skill_utils.py:extract_skill_description */
void extract_skill_description(const skill_frontmatter_t *fm,
                               char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!fm) return;

    const char *desc = skill_fm_get(fm, "description");
    if (!desc) return;

    size_t len = strlen(desc);
    if (len == 0) return;

    /* Strip quotes */
    const char *start = desc;
    while (*start == '\'' || *start == '"') start++;

    char clean[SKILL_UTILS_VAL_MAX];
    size_t cpos = 0;
    for (size_t i = 0; start[i] && cpos < sizeof(clean) - 1; i++) {
        if (start[i] != '\'' && start[i] != '"')
            clean[cpos++] = start[i];
    }
    clean[cpos] = '\0';

    /* Truncate at 60 chars */
    if (cpos > 60) {
        memcpy(out, clean, 57);
        out[57] = '.';
        out[58] = '.';
        out[59] = '.';
        out[60] = '\0';
    } else {
        snprintf(out, out_size, "%s", clean);
    }
}

/* ── File iteration ───────────────────────────────────────── */

/* Port of Python agent/skill_utils.py:iter_skill_index_files */
int iter_skill_index_files(const char *skills_dir, const char *filename,
                           skill_iter_cb callback, void *user) {
    if (!skills_dir || !filename || !callback) return 0;

    struct stat st;
    if (stat(skills_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *d = opendir(skills_dir);
    if (!d) return 0;

    /* Collect matching paths first, then sort */
    typedef struct {
        char path[8192];
    } match_t;

    match_t matches[512];
    int match_count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && match_count < 512) {
        if (entry->d_name[0] == '.') continue;

        /* Build full path */
        char full[8192];
        snprintf(full, sizeof(full), "%s/%s", skills_dir, entry->d_name);

        /* Must be a directory */
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        /* Skip excluded */
        if (is_excluded_skill_path(entry->d_name))
            continue;

        /* Check for the target filename inside */
        char target[8192];
        int n = snprintf(target, sizeof(target), "%s/%s", full, filename);
        if (n < 0 || (size_t)n >= sizeof(target)) continue;
        if (stat(target, &st) == 0 && S_ISREG(st.st_mode)) {
            /* Check path fits before building final match path */
            size_t full_len = strlen(full);
            size_t fn_len = strlen(filename);
            if (full_len + fn_len + 2 <= sizeof(matches[match_count].path)) {
                snprintf(matches[match_count].path, sizeof(matches[match_count].path), "%s/%s", full, filename);
                match_count++;
            }
        }
    }
    closedir(d);

    /* Sort alphabetically (simple bubble sort for small N) */
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = 0; j < match_count - 1 - i; j++) {
            if (strcmp(matches[j].path, matches[j + 1].path) > 0) {
                match_t tmp = matches[j];
                matches[j] = matches[j + 1];
                matches[j + 1] = tmp;
            }
        }
    }

    /* Invoke callback */
    int count = 0;
    for (int i = 0; i < match_count; i++) {
        if (callback(matches[i].path, user))
            break;
        count++;
    }

    return count;
}

/* ── Namespace helpers ────────────────────────────────────── */

/* Port of Python agent/skill_utils.py:parse_qualified_name */
void parse_qualified_name(const char *name,
                                char **namespace_out,
                                char **bare_name_out) {
    *namespace_out = NULL;
    *bare_name_out = NULL;
    if (!name) return;

    const char *colon = strchr(name, ':');
    if (!colon || colon == name) {
        *bare_name_out = strdup(name);
        return;
    }

    size_t ns_len = (size_t)(colon - name);
    char *ns = malloc(ns_len + 1);
    memcpy(ns, name, ns_len);
    ns[ns_len] = '\0';
    *namespace_out = ns;

    *bare_name_out = strdup(colon + 1);
}

/* Port of Python agent/skill_utils.py:is_valid_namespace */
bool is_valid_namespace(const char *candidate) {
    if (!candidate || !candidate[0]) return false;
    for (size_t i = 0; candidate[i]; i++) {
        char c = candidate[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* ──── New thin wrappers (port of Python) ──────────────── */

/* Port of Python agent/skill_utils.py:_normalize_string_set
 * Deduplicate a comma/space-separated string, return malloc'd result. */
char *skill_normalize_string_set(const char *input)
{
    if (!input || !input[0]) return strdup("");

    /* Tokenize by comma or whitespace */
    char *copy = strdup(input);
    if (!copy) return strdup("");

    /* Use a simple array for dedup */
    const int MAX_TOKENS = 256;
    char *tokens[MAX_TOKENS];
    int count = 0;

    char *save;
    char *tok = strtok_r(copy, ", \t\n\r", &save);
    while (tok && count < MAX_TOKENS) {
        /* Skip empty tokens */
        if (tok[0]) {
            /* Check for duplicate */
            bool dup = false;
            for (int i = 0; i < count; i++) {
                if (strcmp(tokens[i], tok) == 0) { dup = true; break; }
            }
            if (!dup) {
                tokens[count] = tok;
                count++;
            }
        }
        tok = strtok_r(NULL, ", \t\n\r", &save);
    }

    /* Build result */
    size_t total = 0;
    for (int i = 0; i < count; i++)
        total += strlen(tokens[i]) + 2;

    char *result = malloc(total + 1);
    if (!result) { free(copy); return strdup(""); }
    result[0] = '\0';

    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(result, ", ");
        strcat(result, tokens[i]);
    }

    free(copy);
    return result;
}

/* Port of Python agent/skill_utils.py:_resolve_dotpath
 * Resolve "a.b.c" into a nested key value from YAML frontmatter.
 * Simple implementation: walk dot-separated path through frontmatter keys
 * where the intermediate parts are stored as "a.b" prefix keys. */
const char *skill_resolve_dotpath(const skill_frontmatter_t *fm,
                                  const char *dotpath)
{
    if (!fm || !dotpath || !dotpath[0]) return NULL;

    /* Try exact match first (most flexible for single-level lookups) */
    const char *val = skill_fm_get(fm, dotpath);
    if (val) return val;

    /* Try prefix-based matching for nested paths */
    /* Build prefix: take everything up to the last dot */
    const char *last_dot = strrchr(dotpath, '.');
    if (!last_dot) return NULL;

    size_t prefix_len = (size_t)(last_dot - dotpath);
    char prefix[256];
    if (prefix_len >= sizeof(prefix)) return NULL;
    memcpy(prefix, dotpath, prefix_len);
    prefix[prefix_len] = '\0';

    const char *leaf = last_dot + 1;

    /* Scan frontmatter for entries matching {prefix}.{leaf} or just {leaf} under prefix context */
    for (int i = 0; i < fm->count; i++) {
        const char *key = fm->entries[i].key;
        size_t klen = strlen(key);
        /* Check if key starts with prefix. */
        if (klen > prefix_len + 1 &&
            strncmp(key, prefix, prefix_len) == 0 &&
            key[prefix_len] == '.' &&
            strcmp(key + prefix_len + 1, leaf) == 0) {
            return fm->entries[i].value;
        }
    }

    return NULL;
}

/* Port of Python agent/skill_utils.py:skill_matches_environment
 * Check frontmatter "environments" field against current runtime env.
 * Unknown env tags fail open (return true). */
bool skill_matches_environment(const skill_frontmatter_t *fm)
{
    if (!fm) return true;
    /* Look for "environments" entry in frontmatter */
    const char *env_val = NULL;
    for (int i = 0; i < fm->count; i++) {
        if (strcmp(fm->entries[i].key, "environments") == 0) {
            env_val = fm->entries[i].value;
            break;
        }
    }
    if (!env_val || !env_val[0]) return true; /* absent -> all environments */

    /* env_val is comma-separated list: "kanban" or "kanban, docker" etc */
    char buf[256];
    strncpy(buf, env_val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *save = NULL;
    char *tok = strtok_r(buf, ", ", &save);
    while (tok) {
        if (tok[0]) {
            /* Known environments: "kanban", "docker", "s6" */
            bool known = (strcmp(tok, "kanban") == 0 ||
                          strcmp(tok, "docker") == 0 ||
                          strcmp(tok, "s6") == 0);
            if (!known) return true; /* unknown -> fail open */
            if (skill_detect_environment(tok)) return true;
        }
        tok = strtok_r(NULL, ", ", &save);
    }
    return false;
}

/* Port of Python agent/skill_utils.py:_detect_environment
 * Detect if a named runtime environment is currently active.
 * Known: "kanban" (env vars), "docker" (/proc/1/cgroup), "s6" (/run/s6).
 * Unknown names return true (fail-open). */
bool skill_detect_environment(const char *env_name)
{
    if (!env_name || !env_name[0]) return true;

    if (strcmp(env_name, "kanban") == 0) {
        /* Kanban is active when HERMES_KANBAN_TASK or HERMES_KANBAN_BOARD is set */
        const char *task = getenv("HERMES_KANBAN_TASK");
        const char *board = getenv("HERMES_KANBAN_BOARD");
        if (task || board) return true;
        /* Check if current profile has kanban toolset (simplified: check
         * enabled_toolsets environment var or config) */
        const char *toolsets = getenv("HERMES_ENABLED_TOOLSETS");
        if (toolsets && strstr(toolsets, "kanban")) return true;
        return false;
    }

    if (strcmp(env_name, "docker") == 0) {
        /* Check /proc/1/cgroup for "docker" marker */
        FILE *f = fopen("/proc/1/cgroup", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "docker") || strstr(line, "containerd")) {
                    fclose(f);
                    return true;
                }
            }
            fclose(f);
        }
        /* Also check /.dockerenv marker */
        if (access("/.dockerenv", F_OK) == 0) return true;
        return false;
    }

    if (strcmp(env_name, "s6") == 0) {
        /* s6-overlay markers */
        struct stat st;
        if (stat("/run/s6", &st) == 0 && S_ISDIR(st.st_mode)) return true;
        if (stat("/package/admin/s6-overlay", &st) == 0 && S_ISDIR(st.st_mode)) return true;
        return false;
    }
    /* Unknown environment -> fail open */
    return true;
}

/* Port of Python agent/skill_utils.py:_external_dirs_cache_clear
 * C cache management is minimal — no-op since C doesn't cache external dirs. */
void skill_external_dirs_cache_clear(void)
{
    /* C implementation does not cache external dir results.
     * Each call to skill_get_external_dirs reads from config fresh.
     * This no-op satisfies the Python interface contract. */
}
/* Port of Python agent/skill_utils.py:discover_all_skill_config_vars
 * Consolidated: C handles per-skill config var extraction inline in
 * skill_commands.c extract_skill_config_vars_from_file (line 79).
 * The batch-scan orchestrator is not needed in C — C loads config
 * vars lazily per-skill instead of eager-scannning all skills. */
/* Port of Python agent/skill_utils.py:yaml_load
 * Thin wrapper: parse a YAML string into a C YAML node structure.
 * Returns yaml_doc_t pointer or NULL on error. */
yaml_doc_t *skill_yaml_load(const char *yaml_str)
{
    if (!yaml_str || !yaml_str[0]) return NULL;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(yaml_str, &err);
    if (err) { free(err); return NULL; }
    return doc;
}

/* Port of Python: resolve_skill_config_values — resolve skill config from config.yaml */
/* C version: returns empty JSON string (config.yaml skills parsing TBD) */
const char *resolve_skill_config_values(void) {
    return "{}";
}
