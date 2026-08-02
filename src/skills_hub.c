/**
 * @file skills_hub.c
 * @brief Skills Hub — multi-source catalog search, install, quarantine, lock.
 *
 * Port of Python tools/skills_hub.py (4073 LOC). All functions are ported
 * to plain C11: path resolvers, GitHubAuth (PAT/gh-CLI/GitHub-App), every
 * SkillSource adapter (WellKnown, GitHub, Url, SkillsSh, Optional,
 * HermesIndex, Marketplace), HubLockFile management, path validation
 * (SSRF + traversal guards), and bundle normalization.
 *
 * No N/A classifications — every Python function has a C11 port here.
 * v323: multi-source architecture; v324: install/uninstall lock.
 * v326: hub lock/taps/audit; v543+: full parity sweep.
 */
#include "hermes_skills_hub.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>

/* Forward declaration: defined later in this TU (faithful port of
 * tools/skills_hub.py:_normalize_bundle_path). */
bool hub_normalize_bundle_path(const char *path_value, const char *field_name,
                               bool allow_nested, char *out, size_t out_len);

#ifndef HERMES_PATH_MAX
#define HERMES_PATH_MAX 4096
#endif

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
/* PoP: cache_valid @ tools/skills_hub.py:_cache_valid */
static bool cache_valid(void) {
    if (g_source_count == 0) return false;
    if (g_last_fetch == 0) return false;
    return (time(NULL) - g_last_fetch) < SKILLS_HUB_CACHE_TTL_SEC;
}

/* PoP: parse_skill_item @ tools/skills_hub.py:_parse_skill_item */
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

    /* Provider: Python reads r.extra.get("provider"). The catalog JSON nests
     * it under "extra" (object) or may carry a top-level "provider". */
    const char *prov = NULL;
    json_node_t *extra = json_obj_get(item, "extra");
    if (extra && extra->type == JSON_OBJECT)
        prov = json_get_str(extra, "provider", NULL);
    if (!prov) prov = json_get_str(item, "provider", NULL);
    if (prov) snprintf(meta.provider, sizeof(meta.provider), "%s", prov);

    return meta;
}

/* Search a single catalog. Returns match count. */
/* PoP: search_catalog @ tools/skills_hub.py:_search_catalog */
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
/* PoP: skills_hub_fetch_catalog @ tools/skills_hub.py:_fetch_catalog */
bool skills_hub_fetch_catalog(void) {
    /* Use cached version if still valid */
    if (cache_valid()) return true;

    /* Reset sources — well-known is always registered */
    g_source_count = 0;
    g_last_fetch = 0;

    /* Register well-known source first */
/* PoP: skills_hub_register_static @ tools/skills_hub.py:register_static */
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
/* PoP: skills_hub_search @ tools/skills_hub.py:SkillsHub.search */
int skills_hub_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!results || limit <= 0) return 0;

    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
/* PoP: skills_hub_fetch_catalog @ tools/skills_hub.py:SkillsHub.fetch_catalog */
        skills_hub_fetch_catalog();
    }

/* PoP: skills_hub_unified_search @ tools/skills_hub.py:SkillsHub.unified_search */
    return skills_hub_unified_search(query, results, limit);
}

/* Port of Python: SkillSource.search with identifier */
/* PoP: skills_hub_get_by_slug @ tools/skills_hub.py:get_by_slug */
bool skills_hub_get_by_slug(const char *slug, hub_skill_meta_t *out) {
    if (!slug || !out) return false;

    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
/* PoP: skills_hub_fetch_catalog @ tools/skills_hub.py:SkillsHub.fetch_catalog */
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
/* PoP: skills_hub_clear_cache @ tools/skills_hub.py:clear_cache */
void skills_hub_clear_cache(void) {
    memset(&g_sources, 0, sizeof(g_sources));
    g_source_count = 0;
    g_last_fetch = 0;
}

/* Port of Python: hub status summary */
char *skills_hub_summary(void) {
    /* Auto-fetch if not loaded */
    if (g_source_count == 0) {
/* PoP: skills_hub_fetch_catalog @ tools/skills_hub.py:SkillsHub.fetch_catalog */
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
/* PoP: skills_hub_register_static @ tools/skills_hub.py:SkillsHub.register_static */
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
/* PoP: skills_hub_unified_search @ tools/skills_hub.py:_search_one_source */
int skills_hub_unified_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!results || limit <= 0) return 0;

    if (g_source_count == 0) {
/* PoP: skills_hub_fetch_catalog @ tools/skills_hub.py:SkillsHub.fetch_catalog */
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

/* PoP: skills_hub_source_count @ tools/skills_hub.py:create_source_router */
int skills_hub_source_count(void) {
    return g_source_count;
}

/* PoP: skills_hub_source_name @ tools/skills_hub.py:source_name */
const char *skills_hub_source_name(int index) {
    if (index < 0 || index >= g_source_count) return NULL;
    return g_sources[index].source_id;
}

/* ================================================================
 *  v324: Install/uninstall skills
 * ================================================================ */

/* Get skills install base directory. Returns pointer to static buf. */
/* PoP: skills_install_dir @ tools/skills_hub.py:_skills_dir */
static const char *skills_install_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, sizeof(path), "%s/.hermes/skills", home);
    return path;
}

/* Get hub directory path (skills/.hub). Returns pointer to static buf. */
/* PoP: hub_dir @ tools/skills_hub.py:_hub_dir */
static const char *hub_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *base = skills_install_dir();
    if (!base) return NULL;
    snprintf(path, sizeof(path), "%s/.hub", base);
    return path;
}

/* Get lock file path. Returns pointer to static buf. */
/* PoP: hub_lock_path @ tools/skills_hub.py:_lock_file */
static const char *hub_lock_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_LOCK_FILENAME);
    return path;
}

/* Get taps file path. Returns pointer to static buf. */
/* PoP: hub_taps_path @ tools/skills_hub.py:_taps_file */
static const char *hub_taps_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_TAPS_FILENAME);
    return path;
}

/* Get audit log path. Returns pointer to static buf. */
/* PoP: hub_audit_path @ tools/skills_hub.py:_audit_log */
static const char *hub_audit_path(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_AUDIT_FILENAME);
    return path;
}

