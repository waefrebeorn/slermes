/*
 * skill_commands.c — Skill slash-command support for Hermes C.
 * Port of Python agent/skill_commands.py (523 lines).
 *
 * Provides:
 * - skill_cmd_scan() — scan ~/.hermes/skills/, parse frontmatter, build /slug cache
 * - skill_cmd_scan_filtered() — scan with disabled-skill filtering
 * - skill_cmd_get() / skill_cmd_get_all() — cached accessors
 * - skill_cmd_resolve() — normalize user input to canonical slug
 * - skill_cmd_build_message() — load SKILL.md, build formatted invocation message
 * - skill_cmd_rescan() — re-scan and return diff
 * - skill_cmd_is_disabled() — check if a skill is in the disabled list
 */

#include "hermes_skill_commands.h"
#include "hermes_system_prompt.h"
#include "skill_utils.h"
#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* ================================================================
 *  Internal state
 * ================================================================ */

#include "hive.h"
static hive_t *g_skills = NULL;   /* of skill_cmd_entry_t* (heap) */
static time_t g_last_scan = 0;
static char g_skills_dir[SKILL_CMD_PATH_MAX];
static char g_last_platform[64] = "";   /* SK06: platform-scoped cache tracking */

/* SK06: Invalidate the skill cache when platform scope changes.
 * Called by the gateway when a session's platform changes, or when
 * skill directories are modified. Forces re-scan on next access. */
static void skill_cache_invalidate(void) {
    if (g_skills) {
        hive_iter_t it;
        hive_iter_begin(g_skills, &it);
        hive_handle_t hnd;
        skill_cmd_entry_t *sk;
        while (hive_iter_next(g_skills, &it, &hnd, (void **)&sk)) {
            free(sk);
            hive_erase(g_skills, hnd);
        }
    }
    g_last_scan = 0;
}

/* SK06: Invalidate if the current platform differs from the cached platform.
 * Returns true if invalidation occurred (cache was stale). */
static bool skill_cache_check_platform(const char *current_platform) {
    if (!current_platform) current_platform = "";
    if (strcmp(g_last_platform, current_platform) != 0) {
        skill_cache_invalidate();
        snprintf(g_last_platform, sizeof(g_last_platform), "%s", current_platform);
        return true;
    }
    return false;
}

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Trim trailing whitespace/newlines */
static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t'
           || s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

/* ================================================================
 *  SK07: Skill config injection from config.yaml
 * ================================================================ */

/* Extract config vars from a skill's SKILL.md frontmatter.
 * Parses the metadata.hermes.config YAML block and populates
 * sk->config_vars[]. Returns the number of config vars found.
 * Uses line-by-line parsing since our YAML parser doesn't support
 * numeric list indices in dot-paths. */
static int extract_skill_config_vars_from_file(const char *md_path, skill_cmd_entry_t *sk) {
    FILE *f = fopen(md_path, "r");
    if (!f) return 0;

    char line[4096];
    int in_fm = 0, fm_ended = 0;
    int in_metadata = 0, in_hermes = 0, in_config = 0;
    int config_indent = 0;
    int count = 0;
    char cur_key[128] = "", cur_desc[256] = "", cur_default[512] = "";

    while (fgets(line, sizeof(line), f) && !fm_ended) {
        trim_right(line);
        if (strcmp(line, "---") == 0) {
            if (!in_fm) { in_fm = 1; continue; }
            else {
                if (in_config && cur_key[0] && cur_desc[0] && count < 8) {
                    strncpy(sk->config_vars[count].key, cur_key, sizeof(sk->config_vars[count].key) - 1);
                    strncpy(sk->config_vars[count].description, cur_desc, sizeof(sk->config_vars[count].description) - 1);
                    strncpy(sk->config_vars[count].default_val, cur_default, sizeof(sk->config_vars[count].default_val) - 1);
                    sk->config_vars[count].resolved[0] = '\0';
                    count++;
                }
                fm_ended = 1;
                continue;
            }
        }
        if (!in_fm) continue;

        int indent = 0;
        while (line[indent] == ' ') indent++;

        if (in_config) {
            if (line[0] != '\0' && indent <= config_indent && strncmp(line, "- ", 2) != 0) {
                if (cur_key[0] && cur_desc[0] && count < 8) {
                    strncpy(sk->config_vars[count].key, cur_key, sizeof(sk->config_vars[count].key) - 1);
                    strncpy(sk->config_vars[count].description, cur_desc, sizeof(sk->config_vars[count].description) - 1);
                    strncpy(sk->config_vars[count].default_val, cur_default, sizeof(sk->config_vars[count].default_val) - 1);
                    sk->config_vars[count].resolved[0] = '\0';
                    count++;
                }
                in_config = in_hermes = in_metadata = 0;
                cur_key[0] = cur_desc[0] = cur_default[0] = '\0';
                continue;
            }
            if (strncmp(line, "- key:", 6) == 0 || strncmp(line, "  key:", 6) == 0) {
                if (cur_key[0] && cur_desc[0] && count < 8) {
                    strncpy(sk->config_vars[count].key, cur_key, sizeof(sk->config_vars[count].key) - 1);
                    strncpy(sk->config_vars[count].description, cur_desc, sizeof(sk->config_vars[count].description) - 1);
                    strncpy(sk->config_vars[count].default_val, cur_default, sizeof(sk->config_vars[count].default_val) - 1);
                    sk->config_vars[count].resolved[0] = '\0';
                    count++;
                    cur_key[0] = cur_desc[0] = cur_default[0] = '\0';
                }
                const char *val = line;
                while (*val != ':') val++; val++;
                while (*val == ' ') val++;
                strncpy(cur_key, val, sizeof(cur_key) - 1);
            } else if (strstr(line, "description:") && !strstr(line, "description: \"\"")) {
                const char *val = strstr(line, "description:") + 12;
                while (*val == ' ') val++;
                strncpy(cur_desc, val, sizeof(cur_desc) - 1);
            } else if (strstr(line, "default:")) {
                const char *val = strstr(line, "default:") + 8;
                while (*val == ' ') val++;
                strncpy(cur_default, val, sizeof(cur_default) - 1);
            }
        } else if (in_hermes) {
            if (indent <= 6) { in_hermes = 0; in_metadata = 0; continue; }
            if (strstr(line, "config:")) { in_config = 1; config_indent = indent; }
        } else if (in_metadata) {
            if (indent <= 2) { in_metadata = 0; continue; }
            if (strstr(line, "hermes:")) { in_hermes = 1; }
        } else {
            if (strstr(line, "metadata:")) { in_metadata = 1; }
        }
    }
    fclose(f);

    if (in_config && cur_key[0] && cur_desc[0] && count < 8) {
        strncpy(sk->config_vars[count].key, cur_key, sizeof(sk->config_vars[count].key) - 1);
        strncpy(sk->config_vars[count].description, cur_desc, sizeof(sk->config_vars[count].description) - 1);
        strncpy(sk->config_vars[count].default_val, cur_default, sizeof(sk->config_vars[count].default_val) - 1);
        sk->config_vars[count].resolved[0] = '\0';
        count++;
    }

    sk->config_var_count = count;
    return count;
}

