/*
 * port_agent_skill_utils.c — Port of Python agent/skill_utils.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: is_skill_support_path */
bool is_skill_support_path(const char *path) {
    if (!path) return false;
    /* Check if path is within the skill support directory */
    const char *skill_support = getenv("HERMES_SKILL_SUPPORT");
    if (!skill_support) skill_support = "/etc/hermes/skills";
    
    size_t support_len = strlen(skill_support);
    return (strncmp(path, skill_support, support_len) == 0);
}

/* === merged from port_agent_skill_utils.c === */
/*
 * port_agent_skill_utils.c — C port of agent/skill_utils.py
 */

#include "hermes_logger.h"
#include "path.h"
#include "skill_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>



/* Shared raw-config cache keyed by (path, mtime_ns, size) — mirrors Python's
 * module-level _RAW_CONFIG_CACHE. A tiny single-slot cache is enough: the
 * skill build path re-reads the same config.yaml repeatedly. */
static struct {
    char   *path;
    long    mtime_ns;   /* st_mtim.tv_sec*1e9 + tv_nsec */
    long    size;
    char   *content;    /* cached file contents */
    int     valid;
} s_raw_config_cache = {0};

/* PoP: cli_agent_skill_utils__raw_config_cache_clear @ agent/skill_utils.py:_raw_config_cache_clear */
/* Test hook — drop the shared raw config cache. */
void cli_agent_skill_utils__raw_config_cache_clear(void)
{
    free(s_raw_config_cache.path);
    free(s_raw_config_cache.content);
    s_raw_config_cache.path = NULL;
    s_raw_config_cache.content = NULL;
    s_raw_config_cache.mtime_ns = 0;
    s_raw_config_cache.size = 0;
    s_raw_config_cache.valid = 0;
}

/* PoP: cli_agent_skill_utils__load_raw_config @ agent/skill_utils.py:_load_raw_config */

/* Port of Python agent/skill_utils.py:_load_raw_config */
/* Read config.yaml with a shared mtime+size keyed cache. */
char *cli_agent_skill_utils__load_raw_config(void)
{
    const char *config_path = getenv("HERMES_CONFIG_PATH");
    char path[1024];
    if (!config_path || !*config_path) {
        /* Default: $HERMES_HOME/config.yaml */
        const char *home = getenv("HERMES_HOME");
        if (!home || !*home) {
            home = getenv("HOME");
            if (!home || !*home) {
                return strdup("{}");
            }
            snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
            config_path = path;
        } else {
            snprintf(path, sizeof(path), "%s/config.yaml", home);
            config_path = path;
        }
    }

    /* Cache lookup keyed by (path, mtime_ns, size) */
    struct stat st;
    long mtime_ns = 0, size = 0;
    int have_stat = (stat(config_path, &st) == 0);
    if (have_stat) {
        mtime_ns = (long)st.st_mtim.tv_sec * 1000000000L + (long)st.st_mtim.tv_nsec;
        size = (long)st.st_size;
        if (s_raw_config_cache.valid && s_raw_config_cache.path &&
            strcmp(s_raw_config_cache.path, config_path) == 0 &&
            s_raw_config_cache.mtime_ns == mtime_ns &&
            s_raw_config_cache.size == size) {
            return strdup(s_raw_config_cache.content);
        }
    }

    /* Read config.yaml */
    FILE *f = fopen(config_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "skill_utils", "Cannot read config: %s", config_path);
        return strdup("{}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return strdup("{}");
    }

    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) {
        fclose(f);
        return strdup("{}");
    }

    size_t n = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[n] = '\0';

    /* Populate cache */
    if (have_stat) {
        free(s_raw_config_cache.path);
        free(s_raw_config_cache.content);
        s_raw_config_cache.path = strdup(config_path);
        s_raw_config_cache.content = strdup(content);
        s_raw_config_cache.mtime_ns = mtime_ns;
        s_raw_config_cache.size = size;
        s_raw_config_cache.valid = 1;
    }

    return content;
}

/* PoP: exist @ agent/skill_utils.py:_resolve_for_skill_ownership */
/* Expand ~ then resolve to a canonical absolute path. Falls back to an
 * unresolved absolute path when the target does not exist (mirrors Python's
 * expanduser().resolve() with .absolute() fallback). Caller frees. */