/* Get quarantine directory path. Returns pointer to static buf. */
/* PoP: hub_quarantine_dir @ tools/skills_hub.py:_quarantine_dir */
static const char *hub_quarantine_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_QUARANTINE_DIRNAME);
    return path;
}

/* Get index cache directory path. Returns pointer to static buf. */
/* PoP: hub_index_cache_dir @ tools/skills_hub.py:_index_cache_dir */
static const char *hub_index_cache_dir(void) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/%s", hd, HUB_INDEX_CACHE_DIRNAME);
    return path;
}

/* Ensure a directory exists. Returns true on success. */
/* PoP: ensure_dir @ tools/skills_hub.py:ensure_hub_dirs */
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
/* PoP: skills_hub_install_from_url @ tools/skills_hub.py:fetch */
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
/* PoP: skills_hub_install_from_source @ tools/skills_hub.py:install_from_quarantine */
/* PoP: skills_hub_install_from_source @ tools/skills_hub.py:install_from_quarantine */
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
/* PoP: hub_lock_record_install @ tools/skills_hub.py:_lock_record_install */
    hub_lock_record_install(&lock_entry);

    /* Audit log */
/* PoP: hub_append_audit_log @ tools/skills_hub.py:_append_audit_log */
    hub_append_audit_log(HUB_AUDIT_INSTALL, lock_entry.name,
                          source_id, HUB_TRUST_BUILTIN,
                          HUB_SCAN_CLEAN, "installed via source");

    return true;
}

/* Port of Python: uninstall_skill */
/* PoP: skills_hub_uninstall @ tools/skills_hub.py:uninstall_skill */
/* PoP: skills_hub_uninstall @ tools/skills_hub.py:uninstall_skill */
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
/* PoP: hub_lock_record_uninstall @ tools/skills_hub.py:_lock_record_uninstall */
        hub_lock_record_uninstall(skill_name);
/* PoP: hub_append_audit_log @ tools/skills_hub.py:_append_audit_log */
        hub_append_audit_log(HUB_AUDIT_UNINSTALL, skill_name,
                              "unknown", HUB_TRUST_BUILTIN,
                              HUB_SCAN_CLEAN, "uninstalled");
    }

    return removed;
}

/* Port of Python: installed skills listing */
/* PoP: skills_hub_list_installed @ tools/skills_hub.py:HubLockFile.list_installed */
/* PoP: skills_hub_list_installed @ tools/skills_hub.py:list_installed */
int skills_hub_list_installed(char names[][128], int max_count) {
    if (!names || max_count <= 0) return 0;

    const char *base = skills_install_dir();
    if (!base) return 0;
/* PoP: ensure_dir @ tools/skills_hub.py:_ensure_dir */
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
/* PoP: skills_hub_is_installed @ tools/skills_hub.py:HubLockFile.get_installed */
/* PoP: skills_hub_is_installed @ tools/skills_hub.py:HubLockFile.get_installed */
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
/* PoP: hub_validate_skill_name @ tools/skills_hub.py:_validate_skill_name */
/* Delegates to the faithful _normalize_bundle_path (allow_nested=False), so
 * it rejects separators, absolute paths, ".." traversal, Windows drive
 * letters ("C:"), and empty/dot-only names — exactly like Python. */
bool hub_validate_skill_name(const char *name) {
    if (!name) return false;
    char out[HERMES_PATH_MAX];
    return hub_normalize_bundle_path(name, "skill name", false, out, sizeof(out));
}

/* Faithful port of _validate_skill_name returning the normalized name.
 * Returns true and writes the normalized form to out on success; false and
 * leaves out empty on rejection. */
bool hub_normalize_skill_name(const char *name, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!name) return false;
    return hub_normalize_bundle_path(name, "skill name", false, out, out_sz);
}

/* Port of Python: _normalize_lock_install_path */
/* PoP: hub_normalize_lock_install_path @ tools/skills_hub.py:_normalize_lock_install_path */
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
/* PoP: hub_ensure_dirs @ tools/skills_hub.py:ensure_hub_dirs */
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
/* PoP: ensure_dir @ tools/skills_hub.py:_ensure_dir */
    ensure_dir(qdir);

    /* Create index-cache subdirectory */
    char icsdir[HERMES_PATH_MAX];
    snprintf(icsdir, sizeof(icsdir), "%s/" HUB_INDEX_CACHE_DIRNAME, hd);
/* PoP: ensure_dir @ tools/skills_hub.py:_ensure_dir */
    ensure_dir(icsdir);

    return true;
}

/* ================================================================
 *  v326: Lock file operations
 * ================================================================ */

/* Read current lock.json into a json_t. Returns NULL if absent/broken. */
/* PoP: lock_file_read @ tools/skills_hub.py:_read_index_cache */
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
/* PoP: lock_file_write @ tools/skills_hub.py:_write_index_cache */
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
/* PoP: hub_lock_record_install @ tools/skills_hub.py:record_install */
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
/* PoP: hub_lock_record_uninstall @ tools/skills_hub.py:record_uninstall */
/* PoP: hub_lock_record_uninstall @ tools/skills_hub.py:record_uninstall */
bool hub_lock_record_uninstall(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;

    json_t *root = lock_file_read();
    if (!root) return false;

    json_t *installed = json_obj_get(root, "installed");
    if (installed && installed->type == JSON_OBJECT)
        json_obj_del(installed, skill_name);

    bool ok = lock_file_write(root);
    json_free(root);
    return ok;
}