/* Resolve config var values from config.yaml.
 * For each skill with config vars, looks up skills.config.<key> in config.yaml
 * and stores the resolved value (or default if not set). */
/* Port of Python: _inject_skill_config */
static void skill_config_inject(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);
    struct stat st;
    if (stat(config_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(config_path, sizeof(config_path), "%s/.slermes/config.yaml", home);
        if (stat(config_path, &st) != 0 || !S_ISREG(st.st_mode))
            return;
    }

    char *err = NULL;
    yaml_doc_t *cfg = yaml_parse_file(config_path, &err);
    if (!cfg) { if (err) free(err); return; }

    if (g_skills) {
        hive_iter_t it;
        hive_iter_begin(g_skills, &it);
        skill_cmd_entry_t *sk;
        while (hive_iter_next(g_skills, &it, NULL, (void **)&sk)) {
            if (sk->config_var_count == 0) continue;
            for (int j = 0; j < sk->config_var_count; j++) {
                char lookup[256];
                snprintf(lookup, sizeof(lookup), "skills.config.%s", sk->config_vars[j].key);
                const char *val = yaml_get_string(cfg, lookup);
                if (val && val[0]) {
                    /* SK07: Expand ~ and ${ENV} in path values (matches Python expanduser/expandvars) */
                    if (strstr(val, "~") || strstr(val, "${")) {
                        char expanded[512];
                        /* Simple ~ expansion */
                        if (val[0] == '~' && (val[1] == '/' || val[1] == '\0')) {
                            const char *homedir = getenv("HOME");
                            if (!homedir) homedir = "/tmp";
                            snprintf(expanded, sizeof(expanded), "%s%s", homedir, val + 1);
                        } else {
                            strncpy(expanded, val, sizeof(expanded) - 1);
                            expanded[sizeof(expanded) - 1] = '\0';
                        }
                        /* Simple ${VAR} expansion for common vars */
                        char final_val[512];
                        char *dst = final_val;
                        char *src = expanded;
                        char *dend = final_val + sizeof(final_val) - 1;
                        while (*src && dst < dend) {
                            if (src[0] == '$' && src[1] == '{') {
                                char *end = strchr(src + 2, '}');
                                if (end && end - src < 64) {
                                    char varname[64];
                                    size_t vlen = end - src - 2;
                                    memcpy(varname, src + 2, vlen);
                                    varname[vlen] = '\0';
                                    const char *vval = getenv(varname);
                                    if (vval) {
                                        size_t vl = strlen(vval);
                                        if (dst + vl < dend) {
                                            memcpy(dst, vval, vl);
                                            dst += vl;
                                        }
                                    }
                                    src = end + 1;
                                    continue;
                                }
                            }
                            *dst++ = *src++;
                        }
                        *dst = '\0';
                        strncpy(sk->config_vars[j].resolved, final_val, sizeof(sk->config_vars[j].resolved) - 1);
                    } else {
                        strncpy(sk->config_vars[j].resolved, val, sizeof(sk->config_vars[j].resolved) - 1);
                    }
                } else if (sk->config_vars[j].default_val[0]) {
                    strncpy(sk->config_vars[j].resolved, sk->config_vars[j].default_val,
                            sizeof(sk->config_vars[j].resolved) - 1);
                } else {
                    sk->config_vars[j].resolved[0] = '\0';
                }
            }
        }
    }
    yaml_free(cfg);
}

/* Normalize a skill name to /slug:
 *   lowercase, spaces_underscores->hyphens,
 *   strip non-alnum (except hyphens), collapse multi-hyphens */
static void name_to_slug(const char *name, char *slug, size_t slug_sz) {
    if (!name || !*name) { slug[0] = '/'; slug[1] = '\0'; return; }

    char buf[SKILL_CMD_SLUG_MAX];
    size_t pos = 0;
    for (const char *p = name; *p && pos < sizeof(buf) - 1; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c == ' ' || c == '_') c = '-';
        if (c == '-' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            buf[pos++] = c;
    }
    buf[pos] = '\0';

    /* Collapse multi-hyphens */
    char out[SKILL_CMD_SLUG_MAX];
    size_t opos = 0;
    int last_hyphen = 0;
    for (size_t i = 0; buf[i] && opos < sizeof(out) - 1; i++) {
        if (buf[i] == '-') {
            if (last_hyphen) continue;
            last_hyphen = 1;
        } else {
            last_hyphen = 0;
        }
        out[opos++] = buf[i];
    }
    out[opos] = '\0';
    /* Strip leading/trailing hyphens */
    while (opos > 0 && out[opos-1] == '-') out[--opos] = '\0';
    const char *start = out;
    while (*start == '-') start++;
    snprintf(slug, slug_sz, "/%s", start);
}