char *cli_agent_skill_utils__resolve_for_skill_ownership(const char *path)
{
    if (!path) return NULL;
    /* expanduser: leading ~ → $HOME */
    char expanded[2048];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home)
            snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
        else
            snprintf(expanded, sizeof(expanded), "%s", path);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", path);
    }
    char *resolved = path_resolve(expanded);   /* realpath — NULL if missing */
    if (resolved) return resolved;
    return path_abs(expanded);                 /* absolute w/o resolving links */
}

/* PoP: cli_agent_skill_utils__is_external_skill_path @ agent/skill_utils.py:is_external_skill_path */
/* Return 1 when path lives under a configured external skills dir. */
int cli_agent_skill_utils__is_external_skill_path(const char *path)
{
    if (!path) return 0;
    char *candidate = cli_agent_skill_utils__resolve_for_skill_ownership(path);
    if (!candidate) return 0;

    const char *config_path = getenv("HERMES_CONFIG_PATH");
    char cfgbuf[1024];
    const char *home = getenv("HERMES_HOME");
    char homebuf[1024];
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (h && *h) { snprintf(homebuf, sizeof(homebuf), "%s/.hermes", h); home = homebuf; }
        else home = "";
    }
    if (!config_path || !*config_path) {
        snprintf(cfgbuf, sizeof(cfgbuf), "%s/config.yaml", home);
        config_path = cfgbuf;
    }
    char local_skills[1024];
    snprintf(local_skills, sizeof(local_skills), "%s/skills", home);

    size_t count = 0;
    char **ext = skill_get_external_dirs(config_path, home, local_skills, &count);
    int is_external = 0;
    for (size_t i = 0; i < count; i++) {
        char *root = cli_agent_skill_utils__resolve_for_skill_ownership(ext[i]);
        if (root) {
            /* path_within_dir returns NULL when candidate is inside root */
            char *err = path_within_dir(candidate, root);
            if (err == NULL) is_external = 1;
            else free(err);
            free(root);
        }
        free(ext[i]);
        if (is_external) { /* free remaining entries */
            for (size_t j = i + 1; j < count; j++) free(ext[j]);
            break;
        }
    }
    free(ext);
    free(candidate);
    return is_external;
}


/* ============================================================
 * agent/skill_utils.py REAL_GAP closures — project-skill tier.
 * C names use the bare stripped Python names.
 * ============================================================ */
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "hermes_json.h"
#include "skills_guard.h"
#include "agent_skill_utils_ports.h"

/* Forward decls for helpers defined later in this section. */
json_t *sku_iter_skill_index_files(const char *skills_dir, const char *filename);

/* PROJECT_SKILLS_SUBDIRS (agent/skill_utils.py:649). */
static const char *const s_project_skills_subdirs[] = {
    ".hermes/skills", ".agents/skills", NULL,
};
#define PROJECT_ROOT_MAX_DEPTH 64

static const char *const s_excluded_skill_dirs[] = {
    ".git", ".github", ".hub", ".archive", ".venv", "venv", "node_modules",
    "site-packages", "__pycache__", ".tox", ".nox", ".pytest_cache",
    ".mypy_cache", ".ruff_cache", NULL,
};
static const char *const s_skill_support_dirs[] = {
    "references", "templates", "assets", "scripts", NULL,
};
#define ORG_MIRROR_DIR_NAME "_org"
#define ORG_ACTIVE_MARKER ".active_org"
#define PROJECT_SCAN_SOURCE "project-local"

static bool su_str_in_set(const char *s, const char *const *set) {
    for (int i = 0; set[i]; i++)
        if (strcmp(s, set[i]) == 0) return true;
    return false;
}

/* Profile-local skills root: $HERMES_HOME/skills (default ~/.hermes/skills). */
static void su_local_skills_dir(char *out, size_t out_size) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) { snprintf(out, out_size, "%s", ""); return; }
    snprintf(out, out_size, "%s/.hermes/skills", home);
}

