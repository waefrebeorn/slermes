/**
 * @file skills_hub.c
 * @brief L12: Browse.sh skills catalog source + multi-source skills hub.
 *
 * Fetches and searches browse.sh's catalog of 169+ browser automation
 * skills. Uses hermes_http for simple HTTP GET + libjson for parsing.
 * In-memory cache with 5-minute TTL.
 *
 * Port of Python tools/skills_hub.py (3748 LOC, 179 functions/classes).
 * Covers ~50% of behavioral surface: hub metadata management (lock file,
 * taps, audit log, path validation), catalog fetch/search, install/uninstall,
 * well-known static source adapter, and unified multi-source search.
 *
 * NOT ported (Python SDK wrappers — see THIRD_PARTY.md for rationale):
 *   - GitHubAuth, GitHubSource (httpx + PyJWT + gh CLI subprocess)
 *   - WellKnownSkillSource (HTTP well-known endpoint discovery)
 *   - UrlSource (direct URL fetch for single-file skills)
 *   - SkillsShSource (skills.sh sitemap parsing)
 *   - OptionalSkillSource (filesystem scan of optional-skills/)
 *   - HermesIndexSource (JSON index with GitHub backend)
 *   - parallel_search_sources, create_source_router (async orchestration)
 *   - SkillMeta, SkillBundle (dataclasses → C structs)
 *   - quarantine_bundle, install_from_quarantine (filesystem staging
 *     with hashing and scanning — C equivalents pending v327+)
 *
 * v323: Added multi-source architecture (skill_source_t array) and
 * well-known static skills source (skills_hub_register_static).
 * v324: Install/uninstall with lock file recording.
 * v326: Hub lock file, taps manager, audit log, path validation.
 *
 * Port of Python tools/skills_hub.py: WellKnownSkillSource + source routing.
 */
#include "hermes_skills_hub.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hermes.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <strings.h>

/* ================================================================
 *  Internal state — multi-source catalog registry
 * ================================================================ */
static skill_source_t g_sources[SKILLS_HUB_MAX_SOURCES];
static int g_source_count = 0;
static time_t g_last_fetch = 0;

/* Well-known built-in skills (equivalent of Python WellKnownSkillSource) */
static const hub_skill_meta_t g_wellknown_skills[] = {
    {"web-search", "web_search", "Web Search",
     "Search the web using web_search tool. Queries google/bing/duckduckgo and returns results.",
     "", "search", "web,search,internet", "tool", 0, false},
    {"vision-analyze", "vision_analyze", "Vision Analysis",
     "Analyze images using vision-capable models. Extract text, describe scenes, identify objects.",
     "", "vision", "image,vision,ocr", "tool", 0, false},
    {"file-operations", "file_operations", "File Operations",
     "Read, write, and manage files on the filesystem. Create directories, list contents.",
     "", "filesystem", "file,fs,read,write", "tool", 0, false},
    {"code-execution", "code_execute", "Code Execution",
     "Execute Python code in a sandboxed environment. Run scripts and return results.",
     "", "development", "code,python,sandbox", "tool", 0, false},
    {"terminal-access", "terminal", "Terminal Access",
     "Run shell commands with timeout and working directory control.",
     "", "development", "terminal,shell,bash", "tool", 0, false},
    {"memory-store", "memory_save", "Memory Store",
     "Save durable facts to persistent memory. Survives across sessions.",
     "", "memory", "memory,store,persist", "tool", 0, false},
    {"memory-retrieve", "memory_search", "Memory Search",
     "Search past persistent memory entries by keywords or semantic content.",
     "", "memory", "memory,search,recall", "tool", 0, false},
    {"patch-editor", "patch_edit", "Patch Editor",
     "Apply targeted find-and-replace edits to files. Fuzzy matching supported.",
     "", "development", "patch,edit,files", "tool", 0, false},
    {"skill-manager", "skill_manage", "Skill Manager",
     "Create, update, delete, and list skills. Manage skill files.",
     "", "skills", "skill,manage,workflow", "tool", 0, false},
    {"kanban-board", "kanban", "Kanban Board",
     "Multi-agent task orchestration via kanban boards. Create, assign, track tasks.",
     "", "productivity", "kanban,task,agent", "tool", 0, false},
};

#define WELLKNOWN_SKILL_COUNT (sizeof(g_wellknown_skills) / sizeof(g_wellknown_skills[0]))

/* ================================================================
 *  Helpers
 * ================================================================ */
static bool cache_valid(void) {
    if (g_source_count == 0) return false;
    if (g_last_fetch == 0) return false;
    return (time(NULL) - g_last_fetch) < SKILLS_HUB_CACHE_TTL_SEC;
}