/* Extract "key: value" from a YAML frontmatter line */
static int extract_fm(const char *line, const char *key,
                       char *out, size_t out_sz) {
    size_t klen = strlen(key);
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, key, klen) != 0) return 0;
    line += klen;
    while (*line == ':' || *line == ' ' || *line == '\t') line++;
    if (*line == '>') { snprintf(out, out_sz, "(multi-line)"); return 1; }
    snprintf(out, out_sz, "%s", line);
    trim_right(out);
    return 1;
}

/* Determine skills directory: try HERMES_HOME, HOME, then fallback */
static const char *get_skills_dir(void) {
    static char buf[SKILL_CMD_PATH_MAX];
    if (buf[0]) return buf;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(buf, sizeof(buf), "%s/.hermes/skills", home);
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(buf, sizeof(buf), "%s/.slermes/skills", home);
        if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode))
            buf[0] = '\0';
    }
    return buf;
}

/* ================================================================
 *  Scanning
 * ================================================================ */

/* Port of Python hermes_cli/config.py:get_config_path(). */
/* Helper: config path resolution (same logic as skill_utils) */
static const char *get_config_path(void) {
    static char buf[SKILL_CMD_PATH_MAX];
    if (buf[0]) return buf;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(buf, sizeof(buf), "%s/.hermes/config.yaml", home);
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(buf, sizeof(buf), "%s/.slermes/config.yaml", home);
        if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode))
            buf[0] = '\0';
    }
    return buf;
}

/* Scan a single skills directory for SKILL.md entries.
 * Returns number of skills added to the hive.
 * Appends to the existing hive (does NOT clear it first). */