/* Raw config as a parsed JSON tree. Caller frees. */
static json_t *su_load_raw_config_json(void) {
    char *raw = cli_agent_skill_utils__load_raw_config();
    if (!raw) return NULL;
    json_t *parsed = json_parse_yaml_real(raw);
    free(raw);
    return parsed;
}

/* PoP: sku_parse_literal_list @ agent/skill_utils.py:parse_config_string_list
 * Mini ast.literal_eval for a bracketed scalar list ('["a","b"]' /
 * "['a']" / "[1, 2]"). Returns a JSON array of item strings, or NULL when
 * the text does not parse as a list. Caller frees. */
static json_t *su_parse_literal_list(const char *text) {
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '[') return NULL;
    p++;
    json_t *items = json_array();
    for (;;) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == ']') { p++; break; }
        if (*p == ',') { p++; continue; }
        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            char buf[1024];
            size_t n = 0;
            while (*p && *p != quote && n + 1 < sizeof(buf)) {
                if (*p == '\\' && p[1]) p++;   /* keep escaped char literally */
                buf[n++] = *p++;
            }
            buf[n] = '\0';
            if (*p == quote) p++;
            json_append(items, json_string(buf));
        } else if (*p) {
            /* bare scalar: number / True / False / None until , or ] */
            char buf[128];
            size_t n = 0;
            while (*p && *p != ',' && *p != ']' && n + 1 < sizeof(buf))
                buf[n++] = *p++;
            buf[n] = '\0';
            /* trim */
            while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
            char *b = buf;
            while (*b && isspace((unsigned char)*b)) b++;
            if (strcmp(b, "True") == 0) json_append(items, json_string("True"));
            else if (strcmp(b, "False") == 0) json_append(items, json_string("False"));
            else if (strcmp(b, "None") == 0) json_append(items, json_string("None"));
            else {
                char *end = NULL;
                double d = strtod(b, &end);
                if (end != b && *end == '\0') {
                    long l = strtol(b, &end, 10);
                    if (end != b && *end == '\0')
                        snprintf(buf, sizeof(buf), "%ld", l);   /* str(1)="1" */
                    else
                        snprintf(buf, sizeof(buf), "%g", d);
                    json_append(items, json_string(buf));
                } else {
                    json_free(items);   /* syntax error */
                    return NULL;
                }
            }
        } else {
            json_free(items);   /* truncated input → ValueError */
            return NULL;
        }
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\0') { json_free(items); return NULL; }   /* trailing junk */
    return items;
}

/* PoP: parse_config_string_list @ agent/skill_utils.py:parse_config_string_list
 * Normalize a config value that may hold a JSON-array string into a list.
 * A quoted-list string is parsed back into items (#86661); a plain scalar
 * string means one name (#13026). Returns a new JSON array of strings. */
json_t *parse_config_string_list(const json_t *value) {
    json_t *out = json_array();
    if (!value || value->type == JSON_NULL) return out;
    if (value->type == JSON_STRING) {
        const char *s = value->str_val ? value->str_val : "";
        const char *q = s;
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q == '[') {
            json_t *parsed = su_parse_literal_list(s);
            if (parsed) { json_free(out); return parsed; }
        }
        json_append(out, json_string(s));
        return out;
    }
    if (value->type == JSON_ARRAY) {   /* list/tuple/set/frozenset */
        size_t n = json_array_size(value);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(value, i);
            if (!item || item->type == JSON_NULL) {
                json_append(out, json_string("None"));
            } else if (item->type == JSON_STRING) {
                json_append(out, json_string(item->str_val ? item->str_val : ""));
            } else if (item->type == JSON_BOOL) {
                json_append(out, json_string(item->bool_val ? "True" : "False"));
            } else if (item->type == JSON_NUMBER) {
                double d = item->num_val;
                if (d == (double)(long)d) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%ld", (long)d);
                    json_append(out, json_string(buf));
                } else {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%g", d);
                    json_append(out, json_string(buf));
                }
            } else {
                json_append(out, json_string(""));
            }
        }
        return out;
    }
    return out;
}

/* PoP: find_project_root @ agent/skill_utils.py:find_project_root
 * Nearest ancestor containing ``.git`` (dir or file), walking up from
 * TERMINAL_CWD / *start* / cwd, capped at 64 levels. The home dir itself is
 * never a project. Caller frees; NULL when not inside a git checkout. */