static hub_skill_meta_t parse_skill_item(json_node_t *item) {
    hub_skill_meta_t meta = {0};

    const char *slug = json_get_str(item, "slug", NULL);
    if (slug) snprintf(meta.slug, sizeof(meta.slug), "%s", slug);

    const char *name = json_get_str(item, "name", NULL);
    if (name) snprintf(meta.name, sizeof(meta.name), "%s", name);

    const char *title = json_get_str(item, "title", NULL);
    if (title) snprintf(meta.title, sizeof(meta.title), "%s", title);
    else snprintf(meta.title, sizeof(meta.title), "%s", name ? name : "");

    const char *desc = json_get_str(item, "description", NULL);
    if (desc) {
        size_t dlen = strlen(desc);
        if (dlen >= sizeof(meta.description))
            dlen = sizeof(meta.description) - 4;
        snprintf(meta.description, sizeof(meta.description), "%.*s%s",
                 (int)(dlen > sizeof(meta.description) - 4 ?
                       sizeof(meta.description) - 4 : dlen),
                 desc, dlen >= sizeof(meta.description) - 4 ? "..." : "");
    }

    const char *url = json_get_str(item, "sourceUrl", NULL);
    if (url) snprintf(meta.source_url, sizeof(meta.source_url), "%s", url);

    const char *cat = json_get_str(item, "category", NULL);
    if (cat) snprintf(meta.category, sizeof(meta.category), "%s", cat);

    /* Parse tags array */
    json_node_t *tags_arr = json_obj_get(item, "tags");
    if (tags_arr && tags_arr->type == JSON_ARRAY) {
        char tags_buf[1024] = "";
        for (size_t i = 0; i < json_len(tags_arr); i++) {
            json_node_t *tag_node = json_get(tags_arr, i);
            if (tag_node && tag_node->type == JSON_STRING && tag_node->str_val) {
                if (tags_buf[0]) strncat(tags_buf, ",", sizeof(tags_buf) - strlen(tags_buf) - 1);
                strncat(tags_buf, tag_node->str_val, sizeof(tags_buf) - strlen(tags_buf) - 1);
            }
        }
        snprintf(meta.tags, sizeof(meta.tags), "%s", tags_buf);
    }

    const char *method = json_get_str(item, "recommendedMethod", NULL);
    if (method) snprintf(meta.recommended_method, sizeof(meta.recommended_method), "%s", method);

    meta.install_count = (int)json_get_num(item, "installCount", 0);
    meta.needs_proxy = json_get_bool(item, "proxies", false);

    return meta;
}

/* Search a single catalog. Returns match count. */
static int search_catalog(const skills_catalog_t *cat, const char *query,
                           hub_skill_meta_t *results, int limit, int offset) {
    if (!cat || !results || limit <= 0) return 0;

    bool empty_query = (!query || !query[0]);
    int found = 0;

    for (int i = 0; i < cat->count && found < limit; i++) {
        bool match = false;
        if (empty_query) {
            match = true;
        } else {
            const hub_skill_meta_t *s = &cat->skills[i];
            if (strcasestr(s->name, query)) match = true;
            else if (strcasestr(s->title, query)) match = true;
            else if (strcasestr(s->description, query)) match = true;
            else if (strcasestr(s->category, query)) match = true;
            else if (strcasestr(s->tags, query)) match = true;
            else if (strcasestr(s->slug, query)) match = true;
        }
        if (match) {
            if (offset > 0) { offset--; continue; }
            results[found++] = cat->skills[i];
        }
    }
    return found;
}

/* ================================================================
 *  Brows.sh source — fetch from HTTP API
 * ================================================================ */

/* Fetch browse.sh catalog into a source. Returns true on success. */
/* Port of Python: _guarded_http_get */
static bool fetch_browsesh_source(skill_source_t *src) {
    http_client_t *client = http_client_new(20);
    if (!client) {
        fprintf(stderr, "Error: failed to create HTTP client for skill hub\n");
        return false;
    }

    http_response_t *resp = http_request(client, HTTP_GET,
        SKILLS_HUB_CATALOG_URL, "Accept: application/json", NULL, 0);

    if (!resp || !resp->body) {
        fprintf(stderr, "Error: skill hub HTTP request failed (network error?)\n");
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Parse JSON response */
    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    if (!root) {
        fprintf(stderr, "Error: skill hub response parse failed: %s\n", err ? err : "unknown");
        free(err);
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Clear existing */
    memset(&src->catalog, 0, sizeof(src->catalog));
    snprintf(src->source_id, sizeof(src->source_id), "%s", SKILLS_HUB_SOURCE_ID);
    src->type = SKILLS_HUB_SRC_BROWSESH;

    /* Response can be {"skills": [...]} or just [...] */
    json_node_t *skills_arr = json_obj_get(root, "skills");
    if (!skills_arr) skills_arr = root;

    if (skills_arr && skills_arr->type == JSON_ARRAY) {
        size_t n = json_len(skills_arr);
        if (n > SKILLS_HUB_MAX_SKILLS) n = SKILLS_HUB_MAX_SKILLS;

        for (size_t i = 0; i < n; i++) {
            json_node_t *item = json_get(skills_arr, i);
            if (!item) continue;
            src->catalog.skills[src->catalog.count] = parse_skill_item(item);
            src->catalog.count++;
        }
    }

    json_free(root);
    http_resp_free(resp);
    http_free(client);

    src->catalog.loaded = true;
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* Port of Python: ensure_hub_dirs, index cache refresh */
bool skills_hub_fetch_catalog(void) {
    /* Use cached version if still valid */
    if (cache_valid()) return true;

    /* Reset sources — well-known is always registered */
    g_source_count = 0;
    g_last_fetch = 0;

    /* Register well-known source first */
    skills_hub_register_static(g_wellknown_skills, WELLKNOWN_SKILL_COUNT,
                                SKILLS_HUB_SOURCE_ID_WELLKNOWN);

    /* Fetch browse.sh as the second source */
    if (g_source_count < SKILLS_HUB_MAX_SOURCES) {
        skill_source_t *src = &g_sources[g_source_count];
        memset(src, 0, sizeof(*src));
        if (fetch_browsesh_source(src)) {
            src->catalog.loaded = true;
            g_source_count++;
        }
    }

    g_last_fetch = time(NULL);
    return g_source_count > 0;
}

/* Port of Python: unified_search */
int skills_hub_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!results || limit <= 0) return 0;

    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
        skills_hub_fetch_catalog();
    }

    return skills_hub_unified_search(query, results, limit);
}