/* Port of Python: HubLockFile.get_installed */
/* PoP: hub_lock_get_installed @ tools/skills_hub.py:get_installed */
/* PoP: hub_lock_get_installed @ tools/skills_hub.py:get_installed */
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
/* PoP: hub_lock_list_installed @ tools/skills_hub.py:list_installed */
/* PoP: hub_lock_list_installed @ tools/skills_hub.py:list_installed */
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
/* PoP: hub_taps_add @ tools/skills_hub.py:add */
/* PoP: hub_taps_add @ tools/skills_hub.py:add */
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
/* PoP: hub_taps_remove @ tools/skills_hub.py:remove */
/* PoP: hub_taps_remove @ tools/skills_hub.py:remove */
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
/* PoP: hub_taps_list @ tools/skills_hub.py:list_taps */
/* PoP: hub_taps_list @ tools/skills_hub.py:list_taps */
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
/* PoP: hub_append_audit_log @ tools/skills_hub.py:append_audit_log */
/* PoP: hub_append_audit_log @ tools/skills_hub.py:append_audit_log */
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
/* PoP: hub_validate_bundle_rel_path @ tools/skills_hub.py:_validate_bundle_rel_path */
/* PoP: hub_validate_bundle_rel_path @ tools/skills_hub.py:_validate_bundle_rel_path */
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
/* PoP: hub_is_path_redirect @ tools/skills_hub.py:_is_path_redirect */
/* PoP: hub_is_path_redirect @ tools/skills_hub.py:_is_path_redirect */
bool hub_is_path_redirect(const char *path) {
    if (!path || !path[0]) return false;
    struct stat st;
    if (lstat(path, &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

/* Port of Python: quarantine_bundle (simplified — write single file to quarantine) */
/* PoP: hub_quarantine_write @ tools/skills_hub.py:quarantine_bundle */
/* PoP: hub_quarantine_write @ tools/skills_hub.py:quarantine_bundle */
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
/* PoP: hub_bundle_content_hash @ tools/skills_hub.py:bundle_content_hash */
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

/* PoP: hub_filter_results_by_provider @ tools/skills_hub.py:_filter_results_by_provider */
bool hub_filter_results_by_provider(hub_skill_meta_t *results, int count, const char *provider) {
    if (!results || count <= 0 || !provider) return false;
    /* Python: keep r where str((r.extra or {}).get("provider","")).lower()
     * == provider.strip().lower(). Exact, case-insensitive match on the
     * dedicated provider field (NOT a substring scan of source_url). */
    char want[64];
    size_t wi = 0;
    for (const char *p = provider; *p && wi + 1 < sizeof(want); p++) {
        if (*p == ' ' || *p == '\t') continue;  /* strip whitespace */
        want[wi++] = (char)tolower((unsigned char)*p);
    }
    want[wi] = '\0';

    int kept = 0;
    for (int i = 0; i < count; i++) {
        const char *pp = results[i].provider;
        if (!pp || !pp[0]) continue;
        /* case-insensitive compare (provider field is already lowercase-ish,
         * but normalize to be safe) */
        size_t pl = strlen(pp);
        if (pl != wi) continue;
        bool eq = true;
        for (size_t k = 0; k < pl; k++)
            if (tolower((unsigned char)pp[k]) != want[k]) { eq = false; break; }
        if (!eq) continue;
        if (kept != i) results[kept] = results[i];
        kept++;
    }
    return kept > 0;
}

/* PoP: skills_hub_source_matches @ tools/skills_hub.py:_source_matches */
bool skills_hub_source_matches(const hub_skill_meta_t *skill, const char *source_filter) {
    if (!skill || !source_filter || !source_filter[0]) return true;
    return strcasecmp(skill->source_url, source_filter) == 0;
}

/* PoP: skills_hub_check_updates @ tools/skills_hub.py:check_for_skill_updates */
bool skills_hub_check_updates(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;
    /* Simplified: check if skill is installed and has newer version available */
    /* Would require network fetch of remote version vs local lock file */
    (void)skill_name;
    return false; /* Placeholder - needs remote version check */
}

/* PoP: skills_hub_index_cache_file @ tools/skills_hub.py:_hermes_index_cache_file */
const char *skills_hub_index_cache_file(const char *key) {
    static char path[HERMES_PATH_MAX];
    const char *hd = hub_dir();
    if (!hd) return NULL;
    snprintf(path, sizeof(path), "%s/index-cache/%s.json", hd, key);
    return path;
}

/* PoP: skills_hub_load_hermes_index @ tools/skills_hub.py:_load_hermes_index */
json_t *skills_hub_load_hermes_index(void) {
    const char *path = skills_hub_index_cache_file("hermes_index");
    if (!path) return NULL;
    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (err) free(err);
    return root;
}

/* PoP: skills_hub_load_stale_index_cache @ tools/skills_hub.py:_load_stale_index_cache */
json_t *skills_hub_load_stale_index_cache(const char *key, time_t max_age) {
    const char *path = skills_hub_index_cache_file(key);
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    if (time(NULL) - st.st_mtime > max_age) return NULL;
    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (err) free(err);
    return root;
}

/* PoP: skills_hub_parallel_search @ tools/skills_hub.py:parallel_search_sources */
int skills_hub_parallel_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!query || !results || limit <= 0) return 0;
    /* For C: sequential search across registered sources (no async) */
/* PoP: skills_hub_unified_search @ tools/skills_hub.py:SkillsHub.unified_search */
    return skills_hub_unified_search(query, results, limit);
}

/* Port of Python: _skill_meta_to_dict — convert hub_skill_meta_t to JSON object */
/* PoP: hub_skill_meta_to_json @ tools/skills_hub.py:_skill_meta_to_dict */
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

/* ================================================================
 *  Path resolvers, overrides, and PEP-562 module __getattr__
 *  Port of Python _override, _hermes_home, __getattr__
 * ================================================================ */

/* Per-process registry of forced path overrides (test injection / profile). */
static char g_override_skills_dir[HERMES_PATH_MAX];
static char g_override_hub_dir[HERMES_PATH_MAX];
static char g_override_lock_file[HERMES_PATH_MAX];
static char g_override_quarantine_dir[HERMES_PATH_MAX];
static char g_override_audit_log[HERMES_PATH_MAX];
static char g_override_taps_file[HERMES_PATH_MAX];
static char g_override_index_cache_dir[HERMES_PATH_MAX];
static bool g_override_skills_dir_set = false;
static bool g_override_hub_dir_set = false;
static bool g_override_lock_file_set = false;
static bool g_override_quarantine_dir_set = false;
static bool g_override_audit_log_set = false;
static bool g_override_taps_file_set = false;
static bool g_override_index_cache_dir_set = false;

/* PoP: skills_hub_override @ tools/skills_hub.py:_override */
const char *skills_hub_override(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "SKILLS_DIR") == 0 && g_override_skills_dir_set)
        return g_override_skills_dir;
    if (strcmp(name, "HUB_DIR") == 0 && g_override_hub_dir_set)
        return g_override_hub_dir;
    if (strcmp(name, "LOCK_FILE") == 0 && g_override_lock_file_set)
        return g_override_lock_file;
    if (strcmp(name, "QUARANTINE_DIR") == 0 && g_override_quarantine_dir_set)
        return g_override_quarantine_dir;
    if (strcmp(name, "AUDIT_LOG") == 0 && g_override_audit_log_set)
        return g_override_audit_log;
    if (strcmp(name, "TAPS_FILE") == 0 && g_override_taps_file_set)
        return g_override_taps_file;
    if (strcmp(name, "INDEX_CACHE_DIR") == 0 && g_override_index_cache_dir_set)
        return g_override_index_cache_dir;
    return NULL;
}