char *find_project_root(const char *start) {
    char cur[PATH_MAX];
    if (start && *start) {
        snprintf(cur, sizeof(cur), "%s", start);
    } else {
        const char *env_cwd = getenv("TERMINAL_CWD");
        if (env_cwd && *env_cwd) {
            snprintf(cur, sizeof(cur), "%s", env_cwd);
        } else if (!getcwd(cur, sizeof(cur))) {
            return NULL;
        }
    }
    char *resolved = realpath(cur, NULL);
    if (!resolved) return NULL;
    char homebuf[PATH_MAX];
    const char *home = getenv("HOME");
    snprintf(homebuf, sizeof(homebuf), "%s", home ? home : "");
    char *home_resolved = homebuf[0] ? realpath(homebuf, NULL) : NULL;

    char *result = NULL;
    for (int depth = 0; depth < PROJECT_ROOT_MAX_DEPTH; depth++) {
        char gitpath[PATH_MAX * 2];
        if ((size_t)strlen(resolved) + 6 >= sizeof(gitpath)) break;
        snprintf(gitpath, sizeof(gitpath), "%s/.git", resolved);
        struct stat st;
        if (stat(gitpath, &st) == 0) {
            /* A git checkout AT the home dir is treated as non-project. */
            if (!home_resolved || strcmp(resolved, home_resolved) != 0)
                result = strdup(resolved);
            break;
        }
        char *parent = strdup(resolved);
        char *slash = strrchr(parent, '/');
        if (!slash) { free(parent); break; }
        if (slash == parent) {
            if (parent[1] == '\0') { free(parent); break; }   /* "/" */
            slash[1] = '\0';                                  /* "/x" → "/" */
        } else {
            *slash = '\0';
        }
        free(resolved);
        resolved = parent;
    }
    free(resolved);
    free(home_resolved);
    return result;
}

/* Resolved set of skills.trusted_project_dirs from config.yaml (set). */
static json_t *su_project_trusted_dirs_from_config(void) {
    json_t *result = json_array();
    json_t *parsed = su_load_raw_config_json();
    if (!parsed || parsed->type != JSON_OBJECT) { json_free(parsed); return result; }
    json_t *skills_cfg = json_obj_get(parsed, "skills");
    json_t *raw = (skills_cfg && skills_cfg->type == JSON_OBJECT)
                      ? json_obj_get(skills_cfg, "trusted_project_dirs")
                      : NULL;
    json_t *entries = NULL;
    if (raw && raw->type == JSON_STRING) {
        entries = json_array();
        json_append(entries, json_string(raw->str_val ? raw->str_val : ""));
    } else if (raw && raw->type == JSON_ARRAY) {
        entries = raw;
    }
    if (entries) {
        size_t n = json_array_size(entries);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_STRING || !e->str_val) continue;
            char entry[2048];
            snprintf(entry, sizeof(entry), "%s", e->str_val);
            char *s = entry;
            while (*s && isspace((unsigned char)*s)) s++;
            size_t len = strlen(s);
            while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
            if (!s[0]) continue;
            /* expanduser + expandvars, then resolve symlinks */
            char expanded[2048];
            if (s[0] == '~' && (s[1] == '/' || s[1] == '\0')) {
                const char *home = getenv("HOME");
                snprintf(expanded, sizeof(expanded), "%s%s", home ? home : "", s + 1);
            } else {
                snprintf(expanded, sizeof(expanded), "%s", s);
            }
            char *resolved = realpath(expanded, NULL);
            if (resolved) {
                bool dup = false;
                size_t have = json_array_size(result);
                for (size_t k = 0; k < have; k++) {
                    json_t *have_e = json_get(result, k);
                    if (have_e && have_e->type == JSON_STRING &&
                        strcmp(have_e->str_val, resolved) == 0) { dup = true; break; }
                }
                if (!dup) json_append(result, json_string(resolved));
                free(resolved);
            }
        }
    }
    json_free(parsed);
    return result;
}