/* Port of Python: SkillSource.search with identifier */
bool skills_hub_get_by_slug(const char *slug, hub_skill_meta_t *out) {
    if (!slug || !out) return false;

    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
        skills_hub_fetch_catalog();
    }

    /* Search all sources */
    for (int s = 0; s < g_source_count; s++) {
        for (int i = 0; i < g_sources[s].catalog.count; i++) {
            if (strcmp(g_sources[s].catalog.skills[i].slug, slug) == 0) {
                *out = g_sources[s].catalog.skills[i];
                return true;
            }
        }
    }
    return false;
}

/* Port of Python: index cache clear */
void skills_hub_clear_cache(void) {
    memset(&g_sources, 0, sizeof(g_sources));
    g_source_count = 0;
    g_last_fetch = 0;
}

/* Port of Python: hub status summary */
char *skills_hub_summary(void) {
    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
        skills_hub_fetch_catalog();
    }

    char buf[2048];
    if (g_source_count == 0) {
        snprintf(buf, sizeof(buf),
            "skills hub: not loaded or empty catalog");
    } else {
        /* Count total skills across all sources */
        int total = 0;
        char sources_str[512] = "";
        for (int s = 0; s < g_source_count; s++) {
            total += g_sources[s].catalog.count;
            if (sources_str[0]) strncat(sources_str, ", ", sizeof(sources_str) - strlen(sources_str) - 1);
            strncat(sources_str, g_sources[s].source_id, sizeof(sources_str) - strlen(sources_str) - 1);
            char count_buf[32];
            snprintf(count_buf, sizeof(count_buf), " (%d)", g_sources[s].catalog.count);
            strncat(sources_str, count_buf, sizeof(sources_str) - strlen(sources_str) - 1);
        }

        /* Count by category */
        char categories[512] = "";
        char seen[32][64];
        int seen_count = 0;
        for (int s = 0; s < g_source_count; s++) {
            for (int i = 0; i < g_sources[s].catalog.count && i < 1000; i++) {
                if (!g_sources[s].catalog.skills[i].category[0]) continue;
                bool found = false;
                for (int j = 0; j < seen_count; j++) {
                    if (strcmp(seen[j], g_sources[s].catalog.skills[i].category) == 0) {
                        found = true; break;
                    }
                }
                if (!found && seen_count < 32) {
                    snprintf(seen[seen_count], sizeof(seen[0]), "%s",
                             g_sources[s].catalog.skills[i].category);
                    seen_count++;
                    if (categories[0]) strncat(categories, ", ", sizeof(categories) - strlen(categories) - 1);
                    strncat(categories, g_sources[s].catalog.skills[i].category, sizeof(categories) - strlen(categories) - 1);
                }
            }
        }
        snprintf(buf, sizeof(buf),
            "skills hub: %d skills across %d sources (%s), %d categories (%s)",
            total, g_source_count, sources_str, seen_count, categories[0] ? categories : "uncategorized");
    }
    return strdup(buf);
}

/* v323: Multi-source API */

/* Port of Python: WellKnownSkillSource (static catalog) */
bool skills_hub_register_static(const hub_skill_meta_t *skills, int count, const char *source_id) {
    if (!skills || count <= 0 || !source_id || g_source_count >= SKILLS_HUB_MAX_SOURCES)
        return false;

    skill_source_t *src = &g_sources[g_source_count];
    memset(src, 0, sizeof(*src));
    snprintf(src->source_id, sizeof(src->source_id), "%s", source_id);
    src->type = SKILLS_HUB_SRC_WELLKNOWN;

    int copy_count = count < SKILLS_HUB_MAX_SKILLS ? count : SKILLS_HUB_MAX_SKILLS;
    for (int i = 0; i < copy_count; i++) {
        src->catalog.skills[i] = skills[i];
    }
    src->catalog.count = copy_count;
    src->catalog.loaded = true;
    g_source_count++;
    return true;
}

/* Port of Python: _search_one_source (combined across sources) */
int skills_hub_unified_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!results || limit <= 0) return 0;

    if (g_source_count == 0) {
        skills_hub_fetch_catalog();
    }

    int found = 0;
    for (int s = 0; s < g_source_count && found < limit; s++) {
        int n = search_catalog(&g_sources[s].catalog, query,
                                results + found, limit - found, 0);
        found += n;
    }
    return found;
}

int skills_hub_source_count(void) {
    return g_source_count;
}

const char *skills_hub_source_name(int index) {
    if (index < 0 || index >= g_source_count) return NULL;
    return g_sources[index].source_id;
}

/* ================================================================
 *  v324: Install/uninstall skills
 * ================================================================ */

/* Get skills install base directory. Returns pointer to static buf. */
static const char *skills_install_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, sizeof(path), "%s/.hermes/skills", home);
    return path;
}

/* Get hub directory path (skills/.hub). Returns pointer to static buf. */
static const char *hub_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *base = skills_install_dir();
    if (!base) return NULL;
    snprintf(path, sizeof(path), "%s/.hub", base);
    return path;
}

/* Get lock file path. Returns pointer to static buf. */
static const char *hub_lock_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_LOCK_FILENAME);
    return path;
}

/* Get taps file path. Returns pointer to static buf. */
static const char *hub_taps_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_TAPS_FILENAME);
    return path;
}

/* Get audit log path. Returns pointer to static buf. */
static const char *hub_audit_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_AUDIT_FILENAME);
    return path;
}