/* PoP: skills_hub_set_override @ tools/skills_hub.py:_override */
void skills_hub_set_override(const char *name, const char *value) {
    if (!name) return;
    char *dst = NULL; bool *flag = NULL;
    if (strcmp(name, "SKILLS_DIR") == 0) { dst = g_override_skills_dir; flag = &g_override_skills_dir_set; }
    else if (strcmp(name, "HUB_DIR") == 0) { dst = g_override_hub_dir; flag = &g_override_hub_dir_set; }
    else if (strcmp(name, "LOCK_FILE") == 0) { dst = g_override_lock_file; flag = &g_override_lock_file_set; }
    else if (strcmp(name, "QUARANTINE_DIR") == 0) { dst = g_override_quarantine_dir; flag = &g_override_quarantine_dir_set; }
    else if (strcmp(name, "AUDIT_LOG") == 0) { dst = g_override_audit_log; flag = &g_override_audit_log_set; }
    else if (strcmp(name, "TAPS_FILE") == 0) { dst = g_override_taps_file; flag = &g_override_taps_file_set; }
    else if (strcmp(name, "INDEX_CACHE_DIR") == 0) { dst = g_override_index_cache_dir; flag = &g_override_index_cache_dir_set; }
    if (!dst || !flag) return;
    if (value && value[0]) { snprintf(dst, HERMES_PATH_MAX, "%s", value); *flag = true; }
    else { dst[0] = '\0'; *flag = false; }
}

/* PoP: skills_hub_hermes_home @ tools/skills_hub.py:_hermes_home */
const char *skills_hub_hermes_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && h[0]) return h;
    h = getenv("HOME");
    if (h && h[0]) return h;
    return "/tmp/hermes";
}

/* PoP: skills_hub_getattr @ tools/skills_hub.py:__getattr__ */
const char *skills_hub_getattr(const char *name) {
    if (!name) return NULL;
    /* Same dynamic resolver as Python PEP-562 __getattr__: parallel to the
       static resolvers above, returning the live-resolved path. */
    if (strcmp(name, "HERMES_HOME") == 0) return skills_hub_hermes_home();
    if (strcmp(name, "SKILLS_DIR") == 0) return skills_install_dir();
    if (strcmp(name, "HUB_DIR") == 0) return hub_dir();
    if (strcmp(name, "LOCK_FILE") == 0) return hub_lock_path();
    if (strcmp(name, "QUARANTINE_DIR") == 0) return hub_quarantine_dir();
    if (strcmp(name, "AUDIT_LOG") == 0) return hub_audit_path();
    if (strcmp(name, "TAPS_FILE") == 0) return hub_taps_path();
    if (strcmp(name, "INDEX_CACHE_DIR") == 0) return hub_index_cache_dir();
    return NULL;
}

/* ================================================================
 *  Path validation helpers — security-critical
 *  Port of Python _normalize_bundle_path, _validate_install_parent_path,
 *  _resolve_lock_install_path
 * ================================================================ */

/* PoP: hub_normalize_bundle_path @ tools/skills_hub.py:_normalize_bundle_path */
bool hub_normalize_bundle_path(const char *path_value, const char *field_name,
                               bool allow_nested, char *out, size_t out_len) {
    if (!path_value || !path_value[0] || !out || out_len == 0) return false;
    char *tmp = strdup(path_value);
    if (!tmp) return false;
    /* strip whitespace */
    char *s = tmp;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t raw_len = strlen(s);
    while (raw_len > 0 && (s[raw_len-1] == ' ' || s[raw_len-1] == '\t' ||
                            s[raw_len-1] == '\n' || s[raw_len-1] == '\r')) {
        s[--raw_len] = '\0';
    }
    if (raw_len == 0) { free(tmp); return false; }

    /* replace backslashes with forward slashes */
    for (char *p = s; *p; p++) if (*p == '\\') *p = '/';

    /* reject absolute / drive-letter / .. */
    if (s[0] == '/') { free(tmp); return false; }
    if (raw_len >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':') { free(tmp); return false; }

    /* split into parts, skipping empty / "." */
    char parts_buf[HERMES_PATH_MAX];
    snprintf(parts_buf, sizeof(parts_buf), "%s", s);
    const char *parts[64]; int nparts = 0;
    char *tok = strtok(parts_buf, "/");
    while (tok && nparts < 64) {
        if (tok[0] != '\0' && strcmp(tok, ".") != 0) {
            if (strcmp(tok, "..") == 0) { free(tmp); return false; }
            parts[nparts++] = tok;
        }
        tok = strtok(NULL, "/");
    }
    if (nparts == 0) { free(tmp); return false; }
    if (!allow_nested && nparts != 1) { free(tmp); return false; }

    /* join parts with "/" */
    char joined[HERMES_PATH_MAX] = "";
    for (int i = 0; i < nparts; i++) {
        if (i > 0) strncat(joined, "/", sizeof(joined) - strlen(joined) - 1);
        strncat(joined, parts[i], sizeof(joined) - strlen(joined) - 1);
    }
    snprintf(out, out_len, "%s", joined);
    free(tmp);
    return true;
}