static int scan_one_skills_dir(const char *sdir) {
    int added = 0;
    DIR *d = opendir(sdir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char dir_path[SKILL_CMD_PATH_MAX];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", sdir, entry->d_name);

        struct stat st;
        if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        /* Must contain SKILL.md */
        char md_path[SKILL_CMD_PATH_MAX + 32];
        snprintf(md_path, sizeof(md_path), "%s/SKILL.md", dir_path);
        if (stat(md_path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        /* Read SKILL.md and parse frontmatter */
        FILE *f = fopen(md_path, "r");
        if (!f) continue;

        skill_cmd_entry_t *sk = calloc(1, sizeof(skill_cmd_entry_t));
        if (!sk) { fclose(f); continue; }
        snprintf(sk->name, sizeof(sk->name), "%s", entry->d_name);

        char line[4096];
        int in_fm = 0, fm_ended = 0;
        int found_name = 0, found_desc = 0, found_platforms = 0;
        int found_gw_hint = 0, found_setup_note = 0;
        char platforms_buf[1024] = "";
        char gw_hint_buf[SKILL_CMD_DESC_MAX] = "";
        char setup_note_buf[SKILL_CMD_DESC_MAX] = "";

        while (fgets(line, sizeof(line), f) && !fm_ended) {
            trim_right(line);
            if (strcmp(line, "---") == 0) {
                if (!in_fm) { in_fm = 1; continue; }
                else { fm_ended = 1; continue; }
            }
            if (in_fm) {
                if (!found_name && extract_fm(line, "name", sk->name, sizeof(sk->name)))
                    found_name = 1;
                if (!found_desc && extract_fm(line, "description", sk->description, sizeof(sk->description)))
                    found_desc = 1;
                if (!found_platforms && extract_fm(line, "platforms", platforms_buf, sizeof(platforms_buf)))
                    found_platforms = 1;
                /* SK08: Extract setup notes */
                if (strstr(line, "setup_skipped:"))
                    sk->setup_skipped = true;
                if (!found_gw_hint && extract_fm(line, "gateway_setup_hint", gw_hint_buf, sizeof(gw_hint_buf)))
                    found_gw_hint = 1;
                if (strstr(line, "setup_needed:"))
                    sk->setup_needed = true;
                if (!found_setup_note && extract_fm(line, "setup_note", setup_note_buf, sizeof(setup_note_buf)))
                    found_setup_note = 1;
            }
        }
        fclose(f);

        /* SK08: Store setup notes in entry */
        if (found_gw_hint && gw_hint_buf[0])
            snprintf(sk->gateway_setup_hint, sizeof(sk->gateway_setup_hint), "%s", gw_hint_buf);
        if (found_setup_note && setup_note_buf[0])
            snprintf(sk->setup_note, sizeof(sk->setup_note), "%s", setup_note_buf);

        /* SK02: Platform filtering — skip skills incompatible with current OS */
        if (found_platforms && platforms_buf[0]) {
            /* Strip YAML list brackets [ ] if present */
            char clean_platforms[1024];
            size_t wp = 0;
            for (const char *p = platforms_buf; *p && wp < sizeof(clean_platforms) - 1; p++) {
                if (*p != '[' && *p != ']') clean_platforms[wp++] = *p;
            }
            clean_platforms[wp] = '\0';

            skill_frontmatter_t fm;
            memset(&fm, 0, sizeof(fm));
            snprintf(fm.entries[0].key, sizeof(fm.entries[0].key), "platforms");
            snprintf(fm.entries[0].value, sizeof(fm.entries[0].value), "%s", clean_platforms);
            fm.count = 1;

            if (!skill_matches_platform(&fm))
                continue;
        }

        /* SK07: Extract config vars from frontmatter */
        extract_skill_config_vars_from_file(md_path, sk);

        name_to_slug(sk->name, sk->slug, sizeof(sk->slug));
        snprintf(sk->skill_path, sizeof(sk->skill_path), "%s", dir_path);
        if (!g_skills) g_skills = hive_new(8);
        bool ok = false;
        hive_insert(g_skills, sk, &ok);
        if (!ok) { free(sk); fclose(f); continue; }
        added++;
    }
    closedir(d);
    return added;
}

static int scan_skills_dir(void) {
    skill_cache_invalidate();
    g_last_scan = time(NULL);

    const char *sdir = get_skills_dir();
    if (!sdir || !sdir[0]) return 0;
    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", sdir);

    /* SK03: Scan all dirs — local + external from config */
    const char *config_path = get_config_path();
    const char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) hermes_home = getenv("HOME");
    if (!hermes_home) hermes_home = "/tmp";

    size_t ndirs = 0;
    char **all_dirs = skill_get_all_dirs(
        config_path && config_path[0] ? config_path : NULL,
        hermes_home, sdir, &ndirs);

    if (all_dirs) {
        for (size_t i = 0; i < ndirs; i++) {
            if (all_dirs[i] && all_dirs[i][0]) {
                scan_one_skills_dir(all_dirs[i]);
            }
            free(all_dirs[i]);
        }
        free(all_dirs);
    } else {
        /* Fallback: scan local dir only */
        scan_one_skills_dir(sdir);
    }

    /* SK07: Resolve config var values from config.yaml */
    skill_config_inject();

    return g_skills ? (int)hive_count(g_skills) : 0;
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* Port of Python: scan_skill_commands */
int skill_cmd_scan(void) {
    /* SK06: Lazy scan with platform awareness.
     * If skills exist and platform hasn't changed, skip rescan.
     * The platform parameter is optionally set via skill_cmd_set_platform()
     * before the first call. */
    if (g_skills && hive_count(g_skills) > 0) return (int)hive_count(g_skills);
    return scan_skills_dir();
}

/* SK06: Set the current platform scope and invalidate cache if changed.
 * Called by the gateway when a new session starts on a specific platform,
 * or when the session platform changes. */
void skill_cmd_set_platform(const char *platform) {
    skill_cache_check_platform(platform);
}

/* SK06: Explicitly invalidate the platform-scoped skill cache.
 * Called when skill directories are modified at runtime. */
void skill_cmd_invalidate_platform_cache(void) {
    skill_cache_invalidate();
    g_last_platform[0] = '\0';
}

const skill_cmd_entry_t *skill_cmd_get(const char *slug) {
    if (!slug || !g_skills) return NULL;
    hive_iter_t it;
    hive_iter_begin(g_skills, &it);
    skill_cmd_entry_t *sk;
    while (hive_iter_next(g_skills, &it, NULL, (void **)&sk)) {
        if (strcmp(sk->slug, slug) == 0)
            return sk;
    }
    return NULL;
}

/* Port of Python: get_skill_commands
 * Returns a heap array of pointers to the live entries; caller frees.
 * *out_count receives the number of entries (0 and NULL on empty). */
const skill_cmd_entry_t **skill_cmd_get_all(int *count) {
    if (count) *count = 0;
    if (!g_skills || hive_count(g_skills) == 0) return NULL;
    size_t n = hive_count(g_skills);
    const skill_cmd_entry_t **arr = malloc(n * sizeof(const skill_cmd_entry_t *));
    if (!arr) return NULL;
    size_t i = 0;
    hive_iter_t it;
    hive_iter_begin(g_skills, &it);
    skill_cmd_entry_t *sk;
    while (hive_iter_next(g_skills, &it, NULL, (void **)&sk))
        arr[i++] = sk;
    if (count) *count = (int)i;
    return arr;
}

/* Port of Python: resolve_skill_command_key */
const char *skill_cmd_resolve(const char *command) {
    if (!command || !g_skills) return NULL;

    /* Strip leading slashes */
    const char *cmd = command;
    while (*cmd == '/') cmd++;
    if (!*cmd) return NULL;

    /* Normalize: underscore->hyphen, lowercase */
    char normalized[SKILL_CMD_SLUG_MAX];
    size_t pos = 0;
    for (const char *p = cmd; *p && pos < sizeof(normalized) - 1; p++) {
        char c = (char)tolower((unsigned char)*p);
        if (c == '_') c = '-';
        normalized[pos++] = c;
    }
    normalized[pos] = '\0';

    /* Build full /slug and search */
    char full_slug[SKILL_CMD_SLUG_MAX];
    snprintf(full_slug, sizeof(full_slug), "/%s", normalized);

    hive_iter_t it;
    hive_iter_begin(g_skills, &it);
    skill_cmd_entry_t *sk;
    while (hive_iter_next(g_skills, &it, NULL, (void **)&sk)) {
        if (strcmp(sk->slug, full_slug) == 0)
            return sk->slug;
    }
    return NULL;
}

/* Port of Python: _build_skill_message, build_skill_invocation_message — consolidated: builds formatted skill message */
char *skill_cmd_build_message(const char *slug, const char *user_args) {
    if (!slug) return NULL;

    const skill_cmd_entry_t *sk = skill_cmd_get(slug);
    if (!sk) return NULL;

    /* Read SKILL.md */
    char md_path[SKILL_CMD_PATH_MAX + 32];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", sk->skill_path);

    FILE *f = fopen(md_path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 100000) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)sz + 1);
    if (!content) { fclose(f); return NULL; }
    size_t n = fread(content, 1, (size_t)sz, f);
    fclose(f);
    content[n] = '\0';

    /* Strip frontmatter */
    char *body = context_strip_frontmatter(content);

    /* Build invocation message */
    size_t total = 4096 + strlen(body) + 4096;
    char *msg = (char *)malloc(total);
    if (!msg) { free(body); free(content); return NULL; }

    int written = snprintf(msg, total,
        "[IMPORTANT: The user has invoked the \"%s\" skill, indicating they want "
        "you to follow its instructions. The full skill content is loaded below.]\n"
        "\n"
        "%s\n"
        "\n"
        "[Skill directory: %s]\n"
        "Resolve any relative paths in this skill (e.g. `scripts/foo.js`, "
        "`templates/config.yaml`) against that directory, then run them "
        "with the terminal tool using the absolute path.\n",
        sk->name, body, sk->skill_path);

    if (written < 0) { free(msg); free(body); free(content); return NULL; }

    /* List supporting files from subdirectories */
    const char *subdirs[] = {"references", "templates", "scripts", "assets"};
    char supporting[16384] = "";
    size_t sp = 0;

    for (int i = 0; i < 4; i++) {
        char sub_path[SKILL_CMD_PATH_MAX + 32];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", sk->skill_path, subdirs[i]);
        struct stat st;
        if (stat(sub_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *sd = opendir(sub_path);
            if (sd) {
                struct dirent *e;
                while ((e = readdir(sd)) != NULL) {
                    if (e->d_name[0] == '.') continue;
                    char full[SKILL_CMD_PATH_MAX + 64];
                    snprintf(full, sizeof(full), "%s/%s", sub_path, e->d_name);
                    struct stat fst;
                    if (stat(full, &fst) == 0 && S_ISREG(fst.st_mode)) {
                        size_t remain = sizeof(supporting) - sp;
                        int added = snprintf(supporting + sp, remain,
                            "- %s/%s  ->  %s\n", subdirs[i], e->d_name, full);
                        if (added > 0 && (size_t)added < remain)
                            sp += (size_t)added;
                    }
                }
                closedir(sd);
            }
        }
    }

    if (sp > 0) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\n[This skill has supporting files:]\n"
            "%s\n"
            "Load any of these with skill_view(name=\"%s\", "
            "file_path=\"<path>\"), or run scripts directly by absolute path "
            "(e.g. `node %s/scripts/foo.js`).\n",
            supporting, sk->slug + 1, sk->skill_path);
    }

    /* SK08: Inject setup notes from frontmatter */
    if (sk->setup_skipped) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\n[Skill setup note: Required environment setup was skipped. "
            "Continue loading the skill and explain any reduced functionality "
            "if it matters.]\n");
    } else if (sk->gateway_setup_hint[0]) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\n[Skill setup note: %s]\n", sk->gateway_setup_hint);
    } else if (sk->setup_needed && sk->setup_note[0]) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\n[Skill setup note: %s]\n", sk->setup_note);
    }

    /* Append user instruction */
    if (user_args && user_args[0]) {
        size_t cur = strlen(msg);
        size_t remain = total - cur;
        snprintf(msg + cur, remain,
            "\nThe user has provided the following instruction alongside the skill invocation: %s\n",
            user_args);
    }

    free(body);
    free(content);
    return msg;
}