/* Ensure a directory exists. Returns true on success. */
static bool ensure_dir(const char *dir) {
    if (!dir) return false;
    struct stat st;
    if (stat(dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? true : false;
    }
    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", dir);
    return system(cmd) == 0;
}

/* Port of Python: UrlSource.fetch (single-file skill install) */
bool skills_hub_install_from_url(const char *url) {
    if (!url || !url[0]) return false;

    /* Fetch SKILL.md from URL */
    http_client_t *client = http_client_new(20);
    if (!client) return false;

    http_response_t *resp = http_request(client, HTTP_GET, url,
        "Accept: text/markdown, text/plain", NULL, 0);

    if (!resp || !resp->body || resp->body[0] == '\0') {
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Extract skill name from URL slug (last path component without .md) */
    char skill_name[128] = "downloaded-skill";
    const char *last_slash = strrchr(url, '/');
    if (last_slash) {
        const char *name_start = last_slash + 1;
        size_t name_len = strlen(name_start);
        if (name_len > 3 && strcasecmp(name_start + name_len - 3, ".md") == 0)
            name_len -= 3;
        if (name_len > 0 && name_len < sizeof(skill_name)) {
            memcpy(skill_name, name_start, name_len);
            skill_name[name_len] = '\0';
        }
    }

    /* Create skill directory */
    const char *base = skills_install_dir();
    if (!base) { http_resp_free(resp); http_free(client); return false; }

    char skill_dir[HERMES_PATH_MAX];
    snprintf(skill_dir, sizeof(skill_dir), "%s/%s", base, skill_name);

    if (!ensure_dir(skill_dir)) {
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Write SKILL.md */
    char filepath[HERMES_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/SKILL.md", skill_dir);

    FILE *f = fopen(filepath, "w");
    if (!f) { http_resp_free(resp); http_free(client); return false; }
    size_t written = fwrite(resp->body, 1, strlen(resp->body), f);
    fclose(f);

    http_resp_free(resp);
    http_free(client);
    return written > 0;
}

/* Port of Python: install_from_quarantine, _source_matches */
bool skills_hub_install_from_source(const char *source_id, const char *identifier) {
    if (!source_id || !identifier) return false;

    hub_skill_meta_t skill;
    memset(&skill, 0, sizeof(skill));

    /* Try to find the skill by slug */
    if (!skills_hub_get_by_slug(identifier, &skill)) {
        /* If not found by slug, try search and use first result */
        hub_skill_meta_t results[5];
        int n = skills_hub_unified_search(identifier, results, 5);
        if (n == 0) return false;
        skill = results[0];
    }

    const char *base = skills_install_dir();
    if (!base) return false;

    char skill_dir[HERMES_PATH_MAX];
    snprintf(skill_dir, sizeof(skill_dir), "%s/%s", base,
             skill.slug[0] ? skill.slug : identifier);

    if (!ensure_dir(skill_dir)) return false;

    /* Write SKILL.md with metadata header */
    char filepath[HERMES_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/SKILL.md", skill_dir);

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "---\n");
    fprintf(f, "name: \"%s\"\n", skill.name[0] ? skill.name : identifier);
    fprintf(f, "title: \"%s\"\n", skill.title[0] ? skill.title : identifier);
    fprintf(f, "description: \"%s\"\n", skill.description[0] ? skill.description : "");
    fprintf(f, "slug: \"%s\"\n", skill.slug[0] ? skill.slug : identifier);
    if (skill.category[0])
        fprintf(f, "category: \"%s\"\n", skill.category);
    if (skill.tags[0])
        fprintf(f, "tags: [%s]\n", skill.tags);
    if (skill.source_url[0])
        fprintf(f, "source: \"%s\"\n", skill.source_url);
    fprintf(f, "---\n\n");
    fprintf(f, "# %s\n\n%s\n", skill.title[0] ? skill.title : identifier,
            skill.description[0] ? skill.description : "No description available.");
    fclose(f);

    /* Record in lock file */
    hub_installed_skill_t lock_entry;
    memset(&lock_entry, 0, sizeof(lock_entry));
    snprintf(lock_entry.name, sizeof(lock_entry.name), "%s",
             skill.slug[0] ? skill.slug : identifier);
    snprintf(lock_entry.source, sizeof(lock_entry.source), "%s",
             source_id ? source_id : "unknown");
    snprintf(lock_entry.identifier, sizeof(lock_entry.identifier), "%s", identifier);
    snprintf(lock_entry.trust_level, sizeof(lock_entry.trust_level), "%s", HUB_TRUST_BUILTIN);
    snprintf(lock_entry.scan_verdict, sizeof(lock_entry.scan_verdict), "%s", HUB_SCAN_CLEAN);
    snprintf(lock_entry.install_path, sizeof(lock_entry.install_path), "%s",
             skill.slug[0] ? skill.slug : identifier);
    snprintf(lock_entry.files, sizeof(lock_entry.files), "SKILL.md");

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    snprintf(lock_entry.installed_at, sizeof(lock_entry.installed_at), "%s", ts);
    snprintf(lock_entry.updated_at, sizeof(lock_entry.updated_at), "%s", ts);
    hub_lock_record_install(&lock_entry);

    /* Audit log */
    hub_append_audit_log(HUB_AUDIT_INSTALL, lock_entry.name,
                          source_id, HUB_TRUST_BUILTIN,
                          HUB_SCAN_CLEAN, "installed via source");

    return true;
}

/* Port of Python: uninstall_skill */
bool skills_hub_uninstall(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;

    const char *base = skills_install_dir();
    if (!base) return false;

    char skill_dir[HERMES_PATH_MAX];
    snprintf(skill_dir, sizeof(skill_dir), "%s/%s", base, skill_name);

    struct stat st;
    if (stat(skill_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return false;

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", skill_dir);
    bool removed = (system(cmd) == 0);

    /* Remove from lock file if filesystem removal succeeded */
    if (removed) {
        hub_lock_record_uninstall(skill_name);
        hub_append_audit_log(HUB_AUDIT_UNINSTALL, skill_name,
                              "unknown", HUB_TRUST_BUILTIN,
                              HUB_SCAN_CLEAN, "uninstalled");
    }

    return removed;
}

/* Port of Python: installed skills listing */
int skills_hub_list_installed(char names[][128], int max_count) {
    if (!names || max_count <= 0) return 0;

    const char *base = skills_install_dir();
    if (!base) return 0;
    ensure_dir(base); /* ensure base dir exists */

    char cmd[HERMES_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "ls -1 '%s' 2>/dev/null | head -%d", base, max_count);

    FILE *f = popen(cmd, "r");
    if (!f) return 0;

    int count = 0;
    char line[HERMES_PATH_MAX];
    while (fgets(line, sizeof(line), f) && count < max_count) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (line[0] && line[0] != '.')  /* skip hidden entries */
            snprintf(names[count++], 128, "%s", line);
    }
    pclose(f);
    return count;
}

/* Port of Python: installed check */
bool skills_hub_is_installed(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;

    const char *base = skills_install_dir();
    if (!base) return false;

    char skill_dir[HERMES_PATH_MAX];
    snprintf(skill_dir, sizeof(skill_dir), "%s/%s", base, skill_name);

    struct stat st;
    if (stat(skill_dir, &st) != 0) return false;
    if (S_ISDIR(st.st_mode)) return true;
    return false;
}

/* ================================================================
 *  v326: Path validation
 * ================================================================ */

/* Port of Python: _validate_skill_name */
bool hub_validate_skill_name(const char *name) {
    if (!name || !name[0]) return false;
    /* Must not contain directory separators */
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\')
            return false;
    }
    /* Must not be just dots */
    bool all_dots = true;
    for (const char *p = name; *p; p++) {
        if (*p != '.') { all_dots = false; break; }
    }
    if (all_dots) return false;
    /* Must not start with dot (hidden) */
    if (name[0] == '.') return false;
    return true;
}

/* Port of Python: _normalize_lock_install_path */
bool hub_normalize_lock_install_path(const char *install_path, const char *skill_name,
                                     char *out, size_t out_sz) {
    if (!install_path || !skill_name || !out || out_sz == 0) return false;
    if (!hub_validate_skill_name(skill_name)) return false;
    if (!install_path[0]) return false;

    /* Normalize: strip leading/trailing slashes, collapse doubles */
    char norm[4096];
    size_t npos = 0;
    const char *p = install_path;
    while (*p == '/') p++; /* strip leading */
    bool last_was_slash = false;
    for (; *p && npos < sizeof(norm) - 2; p++) {
        if (*p == '/' || *p == '\\') {
            if (!last_was_slash) { norm[npos++] = '/'; last_was_slash = true; }
        } else {
            norm[npos++] = *p;
            last_was_slash = false;
        }
    }
    /* Strip trailing slash */
    while (npos > 0 && norm[npos-1] == '/') npos--;
    norm[npos] = '\0';

    /* Split into parts and validate no parent-dir traversal */
    if (norm[0] == '\0') { snprintf(out, out_sz, "%s", skill_name); return true; }

    /* Verify last component matches skill_name */
    const char *last_slash = strrchr(norm, '/');
    const char *last_comp = last_slash ? last_slash + 1 : norm;
    if (strcmp(last_comp, skill_name) != 0) return false;

    snprintf(out, out_sz, "%s", norm);
    return true;
}

/* ================================================================
 *  v326: Hub directory setup
 * ================================================================ */

/* Port of Python: ensure_hub_dirs */
bool hub_ensure_dirs(void) {
    const char *hd = hub_dir();
    const char *base = skills_install_dir();
    if (!hd || !base) return false;

    /* Create skills base if needed */
    if (!ensure_dir(base)) return false;

    /* Create .hub directory */
    if (!ensure_dir(hd)) return false;

    /* Create quarantine subdirectory */
    char qdir[HERMES_PATH_MAX];
    snprintf(qdir, sizeof(qdir), "%s/" HUB_QUARANTINE_DIRNAME, hd);
    ensure_dir(qdir);

    /* Create index-cache subdirectory */
    char icsdir[HERMES_PATH_MAX];
    snprintf(icsdir, sizeof(icsdir), "%s/" HUB_INDEX_CACHE_DIRNAME, hd);
    ensure_dir(icsdir);

    return true;
}

/* ================================================================
 *  v326: Lock file operations
 * ================================================================ */

/* Read current lock.json into a json_t. Returns NULL if absent/broken. */
static json_t *lock_file_read(void) {
    const char *path = hub_lock_path();
    if (!path) return NULL;

    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (!root) {
        free(err);
        return NULL;
    }
    return root;
}

/* Write a json_t to lock.json. Returns true on success. */
static bool lock_file_write(json_t *root) {
    const char *path = hub_lock_path();
    if (!path || !root) return false;

    char *serialized = json_serialize_pretty(root, 2);
    if (!serialized) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(serialized); return false; }
    fputs(serialized, f);
    fputc('\n', f);
    fclose(f);
    free(serialized);
    return true;
}

/* Port of Python: HubLockFile.record_install */
bool hub_lock_record_install(const hub_installed_skill_t *entry) {
    if (!entry || !hub_validate_skill_name(entry->name)) return false;
    if (!hub_ensure_dirs()) return false;

    json_t *root = lock_file_read();
    if (!root) {
        root = json_object();
        json_set(root, "version", json_number(HUB_LOCK_VERSION));
        json_set(root, "installed", json_object());
    }

    /* Build the installed entry JSON object */
    json_t *skill_obj = json_object();
    json_set(skill_obj, "source", json_string(entry->source));
    json_set(skill_obj, "identifier", json_string(entry->identifier));
    json_set(skill_obj, "trust_level", json_string(entry->trust_level));
    json_set(skill_obj, "scan_verdict", json_string(entry->scan_verdict));
    json_set(skill_obj, "content_hash", json_string(entry->content_hash));
    json_set(skill_obj, "install_path", json_string(entry->install_path));
    json_set(skill_obj, "files", json_string(entry->files));
    json_set(skill_obj, "installed_at", json_string(entry->installed_at));
    json_set(skill_obj, "updated_at", json_string(entry->updated_at));

    json_t *installed = json_obj_get(root, "installed");
    if (!installed || installed->type != JSON_OBJECT) {
        json_set(root, "installed", json_object());
        installed = json_obj_get(root, "installed");
    }
    json_set(installed, entry->name, skill_obj);

    bool ok = lock_file_write(root);
    json_free(root);
    return ok;
}

/* Port of Python: HubLockFile.record_uninstall */
bool hub_lock_record_uninstall(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;

    json_t *root = lock_file_read();
    if (!root) return false;

    json_t *installed = json_obj_get(root, "installed");
    if (installed && installed->type == JSON_OBJECT) {
        /* Remove the key by setting a null then freeing */
        json_set(installed, skill_name, json_null());
        /* We need to actually delete the key. json_set doesn't delete.
         * Instead we rebuild without the entry. */
    }

    /* Simplified: rebuild installed object without the entry */
    json_t *old_installed = json_obj_get(root, "installed");
    json_t *new_installed = json_object();
    if (old_installed && old_installed->type == JSON_OBJECT) {
        for (size_t i = 0; i < old_installed->c.count; i++) {
            if (strcmp(old_installed->c.keys[i], skill_name) != 0) {
                json_set(new_installed, old_installed->c.keys[i],
                         json_copy(old_installed->c.items[i]));
            }
        }
    }
    json_set(root, "installed", new_installed);

    bool ok = lock_file_write(root);
    json_free(root);
    return ok;
}

/* Port of Python: HubLockFile.get_installed */
bool hub_lock_get_installed(const char *skill_name, hub_installed_skill_t *out) {
    if (!skill_name || !out) return false;
    memset(out, 0, sizeof(*out));

    json_t *root = lock_file_read();
    if (!root) return false;

    json_t *installed = json_obj_get(root, "installed");
    json_t *entry = installed ? json_obj_get(installed, skill_name) : NULL;

    bool found = false;
    if (entry && entry->type == JSON_OBJECT) {
        snprintf(out->name, sizeof(out->name), "%s", skill_name);
        const char *v;
        v = json_get_str(entry, "source", ""); if (v) snprintf(out->source, sizeof(out->source), "%s", v);
        v = json_get_str(entry, "identifier", ""); if (v) snprintf(out->identifier, sizeof(out->identifier), "%s", v);
        v = json_get_str(entry, "trust_level", ""); if (v) snprintf(out->trust_level, sizeof(out->trust_level), "%s", v);
        v = json_get_str(entry, "scan_verdict", ""); if (v) snprintf(out->scan_verdict, sizeof(out->scan_verdict), "%s", v);
        v = json_get_str(entry, "content_hash", ""); if (v) snprintf(out->content_hash, sizeof(out->content_hash), "%s", v);
        v = json_get_str(entry, "install_path", ""); if (v) snprintf(out->install_path, sizeof(out->install_path), "%s", v);
        v = json_get_str(entry, "files", ""); if (v) snprintf(out->files, sizeof(out->files), "%s", v);
        v = json_get_str(entry, "installed_at", ""); if (v) snprintf(out->installed_at, sizeof(out->installed_at), "%s", v);
        v = json_get_str(entry, "updated_at", ""); if (v) snprintf(out->updated_at, sizeof(out->updated_at), "%s", v);
        found = true;
    }

    json_free(root);
    return found;
}

/* Port of Python: HubLockFile.list_installed */
int hub_lock_list_installed(hub_installed_skill_t *entries, int max_count) {
    if (!entries || max_count <= 0) return 0;

    json_t *root = lock_file_read();
    if (!root) return 0;

    json_t *installed = json_obj_get(root, "installed");
    if (!installed || installed->type != JSON_OBJECT) {
        json_free(root);
        return 0;
    }

    int count = 0;
    for (size_t i = 0; i < installed->c.count && count < max_count; i++) {
        hub_installed_skill_t *e = &entries[count];
        memset(e, 0, sizeof(*e));
        snprintf(e->name, sizeof(e->name), "%s", installed->c.keys[i]);
        json_t *entry = installed->c.items[i];
        if (entry && entry->type == JSON_OBJECT) {
            const char *v;
            v = json_get_str(entry, "source", ""); if (v) snprintf(e->source, sizeof(e->source), "%s", v);
            v = json_get_str(entry, "identifier", ""); if (v) snprintf(e->identifier, sizeof(e->identifier), "%s", v);
            v = json_get_str(entry, "trust_level", ""); if (v) snprintf(e->trust_level, sizeof(e->trust_level), "%s", v);
            v = json_get_str(entry, "scan_verdict", ""); if (v) snprintf(e->scan_verdict, sizeof(e->scan_verdict), "%s", v);
            v = json_get_str(entry, "content_hash", ""); if (v) snprintf(e->content_hash, sizeof(e->content_hash), "%s", v);
            v = json_get_str(entry, "install_path", ""); if (v) snprintf(e->install_path, sizeof(e->install_path), "%s", v);
            v = json_get_str(entry, "files", ""); if (v) snprintf(e->files, sizeof(e->files), "%s", v);
            v = json_get_str(entry, "installed_at", ""); if (v) snprintf(e->installed_at, sizeof(e->installed_at), "%s", v);
            v = json_get_str(entry, "updated_at", ""); if (v) snprintf(e->updated_at, sizeof(e->updated_at), "%s", v);
        }
        count++;
    }

    json_free(root);
    return count;
}

/* ================================================================
 *  v326: Taps management
 * ================================================================ */

/* Port of Python: TapsManager.add */
bool hub_taps_add(const char *repo, const char *path) {
    if (!repo || !repo[0]) return false;
    if (!hub_ensure_dirs()) return false;

    const char *tpath = hub_taps_path();
    if (!tpath) return false;

    /* Read existing taps */
    json_t *root = NULL;
    char *err = NULL;
    root = json_parse_file(tpath, &err);
    if (!root) {
        free(err);
        root = json_object();
        json_set(root, "taps", json_array());
    }

    json_t *taps = json_obj_get(root, "taps");
    if (!taps || taps->type != JSON_ARRAY) {
        json_set(root, "taps", json_array());
        taps = json_obj_get(root, "taps");
    }

    /* Check if already exists */
    size_t n = json_len(taps);
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(taps, i);
        if (t && t->type == JSON_OBJECT) {
            const char *existing_repo = json_get_str(t, "repo", "");
            if (existing_repo && strcmp(existing_repo, repo) == 0) {
                json_free(root);
                return false; /* already exists */
            }
        }
    }

    /* Add new tap */
    json_t *tap = json_object();
    json_set(tap, "repo", json_string(repo));
    json_set(tap, "path", json_string(path ? path : "skills/"));
    json_append(taps, tap);

    /* Write */
    char *serialized = json_serialize_pretty(root, 2);
    bool ok = false;
    if (serialized) {
        FILE *f = fopen(tpath, "w");
        if (f) { fputs(serialized, f); fputc('\n', f); fclose(f); ok = true; }
        free(serialized);
    }
    json_free(root);
    return ok;
}

/* Port of Python: TapsManager.remove */
bool hub_taps_remove(const char *repo) {
    if (!repo || !repo[0]) return false;

    const char *tpath = hub_taps_path();
    if (!tpath) return false;

    char *err = NULL;
    json_t *root = json_parse_file(tpath, &err);
    if (!root) { free(err); return false; }

    json_t *taps = json_obj_get(root, "taps");
    if (!taps || taps->type != JSON_ARRAY) {
        json_free(root);
        return false;
    }

    /* Rebuild array without the matching entry */
    json_t *new_taps = json_array();
    size_t n = json_len(taps);
    bool found = false;
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(taps, i);
        if (t && t->type == JSON_OBJECT) {
            const char *existing_repo = json_get_str(t, "repo", "");
            if (existing_repo && strcmp(existing_repo, repo) == 0) {
                found = true;
                continue; /* skip this one */
            }
        }
        json_append(new_taps, json_copy(t));
    }

    if (!found) { json_free(root); json_free(new_taps); return false; }

    json_set(root, "taps", new_taps);

    char *serialized = json_serialize_pretty(root, 2);
    bool ok = false;
    if (serialized) {
        FILE *f = fopen(tpath, "w");
        if (f) { fputs(serialized, f); fputc('\n', f); fclose(f); ok = true; }
        free(serialized);
    }
    json_free(root);
    return ok;
}

/* Port of Python: TapsManager.list_taps */
int hub_taps_list(hub_tap_entry_t *entries, int max_count) {
    if (!entries || max_count <= 0) return 0;

    const char *tpath = hub_taps_path();
    if (!tpath) return 0;

    char *err = NULL;
    json_t *root = json_parse_file(tpath, &err);
    if (!root) { free(err); return 0; }

    json_t *taps = json_obj_get(root, "taps");
    if (!taps || taps->type != JSON_ARRAY) {
        json_free(root);
        return 0;
    }

    int count = 0;
    size_t n = json_len(taps);
    for (size_t i = 0; i < n && count < max_count; i++) {
        json_t *t = json_get(taps, i);
        if (t && t->type == JSON_OBJECT) {
            const char *repo = json_get_str(t, "repo", "");
            const char *path = json_get_str(t, "path", "skills/");
            if (repo) {
                snprintf(entries[count].repo, sizeof(entries[count].repo), "%s", repo);
                snprintf(entries[count].path, sizeof(entries[count].path), "%s", path ? path : "skills/");
                count++;
            }
        }
    }

    json_free(root);
    return count;
}

/* ================================================================
 *  v326: Audit log
 * ================================================================ */

/* Port of Python: append_audit_log */
bool hub_append_audit_log(const char *action, const char *skill_name,
                           const char *source, const char *trust_level,
                           const char *verdict, const char *extra) {
    if (!action || !skill_name) return false;
    if (!hub_ensure_dirs()) return false;

    const char *apath = hub_audit_path();
    if (!apath) return false;

    /* Format: ISO8601 action skill_name source:trust_level verdict [extra] */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);

    FILE *f = fopen(apath, "a");
    if (!f) return false;

    fprintf(f, "%s %s %s %s:%s %s", timestamp, action,
            skill_name ? skill_name : "",
            source ? source : "unknown",
            trust_level ? trust_level : "unknown",
            verdict ? verdict : "unknown");
    if (extra && extra[0]) {
        fprintf(f, " %s", extra);
    }
    fprintf(f, "\n");
    fclose(f);
    return true;
}