/* PoP: is_project_root_trusted @ agent/skill_utils.py:is_project_root_trusted
 * True when *root* resolves to an entry of skills.trusted_project_dirs. */
bool is_project_root_trusted(const char *root) {
    if (!root || !*root) return false;
    char *resolved = realpath(root, NULL);
    if (!resolved) return false;
    json_t *trusted = su_project_trusted_dirs_from_config();
    bool ok = false;
    size_t n = json_array_size(trusted);
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(trusted, i);
        if (t && t->type == JSON_STRING && strcmp(t->str_val, resolved) == 0) {
            ok = true;
            break;
        }
    }
    json_free(trusted);
    free(resolved);
    return ok;
}

/* Existing skill dirs under *root*, excluding the profile's own skills dir.
 * Paths are symlink-resolved like Path.resolve(). Caller frees. */
static json_t *su_candidate_project_skills_dirs(const char *root) {
    json_t *dirs = json_array();
    char local_skills[PATH_MAX];
    su_local_skills_dir(local_skills, sizeof(local_skills));
    char *local_resolved = local_skills[0] ? realpath(local_skills, NULL) : NULL;
    for (int i = 0; s_project_skills_subdirs[i]; i++) {
        char cand[PATH_MAX * 2];
        snprintf(cand, sizeof(cand), "%s/%s", root, s_project_skills_subdirs[i]);
        struct stat st;
        if (stat(cand, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        char *resolved = realpath(cand, NULL);
        if (!resolved) continue;
        if (local_resolved && strcmp(resolved, local_resolved) == 0) {
            free(resolved);
            continue;
        }
        bool dup = false;
        size_t have = json_array_size(dirs);
        for (size_t k = 0; k < have; k++) {
            json_t *prev = json_get(dirs, k);
            if (prev && prev->type == JSON_STRING && strcmp(prev->str_val, resolved) == 0)
                dup = true;
        }
        if (!dup) json_append(dirs, json_string(resolved));
        free(resolved);
    }
    free(local_resolved);
    return dirs;
}

/* PoP: get_skills_dir @ agent/skill_utils.py:get_skills_dir (helper reuse).
 * The primary profile-local skills dir. Caller frees. */
static char *su_get_skills_dir(void) {
    char local[PATH_MAX];
    su_local_skills_dir(local, sizeof(local));
    return strdup(local);
}

/* PoP: get_external_skills_dirs @ agent/skill_utils.py:get_external_skills_dirs
 * Validated, deduplicated, existing entries from skills.external_dirs,
 * skipping paths resolving to the local skills dir. Caller frees. */
static json_t *su_get_external_skills_dirs(void) {
    json_t *dirs = json_array();
    char local_skills[PATH_MAX];
    su_local_skills_dir(local_skills, sizeof(local_skills));

    const char *config_path = getenv("HERMES_CONFIG_PATH");
    char cfgbuf[1024], homebuf[1024];
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (h && *h) { snprintf(homebuf, sizeof(homebuf), "%s/.hermes", h); home = homebuf; }
        else home = "";
    }
    if (!config_path || !*config_path) {
        snprintf(cfgbuf, sizeof(cfgbuf), "%s/config.yaml", home);
        config_path = cfgbuf;
    }

    size_t count = 0;
    char **ext = skill_get_external_dirs(config_path, home, local_skills, &count);
    for (size_t i = 0; i < count; i++) {
        char *resolved = realpath(ext[i], NULL);
        free(ext[i]);
        if (!resolved) continue;
        if (local_skills[0]) {
            char *local_resolved = realpath(local_skills, NULL);
            if (local_resolved && strcmp(resolved, local_resolved) == 0) {
                free(local_resolved);
                free(resolved);
                continue;
            }
            free(local_resolved);
        }
        bool dup = false;
        size_t have = json_array_size(dirs);
        for (size_t k = 0; k < have; k++) {
            json_t *prev = json_get(dirs, k);
            if (prev && prev->type == JSON_STRING && strcmp(prev->str_val, resolved) == 0)
                dup = true;
        }
        if (!dup) json_append(dirs, json_string(resolved));
        free(resolved);
    }
    free(ext);
    return dirs;
}

/* PoP: get_project_skills_dirs @ agent/skill_utils.py:get_project_skills_dirs
 * Trusted project-local skill dirs for the current cwd ([]-equivalent when
 * not in a trusted git checkout or discovery is disabled). Caller frees. */
json_t *get_project_skills_dirs(void) {
    json_t *dirs = json_array();

    json_t *parsed = su_load_raw_config_json();
    if (parsed && parsed->type == JSON_OBJECT) {
        json_t *skills_cfg = json_obj_get(parsed, "skills");
        if (skills_cfg && skills_cfg->type == JSON_OBJECT) {
            json_t *pd = json_obj_get(skills_cfg, "project_discovery");
            if (pd && pd->type == JSON_BOOL && pd->bool_val == false) {
                json_free(parsed);
                return dirs;
            }
        }
    }
    json_free(parsed);

    char *root = find_project_root(NULL);
    if (!root) return dirs;
    if (!is_project_root_trusted(root)) { free(root); return dirs; }
    json_free(dirs);
    dirs = su_candidate_project_skills_dirs(root);
    free(root);
    return dirs;
}

/* PoP: get_untrusted_project_skills_root @ agent/skill_utils.py:get_untrusted_project_skills_root
 * When cwd's project has skills but is NOT trusted: (root, skill_count).
 * Returns false when there is nothing to notify about; on true, *out_root
 * is malloc'd (caller frees) and *out_count is set. */
bool get_untrusted_project_skills_root(char **out_root, int *out_count) {
    json_t *parsed = su_load_raw_config_json();
    if (parsed && parsed->type == JSON_OBJECT) {
        json_t *skills_cfg = json_obj_get(parsed, "skills");
        if (skills_cfg && skills_cfg->type == JSON_OBJECT) {
            json_t *pd = json_obj_get(skills_cfg, "project_discovery");
            if (pd && pd->type == JSON_BOOL && pd->bool_val == false) {
                json_free(parsed);
                return false;
            }
        }
    }
    json_free(parsed);

    char *root = find_project_root(NULL);
    if (!root) return false;
    if (is_project_root_trusted(root)) { free(root); return false; }

    int count = 0;
    json_t *dirs = su_candidate_project_skills_dirs(root);
    size_t n = json_array_size(dirs);
    for (size_t i = 0; i < n; i++) {
        json_t *d = json_get(dirs, i);
        if (!d || d->type != JSON_STRING) continue;
        json_t *files = sku_iter_skill_index_files(d->str_val, "SKILL.md");
        count += (int)json_array_size(files);
        json_free(files);
    }
    json_free(dirs);
    if (count == 0) { free(root); return false; }
    if (out_root) *out_root = root; else free(root);
    if (out_count) *out_count = count;
    return true;
}

/* PoP: get_scan_ordered_skills_dirs @ agent/skill_utils.py:get_scan_ordered_skills_dirs
 * All skill dirs in precedence order: project → local → external.
 * First-wins name dedup over this order gives project skills priority.
 * Caller frees. */
json_t *get_scan_ordered_skills_dirs(void) {
    json_t *dirs = get_project_skills_dirs();
    char *local = su_get_skills_dir();
    if (local) json_append(dirs, json_string(local));
    free(local);
    json_t *external = su_get_external_skills_dirs();
    size_t n = json_array_size(external);
    for (size_t i = 0; i < n; i++) {
        json_t *d = json_get(external, i);
        if (d && d->type == JSON_STRING) json_append(dirs, json_string(d->str_val));
    }
    json_free(external);
    return dirs;
}

/* read_active_org_id: trimmed contents of <skills_dir>/.active_org, or NULL. */
static char *su_read_active_org_id(const char *skills_dir) {
    char path[PATH_MAX * 2];
    if ((size_t)strlen(skills_dir) + 16 >= sizeof(path)) return NULL;
    snprintf(path, sizeof(path), "%s/%s", skills_dir, ORG_ACTIVE_MARKER);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    if (!s[0]) return NULL;
    return strdup(s);
}

/* Recursive os.walk(followlinks=True) core for iter_skill_index_files. */
static void su_walk_index_files(const char *root_dir, const char *skills_dir,
                                const char *active_org, const char *filename,
                                json_t *matches, int depth) {
    if (depth > 32) return;   /* guard against symlink loops */
    DIR *dir = opendir(root_dir);
    if (!dir) return;
    bool has_skill_md = false;
    char (*subdirs)[256] = NULL;
    int subdir_cap = 64, subdir_n = 0;
    subdirs = malloc((size_t)subdir_cap * sizeof(*subdirs));
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        char full[PATH_MAX * 2];
        if ((size_t)strlen(root_dir) + strlen(name) + 2 >= sizeof(full)) continue;
        snprintf(full, sizeof(full), "%s/%s", root_dir, name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (subdir_n == subdir_cap) {
                subdir_cap *= 2;
                char (*tmp)[256] = realloc(subdirs, (size_t)subdir_cap * sizeof(*subdirs));
                if (!tmp) break;
                subdirs = tmp;
            }
            snprintf(subdirs[subdir_n++], 256, "%s", name);
        } else if (S_ISREG(st.st_mode) && strcmp(name, filename) == 0) {
            has_skill_md = true;   /* matches are collected below via full path */
        }
    }
    closedir(dir);

    /* Python: if filename in files: matches.append(join(root, filename)). */
    char match_path[PATH_MAX * 2];
    snprintf(match_path, sizeof(match_path), "%s/%s", root_dir, filename);
    struct stat mst;
    bool listed = has_skill_md || (stat(match_path, &mst) == 0 && S_ISREG(mst.st_mode));
    if (listed) {
        /* dedupe (has_skill_md implies stat already confirmed) */
        bool dup = false;
        size_t have = json_array_size(matches);
        for (size_t k = 0; k < have; k++) {
            json_t *prev = json_get(matches, k);
            if (prev && prev->type == JSON_STRING && strcmp(prev->str_val, match_path) == 0)
                dup = true;
        }
        if (!dup) json_append(matches, json_string(match_path));
    }

    bool at_org_root = strcmp(root_dir, skills_dir) == 0
                           ? false
                           : (strncmp(root_dir, skills_dir, strlen(skills_dir)) == 0 &&
                              strstr(root_dir, "/" ORG_MIRROR_DIR_NAME) != NULL &&
                              strcmp(strrchr(root_dir, '/'), "/" ORG_MIRROR_DIR_NAME) == 0);

    for (int i = 0; i < subdir_n; i++) {
        const char *name = subdirs[i];
        bool exclude = su_str_in_set(name, s_excluded_skill_dirs);
        if (!exclude && has_skill_md && su_str_in_set(name, s_skill_support_dirs))
            exclude = true;   /* support dirs under a skill are not skill roots */
        if (!exclude && strcmp(name, ORG_MIRROR_DIR_NAME) == 0 && !active_org)
            exclude = true;   /* no active org marker ⇒ no org skills load */
        if (!exclude && at_org_root && (!active_org || strcmp(name, active_org) != 0))
            exclude = true;   /* inside _org/: descend ONLY into the active org */
        if (exclude) continue;
        char full[PATH_MAX * 2];
        snprintf(full, sizeof(full), "%s/%s", root_dir, name);
        su_walk_index_files(full, skills_dir, active_org, filename, matches, depth + 1);
    }
    free(subdirs);
}