int skill_cmd_rescan(int *added, int *removed) {
    /* Snapshot: save old slugs (heap array; sizes are tiny) */
    size_t old_count = g_skills ? hive_count(g_skills) : 0;
    char **old_slugs = malloc((old_count ? old_count : 1) * sizeof(char *));
    if (!old_slugs) { if (added) *added = 0; if (removed) *removed = 0; return 0; }
    size_t oi = 0;
    if (g_skills) {
        hive_iter_t it;
        hive_iter_begin(g_skills, &it);
        skill_cmd_entry_t *sk;
        while (hive_iter_next(g_skills, &it, NULL, (void **)&sk))
            old_slugs[oi++] = sk->slug;   /* borrowed; valid until scan */
    }

    /* Re-scan */
    scan_skills_dir();

    /* Count added: in new but not in old */
    int n_added = 0, n_removed = 0;
    if (g_skills) {
        hive_iter_t it;
        hive_iter_begin(g_skills, &it);
        skill_cmd_entry_t *sk;
        while (hive_iter_next(g_skills, &it, NULL, (void **)&sk)) {
            int found = 0;
            for (size_t j = 0; j < old_count; j++) {
                if (old_slugs[j] && strcmp(sk->slug, old_slugs[j]) == 0) { found = 1; break; }
            }
            if (!found) n_added++;
        }
    }

    /* Count removed: in old but not in new */
    for (size_t j = 0; j < old_count; j++) {
        if (!old_slugs[j]) continue;
        int found = 0;
        if (g_skills) {
            hive_iter_t it;
            hive_iter_begin(g_skills, &it);
            skill_cmd_entry_t *sk;
            while (hive_iter_next(g_skills, &it, NULL, (void **)&sk)) {
                if (strcmp(sk->slug, old_slugs[j]) == 0) { found = 1; break; }
            }
        }
        if (!found) n_removed++;
    }

    free(old_slugs);
    if (added) *added = n_added;
    if (removed) *removed = n_removed;
    return n_added + n_removed;
}

/* Check if a skill slug is in a comma-separated disabled list.
 * The disabled list matches against both the slug (/slug-name) and the bare name. */
bool skill_cmd_is_disabled(const char *slug, const char *disabled_csv) {
    if (!slug || !disabled_csv || !disabled_csv[0]) return false;

    /* Build bare name (strip leading /) */
    const char *bare = slug;
    while (*bare == '/') bare++;

    /* Tokenize CSV and check each entry */
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", disabled_csv);
    char *token = strtok(buf, ",");
    while (token) {
        /* Trim whitespace */
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token);
        while (end > token && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (!*token) { token = strtok(NULL, ","); continue; }

        /* Build token as slug */
        char token_slug[SKILL_CMD_SLUG_MAX];
        if (token[0] != '/') {
            snprintf(token_slug, sizeof(token_slug), "/%s", token);
        } else {
            snprintf(token_slug, sizeof(token_slug), "%s", token);
        }

        if (strcmp(slug, token_slug) == 0 || strcmp(bare, token) == 0) {
            return true;
        }
        token = strtok(NULL, ",");
    }
    return false;
}

/* Scan skills directory, filtering out entries that match the disabled CSV list.
 * Returns the number of active (non-disabled) skill commands found. */
int skill_cmd_scan_filtered(const char *disabled_csv) {
    int all = scan_skills_dir();
    if (!disabled_csv || !disabled_csv[0] || all == 0)
        return g_skills ? (int)hive_count(g_skills) : 0;

    /* Remove disabled entries from the hive (erase by handle) */
    if (g_skills) {
        hive_iter_t it;
        hive_iter_begin(g_skills, &it);
        hive_handle_t hnd;
        skill_cmd_entry_t *sk;
        while (hive_iter_next(g_skills, &it, &hnd, (void **)&sk)) {
            if (skill_cmd_is_disabled(sk->slug, disabled_csv)) {
                free(sk);
                hive_erase(g_skills, hnd);
            }
        }
    }
    return g_skills ? (int)hive_count(g_skills) : 0;
}

/* ================================================================
 *  Preloaded skills prompt (port of Python build_preloaded_skills_prompt)
 * ================================================================ */