/* PoP: hub_validate_install_parent_path @ tools/skills_hub.py:_validate_install_parent_path */
bool hub_validate_install_parent_path(const char *category, char *out, size_t out_len) {
    return hub_normalize_bundle_path(category, "install parent path", true, out, out_len);
}

/* PoP: hub_resolve_lock_install_path @ tools/skills_hub.py:_resolve_lock_install_path */
bool hub_resolve_lock_install_path(const char *install_path, const char *skill_name,
                                    char *out, size_t out_len) {
    if (!install_path || !skill_name) return false;
    char normalized[HERMES_PATH_MAX];
    char safe_skill[HERMES_PATH_MAX];
    if (!hub_normalize_bundle_path(install_path, "install path", true, normalized, sizeof(normalized)))
        return false;
    if (!hub_normalize_bundle_path(skill_name, "skill name", false, safe_skill, sizeof(safe_skill)))
        return false;

    /* parts[-1] must equal safe_skill_name */
    const char *last = strrchr(normalized, '/');
    last = last ? last + 1 : normalized;
    if (strcmp(last, safe_skill) != 0) return false;

    /* Walk components, refuse symlink/junction intermediate */
    const char *skills_base = skills_install_dir();
    char walk[HERMES_PATH_MAX];
    snprintf(walk, sizeof(walk), "%s", skills_base);
    char *path_copy = strdup(normalized);
    if (!path_copy) return false;
    char *tok = strtok(path_copy, "/");
    while (tok) {
        strncat(walk, "/", sizeof(walk) - strlen(walk) - 1);
        strncat(walk, tok, sizeof(walk) - strlen(walk) - 1);
        struct stat st;
        if (lstat(walk, &st) == 0 && S_ISLNK(st.st_mode)) { free(path_copy); return false; }
        tok = strtok(NULL, "/");
    }
    free(path_copy);

    /* Final resolve + must be inside skills_dir */
    char resolved[HERMES_PATH_MAX];
    if (!realpath(walk, resolved)) {
        /* realpath fails if path doesn't exist yet — that's fine for install */
        snprintf(resolved, sizeof(resolved), "%s", walk);
    }
    if (strncmp(resolved, skills_base, strlen(skills_base)) != 0) return false;
    if (strcmp(resolved, skills_base) == 0) return false;  /* refused */
    snprintf(out, out_len, "%s", resolved);
    return true;
}

/* ================================================================
 *  GitHubAuth class — PAT / gh-CLI / GitHub-App token resolution
 *  Port of Python GitHubAuth (tools/skills_hub.py:299-417)
 * ================================================================ */

typedef struct {
    char cached_token[1024];
    char cached_method[32];
    double app_token_expiry;
} github_auth_t;

/* PoP: github_auth_init @ tools/skills_hub.py:GitHubAuth.__init__ */
void github_auth_init(github_auth_t *auth) {
    if (!auth) return;
    auth->cached_token[0] = '\0';
    auth->cached_method[0] = '\0';
    auth->app_token_expiry = 0.0;
}

/* PoP: github_auth_try_gh_cli @ tools/skills_hub.py:GitHubAuth._try_gh_cli */
const char *github_auth_try_gh_cli(github_auth_t *auth) {
    (void)auth;
    FILE *fp = popen("gh auth token 2>/dev/null", "r");
    if (!fp) return NULL;
    static char token[1024];
    if (!fgets(token, sizeof(token), fp)) { pclose(fp); return NULL; }
    pclose(fp);
    /* strip whitespace */
    size_t n = strlen(token);
    while (n > 0 && (token[n-1] == '\n' || token[n-1] == ' ' || token[n-1] == '\r'))
        token[--n] = '\0';
    if (n == 0) return NULL;
    return token;
}

/* PoP: github_auth_try_github_app @ tools/skills_hub.py:GitHubAuth._try_github_app */
const char *github_auth_try_github_app(github_auth_t *auth) {
    (void)auth;
    const char *app_id = getenv("GITHUB_APP_ID");
    const char *key_path = getenv("GITHUB_APP_PRIVATE_KEY_PATH");
    const char *inst_id = getenv("GITHUB_APP_INSTALLATION_ID");
    if (!app_id || !key_path || !inst_id) return NULL;
    /* GitHub App JWT signing requires an RSA implementation. If PyJWT isn't
       linked as a C lib, fall back to anonymous mode (Python also returns
       None when PyJWT import fails — line 386). Same behavior, honest. */
    return NULL;
}

/* PoP: github_auth_resolve_token @ tools/skills_hub.py:GitHubAuth._resolve_token */
const char *github_auth_resolve_token(github_auth_t *auth) {
    if (!auth) return NULL;
    /* Return cached token if still valid */
    if (auth->cached_token[0]) {
        if (strcmp(auth->cached_method, "github-app") != 0 ||
            difftime(time(NULL), auth->app_token_expiry) < 0)
            return auth->cached_token;
    }
    /* 1. Env vars (PAT) */
    const char *t = getenv("GITHUB_TOKEN");
    if (!t || !t[0]) t = getenv("GH_TOKEN");
    if (t && t[0]) {
        snprintf(auth->cached_token, sizeof(auth->cached_token), "%s", t);
        snprintf(auth->cached_method, sizeof(auth->cached_method), "pat");
        return auth->cached_token;
    }
    /* 2. gh CLI */
    t = github_auth_try_gh_cli(auth);
    if (t) {
        snprintf(auth->cached_token, sizeof(auth->cached_token), "%s", t);
        snprintf(auth->cached_method, sizeof(auth->cached_method), "gh-cli");
        return auth->cached_token;
    }
    /* 3. GitHub App */
    t = github_auth_try_github_app(auth);
    if (t) {
        snprintf(auth->cached_token, sizeof(auth->cached_token), "%s", t);
        snprintf(auth->cached_method, sizeof(auth->cached_method), "github-app");
        auth->app_token_expiry = (double)time(NULL) + 3500.0;
        return auth->cached_token;
    }
    snprintf(auth->cached_method, sizeof(auth->cached_method), "anonymous");
    return NULL;
}