static int su_strcmp_qsort(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    return strcmp(ja->str_val ? ja->str_val : "", jb->str_val ? jb->str_val : "");
}

/* PoP: sku_iter_skill_index_files @ agent/skill_utils.py:iter_skill_index_files
 * Walk skills_dir yielding sorted paths matching *filename*. Excludes Hermes
 * metadata, VCS, virtualenv/dependency, cache, and skill support dirs; org
 * mirrors (_org/) resolve only the active org's subdir. Caller frees. */
json_t *sku_iter_skill_index_files(const char *skills_dir, const char *filename) {
    json_t *matches = json_array();
    if (!skills_dir || !*skills_dir || !filename) return matches;
    char *active_org = su_read_active_org_id(skills_dir);
    size_t root_len = strlen(skills_dir);
    char org_root[PATH_MAX];
    snprintf(org_root, sizeof(org_root), "%s/" ORG_MIRROR_DIR_NAME, skills_dir);
    /* Python prunes _org at the top level when no marker; deeper gating is
     * handled inside su_walk_index_files. */
    (void)root_len;
    su_walk_index_files(skills_dir, skills_dir, active_org, filename, matches, 0);
    free(active_org);

    /* sorted(matches) */
    size_t n = json_array_size(matches);
    if (n > 1) {
        json_t **items = malloc(n * sizeof(*items));
        for (size_t i = 0; i < n; i++) items[i] = json_get(matches, i);
        qsort(items, n, sizeof(*items), su_strcmp_qsort);
        json_t *sorted_matches = json_array();
        for (size_t i = 0; i < n; i++)
            json_append(sorted_matches, json_string(items[i]->str_val));
        free(items);
        json_free(matches);
        matches = sorted_matches;
    }
    return matches;
}