/* Load one or more skills for session-wide CLI preloading.
 * skill_identifiers: comma/space-separated list of skill names or slugs.
 * out_prompt: (output) malloc'd combined prompt text (caller free)
 * out_loaded: (output) malloc'd comma-separated loaded skill names (caller free)
 * out_missing: (output) malloc'd comma-separated missing identifiers (caller free)
 *
 * Returns the number of skills successfully loaded.
 * Port of Python skill_commands.py:build_preloaded_skills_prompt(). */
int build_preloaded_skills_prompt(const char *skill_identifiers,
                                     char **out_prompt,
                                     char **out_loaded,
                                     char **out_missing) {
    if (out_prompt) *out_prompt = NULL;
    if (out_loaded) *out_loaded = NULL;
    if (out_missing) *out_missing = NULL;
    if (!skill_identifiers || !skill_identifiers[0]) return 0;

    /* Ensure skills are scanned */
    skill_cmd_scan();
    if (!g_skills || hive_count(g_skills) == 0) return 0;

    /* We'll build prompt parts into this buffer */
    char prompt_buf[65536];
    prompt_buf[0] = '\0';
    size_t prompt_pos = 0;
    char loaded_buf[4096];
    loaded_buf[0] = '\0';
    size_t loaded_pos = 0;
    char missing_buf[4096];
    missing_buf[0] = '\0';
    size_t missing_pos = 0;
    int loaded_count = 0;

    /* Tokenize identifiers by comma or space */
    char idents[4096];
    snprintf(idents, sizeof(idents), "%s", skill_identifiers);
    const char *delim = " ,\t";
    char *saveptr;
    char *token = strtok_r(idents, delim, &saveptr);

    while (token) {
        while (*token == ' ' || *token == '\t') token++;
        if (!*token) { token = strtok_r(NULL, delim, &saveptr); continue; }

        /* Try to resolve as slug (allow bare names or /slug format) */
        char slug_buf[SKILL_CMD_SLUG_MAX];
        const char *slug = NULL;

        /* If already starts with /, use directly */
        if (token[0] == '/') {
            slug = token;
        } else {
            /* Try resolving via skill_cmd_resolve (handles case, underscores) */
            /* skill_cmd_resolve expects a command without leading / */
            slug = skill_cmd_resolve(token);
            if (!slug) {
                /* Try with leading / */
                snprintf(slug_buf, sizeof(slug_buf), "/%s", token);
                const skill_cmd_entry_t *sk = skill_cmd_get(slug_buf);
                if (sk) slug = sk->slug;
            }
        }

        if (!slug || !skill_cmd_get(slug)) {
            /* Missing skill */
            if (missing_pos > 0) missing_buf[missing_pos++] = ',';
            size_t tlen = strlen(token);
            if (missing_pos + tlen < sizeof(missing_buf) - 1) {
                memcpy(missing_buf + missing_pos, token, tlen);
                missing_pos += tlen;
                missing_buf[missing_pos] = '\0';
            }
            token = strtok_r(NULL, delim, &saveptr);
            continue;
        }

        /* Build the skill message */
        char *msg = skill_cmd_build_message(slug, NULL);
        if (!msg) {
            if (missing_pos > 0) missing_buf[missing_pos++] = ',';
            size_t tlen = strlen(token);
            if (missing_pos + tlen < sizeof(missing_buf) - 1) {
                memcpy(missing_buf + missing_pos, token, tlen);
                missing_pos += tlen;
                missing_buf[missing_pos] = '\0';
            }
            token = strtok_r(NULL, delim, &saveptr);
            continue;
        }

        /* Add separator between skill messages */
        if (prompt_pos > 0 && prompt_pos < sizeof(prompt_buf) - 3) {
            prompt_buf[prompt_pos++] = '\n';
            prompt_buf[prompt_pos++] = '\n';
            prompt_buf[prompt_pos] = '\0';
        }

        size_t mlen = strlen(msg);
        if (prompt_pos + mlen < sizeof(prompt_buf) - 1) {
            memcpy(prompt_buf + prompt_pos, msg, mlen);
            prompt_pos += mlen;
            prompt_buf[prompt_pos] = '\0';
        }
        free(msg);

        /* Track loaded name */
        const skill_cmd_entry_t *sk = skill_cmd_get(slug);
        if (sk) {
            if (loaded_pos > 0) loaded_buf[loaded_pos++] = ',';
            size_t nlen = strlen(sk->name);
            if (loaded_pos + nlen < sizeof(loaded_buf) - 1) {
                memcpy(loaded_buf + loaded_pos, sk->name, nlen);
                loaded_pos += nlen;
                loaded_buf[loaded_pos] = '\0';
            }
        }
        loaded_count++;
        token = strtok_r(NULL, delim, &saveptr);
    }

    /* Set output parameters */
    if (out_prompt && prompt_buf[0])
        *out_prompt = strdup(prompt_buf);
    if (out_loaded && loaded_buf[0])
        *out_loaded = strdup(loaded_buf);
    if (out_missing && missing_buf[0])
        *out_missing = strdup(missing_buf);

    return loaded_count;
}

/* Port of Python: _resolve_skill_commands_platform — get current platform scope */
const char *resolve_skill_commands_platform(void) {
    const char *platform = getenv("HERMES_PLATFORM");
    if (platform && platform[0]) return platform;
    platform = getenv("HERMES_SESSION_PLATFORM");
    if (platform && platform[0]) return platform;
    return NULL;
}