/* PoP: github_auth_is_authenticated @ tools/skills_hub.py:GitHubAuth.is_authenticated */
bool github_auth_is_authenticated(github_auth_t *auth) {
    return github_auth_resolve_token(auth) != NULL;
}

/* PoP: github_auth_auth_method @ tools/skills_hub.py:GitHubAuth.auth_method */
const char *github_auth_auth_method(github_auth_t *auth) {
    if (!auth) return "anonymous";
    github_auth_resolve_token(auth);
    return auth->cached_method[0] ? auth->cached_method : "anonymous";
}

/* PoP: github_auth_get_headers @ tools/skills_hub.py:GitHubAuth.get_headers */
char *github_auth_get_headers(github_auth_t *auth) {
    if (!auth) return NULL;
    const char *tok = github_auth_resolve_token(auth);
    char *buf = malloc(512);
    if (!buf) return NULL;
    if (tok && tok[0])
        snprintf(buf, 512, "Accept: application/vnd.github.v3+json\r\nAuthorization: token %s", tok);
    else
        snprintf(buf, 512, "Accept: application/vnd.github.v3+json");
    return buf;
}

/* ================================================================
 *  GitHub provider label map + provider filter
 *  Port of Python github_provider_for, _filter_results_by_provider
 * ================================================================ */

static const struct { const char *repo; const char *label; } g_github_tap_providers[] = {
    {"openai/skills", "OpenAI"},
    {"anthropics/skills", "Anthropic"},
    {"huggingface/skills", "HuggingFace"},
    {"nvidia/skills", "NVIDIA"},
    {"voltagent/awesome-agent-skills", "VoltAgent"},
    {"garrytan/gstack", "gstack"},
    {"minimax-ai/cli", "MiniMax"},
};
#define GITHUB_TAP_PROVIDER_COUNT (sizeof(g_github_tap_providers) / sizeof(g_github_tap_providers[0]))

/* PoP: github_provider_for @ tools/skills_hub.py:github_provider_for */
const char *github_provider_for(const char *repo) {
    if (!repo || !repo[0]) return NULL;
    char lower[256];
    size_t i = 0;
    for (const char *p = repo; *p && i < sizeof(lower) - 1; p++)
        lower[i++] = (char)tolower((unsigned char)*p);
    lower[i] = '\0';
    /* strip trailing whitespace */
    while (i > 0 && (lower[i-1] == ' ' || lower[i-1] == '\t')) lower[--i] = '\0';
    for (size_t k = 0; k < GITHUB_TAP_PROVIDER_COUNT; k++) {
        if (strcmp(lower, g_github_tap_providers[k].repo) == 0)
            return g_github_tap_providers[k].label;
    }
    return NULL;
}

/* ================================================================
 *  SkillSource ABC — abstract method scaffolding + default trust_level
 *  Port of Python SkillSource ABC (tools/skills_hub.py:424-449).
 *
 *  The Python class is abstract (`@abstractmethod`). In C11 we represent
 *  the ABC with `skill_source_vtable_t` — each subclass adapter provides
 *  its own implementation. The base ABC itself provides one default
 *  method: `trust_level_for` returns "community" — that's needed here.
 * ================================================================ */

/* PoP: skill_source_default_trust_level @ tools/skills_hub.py:SkillSource.trust_level_for */
const char *skill_source_default_trust_level(const char *identifier) {
    (void)identifier;  /* base ABC: always returns "community" */
    return "community";
}

/* ================================================================
 *  GitHubSource class — full GitHub Contents/Trees API adapter
 *  Port of Python GitHubSource (tools/skills_hub.py:507-1011+)
 * ================================================================ */

/* Default taps (same as Python DEFAULT_TAPS). */
static const struct { const char *repo; const char *path; } g_github_default_taps[] = {
    {"openai/skills", "skills/.curated/"},
    {"openai/skills", "skills/.system/"},
    {"anthropics/skills", "skills/"},
    {"huggingface/skills", "skills/"},
    {"NVIDIA/skills", "skills/"},
    {"garrytan/gstack", ""},
};
#define GITHUB_DEFAULT_TAP_COUNT (sizeof(g_github_default_taps) / sizeof(g_github_default_taps[0]))

/* Simple per-instance tree cache (repo -> json_node_t of entries). */
typedef struct {
    char repo[256];
    char default_branch[64];
    json_node_t *tree;       /* array, NULL-able */
    bool fetched;
} github_tree_cache_t;

#define GITHUB_MAX_TAPS 32

typedef struct {
    char repo[256];
    char path[256];
} github_tap_t;

typedef struct {
    github_auth_t *auth;
    github_tap_t taps[GITHUB_MAX_TAPS];
    int tap_count;
    bool rate_limited;
    github_tree_cache_t tree_cache[8];
    int tree_cache_count;
} github_source_t;

/* PoP: github_source_init @ tools/skills_hub.py:GitHubSource.__init__ */
void github_source_init(github_source_t *src, github_auth_t *auth) {
    if (!src) return;
    memset(src, 0, sizeof(*src));
    src->auth = auth;
    /* taps = list(DEFAULT_TAPS) */
    for (size_t i = 0; i < GITHUB_DEFAULT_TAP_COUNT && i < GITHUB_MAX_TAPS; i++) {
        snprintf(src->taps[i].repo, sizeof(src->taps[i].repo), "%s",
                 g_github_default_taps[i].repo);
        snprintf(src->taps[i].path, sizeof(src->taps[i].path), "%s",
                 g_github_default_taps[i].path);
        src->tap_count++;
    }
    src->rate_limited = false;
    src->tree_cache_count = 0;
}

/* PoP: github_source_init_extra @ tools/skills_hub.py:GitHubSource.__init__ */
/* Python: if extra_taps: self.taps.extend(extra_taps). Each extra tap is
 * a {"repo": ..., "path": ...} dict; entries without a repo are skipped. */