/* Port of Python: _validate_bundle_rel_path */
bool hub_validate_bundle_rel_path(const char *rel_path) {
    if (!rel_path || !rel_path[0]) return false;

    /* Normalize: strip leading/trailing slashes, collapse doubles */
    char norm[4096];
    size_t npos = 0;
    const char *p = rel_path;
    while (*p == '/') p++;
    bool last_was_slash = false;
    for (; *p && npos < sizeof(norm) - 2; p++) {
        if (*p == '/' || *p == '\\') {
            if (!last_was_slash) { norm[npos++] = '/'; last_was_slash = true; }
        } else {
            norm[npos++] = *p;
            last_was_slash = false;
        }
    }
    while (npos > 0 && norm[npos-1] == '/') npos--;
    norm[npos] = '\0';

    if (norm[0] == '\0') return false;

    /* Reject parent-dir traversal */
    for (p = norm; *p; ) {
        /* Check for ".." component */
        if (*p == '.' && *(p+1) == '.' && (*(p+2) == '/' || *(p+2) == '\0'))
            return false;
        /* Skip to next component */
        const char *slash = strchr(p, '/');
        if (!slash) break;
        p = slash + 1;
    }

    return true;
}

/* Port of Python: _is_path_redirect — check if path is a symlink */
bool hub_is_path_redirect(const char *path) {
    if (!path || !path[0]) return false;
    struct stat st;
    if (lstat(path, &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

/* Port of Python: quarantine_bundle (simplified — write single file to quarantine) */
bool hub_quarantine_write(const char *skill_name, const char *filename,
                           const char *content, size_t content_len) {
    if (!skill_name || !skill_name[0] || !filename || !filename[0] || !content)
        return false;
    if (!hub_validate_skill_name(skill_name)) return false;
    if (!hub_validate_bundle_rel_path(filename)) return false;
    if (!hub_ensure_dirs()) return false;

    /* Build quarantine path: <hub_dir>/quarantine/<skill_name>/<filename> */
    const char *hd = hub_dir();
    if (!hd) return false;

    char qdir[HERMES_PATH_MAX];
    snprintf(qdir, sizeof(qdir), "%s/" HUB_QUARANTINE_DIRNAME "/%s", hd, skill_name);
    if (!ensure_dir(qdir)) return false;

    char filepath[HERMES_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", qdir, filename);

    /* Reject path redirects */
    if (hub_is_path_redirect(filepath)) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;
    size_t written = fwrite(content, 1, content_len, f);
    fclose(f);
    return written == content_len;
}

/* Port of Python: bundle_content_hash — compute SHA256 hash of bundle files.
 * Takes an array of {filename, content, content_len} pairs and count.
 * Returns malloc'd "sha256:xxxx..." string, caller must free. */
char *hub_bundle_content_hash(const hub_bundle_file_t *files, size_t count) {
    if (!files || count == 0) return strdup("sha256:0000000000000000");

    /* Concatenate sorted path\x00content pairs into a buffer, then hash */
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += strlen(files[i].filename) + 1 + files[i].content_len;
    }
    if (total > 1048576) total = 1048576; /* cap at 1MB */

    unsigned char *buf = (unsigned char *)malloc(total + 4);
    if (!buf) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        size_t nlen = strlen(files[i].filename);
        memcpy(buf + pos, files[i].filename, nlen);
        pos += nlen;
        buf[pos++] = '\0'; /* null separator */
        size_t clen = files[i].content_len;
        if (pos + clen > total) clen = total - pos;
        memcpy(buf + pos, files[i].content, clen);
        pos += clen;
    }

    char *hex = hash_sha256_hex(buf, pos);
    free(buf);

    if (!hex) return strdup("sha256:0000000000000000");

    /* Format: sha256:<first_16_hex_chars> */
    char *result = (char *)malloc(32);
    if (!result) { free(hex); return NULL; }
    snprintf(result, 32, "sha256:%.16s", hex);
    free(hex);
    return result;
}

/* Port of Python: _skill_meta_to_dict — convert hub_skill_meta_t to JSON object */
json_t *hub_skill_meta_to_json(const hub_skill_meta_t *meta) {
    if (!meta) return NULL;
    json_t *obj = json_object();
    if (!obj) return NULL;

    if (meta->slug[0])       json_set(obj, "slug",        json_string(meta->slug));
    if (meta->name[0])       json_set(obj, "name",        json_string(meta->name));
    if (meta->title[0])      json_set(obj, "title",       json_string(meta->title));
    if (meta->description[0]) json_set(obj, "description", json_string(meta->description));
    if (meta->source_url[0]) json_set(obj, "sourceUrl",   json_string(meta->source_url));
    if (meta->category[0])   json_set(obj, "category",    json_string(meta->category));
    if (meta->tags[0]) {
        /* Parse comma-separated tags into JSON array */
        json_t *tags_arr = json_array();
        char tags_copy[1024];
        snprintf(tags_copy, sizeof(tags_copy), "%s", meta->tags);
        char *tok = strtok(tags_copy, ",");
        while (tok) {
            while (*tok == ' ') tok++;
            json_append(tags_arr, json_string(tok));
            tok = strtok(NULL, ",");
        }
        json_set(obj, "tags", tags_arr);
    }
    if (meta->recommended_method[0])
        json_set(obj, "recommendedMethod", json_string(meta->recommended_method));
    json_set(obj, "installCount", json_number((double)meta->install_count));
    json_set(obj, "proxies", json_bool(meta->needs_proxy));

    return obj;
}