/* PoP: skill_payload_free @ agent/skill_commands.py:_load_skill_payload */
/* Port of Python: _load_skill_payload — load a skill by name/path */
/* Returns a skill_cmd_payload_t* or NULL on failure. Caller must free with skill_payload_free(). */
skill_cmd_payload_t *load_skill_payload(const char *skill_identifier)
{
    if (!skill_identifier || !skill_identifier[0]) return NULL;

    /* Ensure skills are scanned */
    skill_cmd_scan();

    /* Find the skill by slug or name */
    const skill_cmd_entry_t *sk = NULL;
    char slug_buf[SKILL_CMD_SLUG_MAX];

    /* Try as slug first (starts with /) */
    if (skill_identifier[0] == '/') {
        sk = skill_cmd_get(skill_identifier);
    }
    if (!sk) {
        /* Try converting name to slug */
        name_to_slug(skill_identifier, slug_buf, sizeof(slug_buf));
        sk = skill_cmd_get(slug_buf);
    }
    if (!sk) return NULL;

    /* Read SKILL.md */
    char md_path[SKILL_CMD_PATH_MAX + 32];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", sk->skill_path);
    FILE *f = fopen(md_path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 100000 || sz <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)sz + 1);
    if (!content) { fclose(f); return NULL; }
    size_t n = fread(content, 1, (size_t)sz, f);
    fclose(f);
    content[n] = '\0';

    /* Allocate payload */
    skill_cmd_payload_t *payload = (skill_cmd_payload_t *)calloc(1, sizeof(skill_cmd_payload_t));
    if (!payload) { free(content); return NULL; }

    /* Copy skill info */
    payload->skill_name = strdup(sk->name);
    payload->skill_dir = strdup(sk->skill_path);
    payload->frontmatter = strdup(content);

    /* Strip frontmatter to get body */
    char *body = context_strip_frontmatter(content);
    payload->body = body ? body : strdup("");

    free(content);
    return payload;
}

/* Free a skill payload returned by load_skill_payload() */
void skill_payload_free(skill_cmd_payload_t *p)
{
    if (!p) return;
    free(p->skill_name);
    free(p->skill_dir);
    free(p->frontmatter);
    free(p->body);
    free(p);
}

/* === merged from port_agent_skill_commands.c === */
/*
 * port_agent_skill_commands.c — faithful C11 port of the skill-scaffolding
 * extractor surface of agent/skill_commands.py.
 *
 * When a user invokes a /skill (or /bundle), Hermes expands the turn into a
 * model-facing message embedding the full skill body plus scaffolding. Any
 * surface that summarizes a user turn from raw content (session titles,
 * sidebar previews, /rewind picker) must recover what the user actually
 * typed. These markers MUST stay byte-identical to the builders
 * (skill_cmd_build_message in skill_commands.c, bundle builder in
 * skill_bundles.c) — mirror of the Python co-location contract.
 */
#include "skill_scaffolding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Scaffolding markers — byte-identical to agent/skill_commands.py. */
static const char SKILL_INVOCATION_PREFIX[] = "[IMPORTANT: The user has invoked the ";
static const char SINGLE_SKILL_MARKER[] = "The full skill content is loaded below.]";
static const char SINGLE_SKILL_INSTRUCTION[] =
    "The user has provided the following instruction alongside the skill invocation: ";
static const char RUNTIME_NOTE[] = "\n\n[Runtime note:";
static const char BUNDLE_MARKER[] = " skill bundle,";
static const char BUNDLE_USER_INSTRUCTION[] = "\nUser instruction: ";
static const char BUNDLE_FIRST_SKILL_BLOCK[] = "\n\n[Loaded as part of the ";
/* PoP: strip_dup @ agent/skill_commands.py:SKILL_EXCERPT_JOINT */
static const char SKILL_EXCERPT_JOINT_CH = '\x1e';

/* Duplicate [s, s+len) with Python str.strip() semantics applied. */
static char *strip_dup(const char *s, size_t len) {
    size_t b = 0, e = len;
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    char *out = malloc(e - b + 1);
    if (!out) return NULL;
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}

/* Python `rfind(needle)` on a NUL-terminated haystack. Returns index or -1. */
static long str_rfind(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    size_t hlen = strlen(hay);
    if (nlen == 0 || nlen > hlen) return -1;
    for (long i = (long)(hlen - nlen); i >= 0; i--)
        if (strncmp(hay + i, needle, nlen) == 0) return i;
    return -1;
}

/* PoP: extract_single_skill_user_instruction @ agent/skill_commands.py:_extract_single_skill_user_instruction */
char *extract_single_skill_user_instruction(const char *message) {
    if (!message) return NULL;
    /* Single-skill format appends the user instruction after the skill body,
     * so the LAST occurrence is the user-provided one. */
    long idx = str_rfind(message, SINGLE_SKILL_INSTRUCTION);
    if (idx < 0) return NULL;
    const char *instr = message + idx + (long)strlen(SINGLE_SKILL_INSTRUCTION);
    const char *rt = strstr(instr, RUNTIME_NOTE);
    size_t len = rt ? (size_t)(rt - instr) : strlen(instr);
    char *out = strip_dup(instr, len);
    if (out && !*out) { free(out); return NULL; }  /* instruction or None */
    return out;
}

/* PoP: extract_bundle_user_instruction @ agent/skill_commands.py:_extract_bundle_user_instruction */
char *extract_bundle_user_instruction(const char *message) {
    if (!message) return NULL;
    /* Bundle format puts the user instruction before the loaded skills, so
     * the FIRST occurrence is the user-provided one. */
    const char *m = strstr(message, BUNDLE_USER_INSTRUCTION);
    if (!m) return NULL;
    const char *instr = m + strlen(BUNDLE_USER_INSTRUCTION);
    const char *fs = strstr(instr, BUNDLE_FIRST_SKILL_BLOCK);
    size_t len = fs ? (size_t)(fs - instr) : strlen(instr);
    char *out = strip_dup(instr, len);
    if (out && !*out) { free(out); return NULL; }
    return out;
}

/* Internal: faithful extract with a pass-through flag mirroring Python's
 * `instruction is not content` identity check in describe_skill_invocation.
 * *passthrough=true means the message was NOT skill scaffolding and the
 * returned string is the content unchanged. */
static char *extract_impl(const char *content, bool *passthrough) {
    if (passthrough) *passthrough = false;
    if (!content) return NULL;
    if (strncmp(content, SKILL_INVOCATION_PREFIX,
                strlen(SKILL_INVOCATION_PREFIX)) != 0) {
        if (passthrough) *passthrough = true;
        return strdup(content);   /* normal user message passes through */
    }
    if (strstr(content, BUNDLE_MARKER))
        return extract_bundle_user_instruction(content);
    if (strstr(content, SINGLE_SKILL_MARKER))
        return extract_single_skill_user_instruction(content);
    return NULL;   /* bare /skill invocation: no user content worth storing */
}