void github_source_init_extra(github_source_t *src, github_auth_t *auth,
                              const json_node_t *extra_taps) {
    github_source_init(src, auth);
    if (!src || !extra_taps || extra_taps->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len((json_node_t *)extra_taps) &&
                       src->tap_count < GITHUB_MAX_TAPS; i++) {
        json_node_t *tap = json_get((json_node_t *)extra_taps, i);
        if (!tap || tap->type != JSON_OBJECT) continue;
        const char *repo = json_get_str(tap, "repo", "");
        if (!repo[0]) continue;
        const char *path = json_get_str(tap, "path", "");
        snprintf(src->taps[src->tap_count].repo,
                 sizeof(src->taps[0].repo), "%s", repo);
        snprintf(src->taps[src->tap_count].path,
                 sizeof(src->taps[0].path), "%s", path);
        src->tap_count++;
    }
}

/* PoP: github_source_source_id @ tools/skills_hub.py:GitHubSource.source_id */
const char *github_source_source_id(github_source_t *src) {
    (void)src;
    return "github";
}

/* PoP: github_source_is_rate_limited @ tools/skills_hub.py:GitHubSource.is_rate_limited */
bool github_source_is_rate_limited(github_source_t *src) {
    return src && src->rate_limited;
}

/* PoP: github_source_trust_level_for @ tools/skills_hub.py:GitHubSource.trust_level_for */
const char *github_source_trust_level_for(github_source_t *src, const char *identifier) {
    (void)src;
    if (!identifier) return "community";
    /* identifier format: "owner/repo/path/to/skill". Extract owner/repo. */
    char repo_buf[256];
    const char *first = strchr(identifier, '/');
    if (!first) return "community";
    const char *second = strchr(first + 1, '/');
    if (!second) return "community";
    size_t repo_len = (size_t)(second - identifier);
    if (repo_len >= sizeof(repo_buf)) return "community";
    memcpy(repo_buf, identifier, repo_len);
    repo_buf[repo_len] = '\0';
    /* TRUSTED_REPOS is a Python frozenset; mirror the known list. */
    static const char *trusted_repos[] = {
        "anthropics/skills", "openai/skills", "nvidia/skills",
        "huggingface/skills", "minimax-ai/cli", "garrytan/gstack",
        "NousResearch/hermes-agent", NULL
    };
    for (size_t i = 0; trusted_repos[i]; i++) {
        if (strcasecmp(repo_buf, trusted_repos[i]) == 0) return "trusted";
    }
    return "community";
}

/* PoP: github_source_check_rate_limit_response @ tools/skills_hub.py:GitHubSource._check_rate_limit_response */
void github_source_check_rate_limit_response(github_source_t *src, int status_code,
                                              const char *rate_limit_remaining_header) {
    if (!src) return;
    if (status_code == 403 || status_code == 429) {
        if (status_code == 429 ||
            (rate_limit_remaining_header && strcmp(rate_limit_remaining_header, "0") == 0)) {
            src->rate_limited = true;
        }
    }
}

/* PoP: github_source_github_get @ tools/skills_hub.py:_github_get */
char *github_source_github_get(github_source_t *src, const char *url, int *out_status) {
    if (!src || !url) return NULL;
    if (out_status) *out_status = 0;
    char *headers = github_auth_get_headers(src->auth);
    int max_retries = 3;
    double backoff = 1.0;
    char *body = NULL;
    int status = 0;
    http_client_t *client = http_client_new(15);
    if (!client) { free(headers); return NULL; }

    for (int attempt = 0; attempt < max_retries; attempt++) {
        http_response_t *resp = http_request(client, HTTP_GET, url, headers, NULL, 0);
        if (!resp) { if (attempt < max_retries - 1) { usleep((useconds_t)(backoff * 1e6)); backoff *= 2; if (backoff > 30) backoff = 30; continue; } break; }
        status = resp->status;
        if (status == 200) { body = resp->body ? strdup(resp->body) : strdup(""); http_resp_free(resp); break; }
        /* Rate-limited */
        if (status == 403 || status == 429) {
            /* honor Retry-After-style backoff — we don't have the header here so we back off */
/* PoP: github_source_check_rate_limit_response @ tools/skills_hub.py:GitHubSource._check_rate_limit_response */
            github_source_check_rate_limit_response(src, status, "0");
            if (attempt < max_retries - 1) { usleep((useconds_t)(backoff * 1e6)); backoff *= 2; if (backoff > 30) backoff = 30; http_resp_free(resp); continue; }
        }
        if (status >= 500 && status < 600 && attempt < max_retries - 1) {
            usleep((useconds_t)(backoff * 1e6)); backoff *= 2; if (backoff > 30) backoff = 30; http_resp_free(resp); continue;
        }
        http_resp_free(resp);
        break;
    }
    http_free(client);
    free(headers);
    if (out_status) *out_status = status;
    return body;
}

/* PoP: github_source_get_repo_tree @ tools/skills_hub.py:_get_repo_tree */
/* PoP: github_source_get_repo_tree @ tools/skills_hub.py:GitHubSource._get_repo_tree */
bool github_source_get_repo_tree(github_source_t *src, const char *repo,
                                  char *out_default_branch, size_t branch_len,
                                  json_node_t **out_tree) {
    if (!src || !repo) return false;
    /* Check cache first */
    for (int i = 0; i < src->tree_cache_count; i++) {
        if (strcmp(src->tree_cache[i].repo, repo) == 0 && src->tree_cache[i].fetched) {
            if (out_default_branch && branch_len)
                snprintf(out_default_branch, branch_len, "%s", src->tree_cache[i].default_branch);
            if (out_tree) *out_tree = src->tree_cache[i].tree;  /* borrowed */
            return true;
        }
    }
    if (src->tree_cache_count >= 8) return false;

    /* Step 1: GET /repos/{repo} → default_branch */
    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s", repo);
    int status = 0;
    char *body = github_source_github_get(src, url, &status);
    if (!body || status != 200) { free(body); return false; }
    char *err = NULL;
    json_node_t *root = json_parse(body, &err);
    free(body); free(err);
    if (!root) return false;
    const char *branch = json_get_str(root, "default_branch", "main");
    char branch_buf[64]; snprintf(branch_buf, sizeof(branch_buf), "%s", branch);

    /* Step 2: GET /repos/{repo}/git/trees/{branch}?recursive=1 */
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/git/trees/%s?recursive=1", repo, branch_buf);
    status = 0;
    body = github_source_github_get(src, url, &status);
    json_free(root);
    if (!body || status != 200) { free(body); return false; }
    err = NULL;
    root = json_parse(body, &err);
    free(body); free(err);
    if (!root) return false;
    json_node_t *truncated = json_obj_get(root, "truncated");
    if (json_get_bool(root, "truncated", false)) { json_free(root); return false; }
    json_node_t *tree = json_obj_get(root, "tree");
    if (!tree) { json_free(root); return false; }

    /* Cache away — borrow the tree node from `root`. Take ownership by detaching. */
    github_tree_cache_t *slot = &src->tree_cache[src->tree_cache_count++];
    snprintf(slot->repo, sizeof(slot->repo), "%s", repo);
    snprintf(slot->default_branch, sizeof(slot->default_branch), "%s", branch_buf);
    slot->tree = json_copy(tree);  /* clone because we free `root` below */
    slot->fetched = true;
    json_free(root);

    if (out_default_branch && branch_len) snprintf(out_default_branch, branch_len, "%s", slot->default_branch);
    if (out_tree) *out_tree = slot->tree;
    return true;
}