/* Per-process quarantine cache keyed by resolved skill dir
 * (agent/skill_utils.py:_PROJECT_QUARANTINE_CACHE). */
#define SU_QUARANTINE_CACHE_CAP 256
static struct {
    char *key;
    bool quarantined;
} s_quarantine_cache[SU_QUARANTINE_CACHE_CAP];
static int s_quarantine_cache_n = 0;

/* PoP: is_quarantined_project_skill @ agent/skill_utils.py:is_quarantined_project_skill
 * True when a project skill's scan verdict is ``dangerous``. Fail-closed:
 * a scanner crash or missing scanner quarantines the skill. */
bool is_quarantined_project_skill(const char *skill_md) {
    if (!skill_md || !*skill_md) return true;
    char key[PATH_MAX * 2];
    char *resolved = realpath(skill_md, NULL);
    snprintf(key, sizeof(key), "%s", resolved ? resolved : skill_md);
    free(resolved);
    /* key = str(Path(skill_md).parent) — drop the last path component. */
    char *slash = strrchr(key, '/');
    if (slash && slash != key) *slash = '\0';

    for (int i = 0; i < s_quarantine_cache_n; i++)
        if (strcmp(s_quarantine_cache[i].key, key) == 0)
            return s_quarantine_cache[i].quarantined;

    /* cache dir: HERMES_HOME/cache/project_skill_scans */
    char cache_dir[PATH_MAX];
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) home = "";
    snprintf(cache_dir, sizeof(cache_dir), "%s/.hermes/cache/project_skill_scans",
             strcmp(home, "") == 0 ? "" : home);

    skills_guard_scan_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = skills_guard_scan_skill_cached(skill_md, PROJECT_SCAN_SOURCE, NULL,
                                            cache_dir, &result);
    bool quarantined;
    if (rc == 0) {
        quarantined = strcmp(result.verdict, "dangerous") == 0;
        if (quarantined)
            hermes_log(LOG_WARNING, "skill_utils",
                       "Project skill quarantined (verdict=dangerous): %s — %s",
                       key, result.summary);
    } else {
        hermes_log(LOG_WARNING, "skill_utils",
                   "Project skill scan failed — quarantining (fail closed): %s", key);
        quarantined = true;
    }

    if (s_quarantine_cache_n < SU_QUARANTINE_CACHE_CAP) {
        s_quarantine_cache[s_quarantine_cache_n].key = strdup(key);
        s_quarantine_cache[s_quarantine_cache_n].quarantined = quarantined;
        s_quarantine_cache_n++;
    }
    return quarantined;
}

/* PoP: iter_project_skill_files @ agent/skill_utils.py:iter_project_skill_files
 * Yield non-quarantined SKILL.md files under a trusted project dir. The
 * single iteration chokepoint so the quarantine cannot be bypassed.
 * Caller frees. */
json_t *iter_project_skill_files(const char *project_dir) {
    json_t *out = json_array();
    if (!project_dir || !*project_dir) return out;
    json_t *all = sku_iter_skill_index_files(project_dir, "SKILL.md");
    size_t n = json_array_size(all);
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(all, i);
        if (!p || p->type != JSON_STRING) continue;
        if (is_quarantined_project_skill(p->str_val)) continue;
        json_append(out, json_string(p->str_val));
    }
    json_free(all);
    return out;
}