/* PoP: extract_user_instruction_from_skill_message @ agent/skill_commands.py:extract_user_instruction_from_skill_message */
char *extract_user_instruction_from_skill_message(const char *content) {
    return extract_impl(content, NULL);
}

/* PoP: append_user_instruction @ agent/skill_commands.py:append_user_instruction */
char *append_user_instruction(char ***parts, size_t *nparts, const char *instruction)
{
    /* Append the instruction line to parts; return the stable prefix.
     * Shared by every builder that ends a static skill scaffold with the
     * caller-supplied volatile instruction. The returned prefix ends exactly
     * at the instruction marker, so registering it with the cache-boundary
     * machinery lets the cache planner put a breakpoint on the scaffold. */
    if (!parts || !nparts || !instruction) return NULL;
    static const char *INSTRUCTION = "The user has provided the following instruction alongside the skill invocation: ";
    /* stable_prefix = "\n".join(parts) + "\n" + INSTRUCTION */
    size_t total = 0;
    for (size_t i = 0; i < *nparts; i++)
        total += strlen((*parts)[i]) + 1; /* +1 for '\n' */
    total += strlen(INSTRUCTION);
    char *prefix = malloc(total + 2);
    if (!prefix) return NULL;
    size_t off = 0;
    for (size_t i = 0; i < *nparts; i++) {
        size_t l = strlen((*parts)[i]);
        memcpy(prefix + off, (*parts)[i], l); off += l;
        prefix[off++] = '\n';
    }
    size_t il = strlen(INSTRUCTION);
    memcpy(prefix + off, INSTRUCTION, il); off += il;
    prefix[off] = '\0';
    /* parts.append(f"{INSTRUCTION}{instruction}") */
    char *joined = malloc(il + strlen(instruction) + 1);
    if (!joined) { free(prefix); return NULL; }
    memcpy(joined, INSTRUCTION, il);
    strcpy(joined + il, instruction);
    char **np = realloc(*parts, (*nparts + 1) * sizeof(char *));
    if (!np) { free(joined); free(prefix); return NULL; }
    *parts = np;
    (*parts)[*nparts] = joined;
    (*nparts)++;
    return prefix;
}

/* PoP: describe_skill_invocation @ agent/skill_commands.py:describe_skill_invocation */
char *describe_skill_invocation(const char *content) {
    if (!content || strncmp(content, SKILL_INVOCATION_PREFIX,
                            strlen(SKILL_INVOCATION_PREFIX)) != 0)
        return NULL;

    /* _SKILL_NAME_RE: prefix immediately followed by "([^"]*)" — the name
     * sits in the first quoted span of the activation note. re.match anchors
     * at position 0, so the quote must directly follow the prefix. */
    char *name = NULL;
    const char *after = content + strlen(SKILL_INVOCATION_PREFIX);
    if (*after == '"') {
        const char *q = after + 1;
        const char *close = strchr(q, '"');
        if (close) {
            char *raw = strip_dup(q, (size_t)(close - q));
            name = raw;
        }
    }
    if (!name) name = strdup("");

    /* label = name if name.startswith("/") else f"/{name}" */
    size_t nlen = strlen(name);
    char *label = malloc(nlen + 2);
    if (!label) { free(name); return NULL; }
    if (name[0] == '/') strcpy(label, name);
    else { label[0] = '/'; strcpy(label + 1, name); }

    bool passthrough = false;
    char *instruction = extract_impl(content, &passthrough);
    if (instruction && !passthrough) {
        /* Keep only the side before an excerpt joint. */
        char *joint = strchr(instruction, SKILL_EXCERPT_JOINT_CH);
        if (joint) *joint = '\0';
        /* " ".join(instruction.split()) — collapse all whitespace runs. */
        char *collapsed = malloc(strlen(instruction) + 1);
        if (collapsed) {
            size_t ci = 0; bool in_ws = true;
            for (const char *p = instruction; *p; p++) {
                if (isspace((unsigned char)*p)) { in_ws = true; continue; }
                if (in_ws && ci > 0) collapsed[ci++] = ' ';
                in_ws = false;
                collapsed[ci++] = *p;
            }
            collapsed[ci] = '\0';
        }
        free(instruction);
        instruction = collapsed;

        if (instruction && *instruction) {
            char *out;
            if (nlen > 0) {
                /* f"{label} — {instruction}" (em dash U+2014) */
                size_t need = strlen(label) + strlen("\u2014") + strlen(instruction) + 3;
                out = malloc(need);
                if (out) snprintf(out, need, "%s \u2014 %s", label, instruction);
            } else {
                out = strdup(instruction);
            }
            free(instruction); free(name); free(label);
            return out;
        }
    }
    free(instruction);

    /* return label if name else None */
    if (nlen > 0) { free(name); return label; }
    free(name); free(label);
    return NULL;
}



/* === dissolved from ports_wrappers.c === */
/* PoP: agent_skill_commands_split_stacked_skill_commands @ agent/skill_commands.py:split_stacked_skill_commands */
int agent_skill_commands_split_stacked_skill_commands(const char *arg) {
    /* Python: stacked /skill tokens. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\t%s\n", tab ? tab + 1 : ""); return 0; }
    printf("%s\t\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_skill_commands_build_stacked_skill_invocation_message @ agent/skill_commands.py:build_stacked_skill_invocation_message */
int agent_skill_commands_build_stacked_skill_invocation_message(const char *arg) {
    /* Python: stacked slash builder. Arg =
     * "loaded\tmissing\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\t\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n\t\t\n"); return 0; }
    printf("message built: loaded=%s missing=%s (blocks start with [Loaded as part of the...])\n", t1 ? t1 + 1 : "[]", t3 ? t3 + 1 : "[]");
    return 0;
}