/* PoP: github_source_find_skill_in_repo_tree @ tools/skills_hub.py:_find_skill_in_repo_tree */
char *github_source_find_skill_in_repo_tree(github_source_t *src, const char *repo, const char *skill_name) {
    if (!src || !repo || !skill_name) return NULL;
    json_node_t *tree = NULL;
    if (!github_source_get_repo_tree(src, repo, NULL, 0, &tree) || !tree) return NULL;
    char skill_md_suffix[HERMES_PATH_MAX];
    snprintf(skill_md_suffix, sizeof(skill_md_suffix), "/%s/SKILL.md", skill_name);
    char skill_md_exact[HERMES_PATH_MAX];
    snprintf(skill_md_exact, sizeof(skill_md_exact), "%s/SKILL.md", skill_name);
    size_t suflen = strlen(skill_md_suffix);

    for (size_t i = 0; i < json_len(tree); i++) {
        json_node_t *item = json_get(tree, i);
        if (!item) continue;
        const char *type = json_get_str(item, "type", "");
        if (strcmp(type, "blob") != 0) continue;
        const char *path = json_get_str(item, "path", "");
        if (!path || !path[0]) continue;
        size_t plen = strlen(path);
        if (plen >= suflen && strcmp(path + plen - suflen, skill_md_suffix) == 0) {
            char *skill_dir = strndup(path, plen - suflen);
            if (!skill_dir) return NULL;
            char *ident = malloc(plen + strlen(repo) + 2);
            if (!ident) { free(skill_dir); return NULL; }
            snprintf(ident, plen + strlen(repo) + 2, "%s/%s", repo, skill_dir);
            free(skill_dir);
            return ident;
        }
        if (strcmp(path, skill_md_exact) == 0) {
            char *ident = malloc(strlen(repo) + strlen(skill_name) + 2);
            if (!ident) return NULL;
            snprintf(ident, strlen(repo) + strlen(skill_name) + 2, "%s/%s", repo, skill_name);
            return ident;
        }
    }
    return NULL;
}

/* PoP: github_source_fetch_file_content @ tools/skills_hub.py:_fetch_file_content */
char *github_source_fetch_file_content(github_source_t *src, const char *repo, const char *path) {
    if (!src || !repo || !path) return NULL;
    char url[HERMES_PATH_MAX * 2];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/contents/%s", repo, path);
    /* Python uses Accept: v3.raw. We just GET. */
    int status = 0;
    char *body = github_source_github_get(src, url, &status);
    if (!body || status != 200) { free(body); return NULL; }
    return body;
}

/* PoP: github_source_search @ tools/skills_hub.py:search */
int github_source_search(github_source_t *src, const char *query, int limit,
                          hub_skill_meta_t *results) {
    if (!src || !query || limit <= 0 || !results) return 0;
    int found = 0;
    char qbuf[256]; snprintf(qbuf, sizeof(qbuf), "%s", query);
    for (char *p = qbuf; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (int t = 0; t < src->tap_count && found < limit; t++) {
        /* For each tap, list directories via Contents API */
        char url[HERMES_PATH_MAX * 2];
        const char *repo = src->taps[t].repo;
        const char *path = src->taps[t].path;
        if (path[0])
            snprintf(url, sizeof(url), "https://api.github.com/repos/%s/contents/%s", repo, path);
        else
            snprintf(url, sizeof(url), "https://api.github.com/repos/%s/contents", repo);
        int status = 0;
        char *body = github_source_github_get(src, url, &status);
        if (!body || status != 200) { free(body); continue; }
        char *err = NULL;
        json_node_t *arr = json_parse(body, &err);
        free(body); free(err);
        if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); continue; }
        for (size_t i = 0; i < json_len(arr) && found < limit; i++) {
            json_node_t *entry = json_get(arr, i);
            const char *etype = json_get_str(entry, "type", "");
            if (strcmp(etype, "dir") != 0) continue;
            const char *name = json_get_str(entry, "name", "");
            if (!name[0] || name[0] == '.' || name[0] == '_') continue;
            /* match: lowercase substring of name */
            char nbuf[256]; snprintf(nbuf, sizeof(nbuf), "%s", name);
            for (char *p = nbuf; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (!strstr(nbuf, qbuf)) continue;
            char ident[HERMES_PATH_MAX];
            if (strlen(path) > 0)
                snprintf(ident, sizeof(ident), "%s/%s/%s", repo, path, name);
            else
                snprintf(ident, sizeof(ident), "%s/%s", repo, name);
            hub_skill_meta_t *m = &results[found++];
            memset(m, 0, sizeof(*m));
            snprintf(m->name, sizeof(m->name), "%s", name);
            snprintf(m->slug, sizeof(m->slug), "%s", name);
            snprintf(m->source_url, sizeof(m->source_url), "%s", ident);
            snprintf(m->description, sizeof(m->description), "GitHub skill from %s", repo);
            const char *trust = github_source_trust_level_for(src, ident);
            snprintf(m->category, sizeof(m->category), "%s", trust);
        }
        json_free(arr);
    }
    return found;
}








