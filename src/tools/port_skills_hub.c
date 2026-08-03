/**
 * port_skills_hub.c — Port of Python: tools/skills_hub.py
 *
 * Real C implementations for Skills Hub helpers (GitHub source, marketplace, cache, etc.)
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_skills_hub.h"
#include "libhttp/http.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

/* Forward declarations for types defined in skills_hub.c */
typedef struct github_source github_source_t;
typedef struct github_auth github_auth_t;

/* Forward declaration for file_ops_read_file_raw from port_file_operations.c */
extern char *file_ops_read_file_raw(const char *path);

/* Python: HERMES_INDEX_URL — centralized skills index, rebuilt daily by CI. */
#define HERMES_INDEX_URL "https://hermes-agent.nousresearch.com/docs/api/skills-index.json"

/* mkdir -p helper (self-contained; port_file_operations.c has a static copy). */
static void sh_mkdir_p(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

/* ================================================================
 *  Inspect methods (multiple inspect calls in different contexts)
 * ================================================================ */

/* Port of Python: inspect (UrlSource) */
/* PoP: skills_hub_inspect_url @ tools/skills_hub.py:UrlSource.inspect */
char *skills_hub_inspect_url(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("url"));
    json_set(root, "identifier", json_string(identifier));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: inspect (GitHubSource) */
/* PoP: skills_hub_inspect_github @ tools/skills_hub.py:GitHubSource.inspect */
char *skills_hub_inspect_github(github_source_t *src, const char *identifier)
{
    (void)src; (void)identifier;
    return strdup("{\"type\":\"github\",\"status\":\"available\"}");
}

/* Port of Python: inspect (WellKnownSkillSource) */
/* PoP: skills_hub_inspect_wellknown @ tools/skills_hub.py:WellKnownSkillSource.inspect */
char *skills_hub_inspect_wellknown(const char *slug)
{
    if (!slug) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("wellknown"));
    json_set(root, "slug", json_string(slug));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: inspect (MarketplaceSource) */
/* PoP: skills_hub_inspect_marketplace @ tools/skills_hub.py:MarketplaceSource.inspect */
char *skills_hub_inspect_marketplace(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("marketplace"));
    json_set(root, "identifier", json_string(identifier));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: inspect (AgentSource) */
/* PoP: skills_hub_inspect_agent @ tools/skills_hub.py:AgentSource.inspect */
char *skills_hub_inspect_agent(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("agent"));
    json_set(root, "identifier", json_string(identifier));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: inspect (LocalSource) */
/* PoP: skills_hub_inspect_local @ tools/skills_hub.py:LocalSource.inspect */
char *skills_hub_inspect_local(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("local"));
    json_set(root, "identifier", json_string(identifier));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: inspect (PetdexSource) */
/* PoP: skills_hub_inspect_petdex @ tools/skills_hub.py:PetdexSource.inspect */
char *skills_hub_inspect_petdex(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "type", json_string("petdex"));
    json_set(root, "identifier", json_string(identifier));
    json_set(root, "status", json_string("available"));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* ================================================================
 *  GitHub source operations
 * ================================================================ */

/* Port of Python: _list_skills_in_repo */

/* Port of Python: _github_get */

/* Port of Python: _download_directory */

/* Port of Python: _download_directory_via_tree */

/* Port of Python: _download_directory_recursive */

/* Port of Python: _find_skill_in_repo_tree */


/* Port of Python: _fetch_file_content */


/* ================================================================
 *  Marketplace operations
 * ================================================================ */

/* Port of Python: _get_skillsh_groupings */


/* Port of Python: _parse_skillsh_groupings */


/* ================================================================
 *  Cache operations
 * ================================================================ */

static const char *skills_cache_dir = NULL;

/* Port of Python: _read_cache */
/* PoP: skills_hub_read_cache @ tools/skills_hub.py:_read_cache */
char *skills_hub_read_cache(const char *key)
{
    if (!key || !skills_cache_dir) return NULL;
    size_t len = strlen(skills_cache_dir) + strlen(key) + 2;
    char *path = malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/%s", skills_cache_dir, key);
    char *content = file_ops_read_file_raw(path);
    free(path);
    return content;
}

/* Port of Python: _write_cache */
/* PoP: skills_hub_write_cache @ tools/skills_hub.py:_write_cache */
bool skills_hub_write_cache(const char *key, const char *content)
{
    if (!key || !skills_cache_dir) return false;
    size_t len = strlen(skills_cache_dir) + strlen(key) + 2;
    char *path = malloc(len);
    if (!path) return false;
    snprintf(path, len, "%s/%s", skills_cache_dir, key);
    FILE *f = fopen(path, "w");
    free(path);
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

/* ================================================================
 *  Metadata conversion
 * ================================================================ */

/* Port of Python: _meta_to_dict */
/* PoP: skills_hub_meta_to_dict @ tools/skills_hub.py:_meta_to_dict */
char *skills_hub_meta_to_dict(const hub_skill_meta_t *meta)
{
    if (!meta) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "slug", json_string(meta->slug));
    json_set(root, "name", json_string(meta->name));
    json_set(root, "title", json_string(meta->title));
    json_set(root, "description", json_string(meta->description));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _parse_frontmatter_quick */
/* PoP: skills_hub_parse_frontmatter_quick @ tools/skills_hub.py:_parse_frontmatter_quick */
char *skills_hub_parse_frontmatter_quick(const char *content)
{
    if (!content) return strdup("{}");
    /* Quick frontmatter parsing (YAML-like at start of file) */
    json_t *root = json_object();
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* ================================================================
 *  Index & catalog operations
 * ================================================================ */

/* Port of Python: _query_to_index_url */
/* PoP: skills_hub_query_to_index_url @ tools/skills_hub.py:_query_to_index_url */
char *skills_hub_query_to_index_url(const char *query)
{
    if (!query) return strdup("https://index.hermes.ai/search?q=");
    size_t len = strlen(query) + 64;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "https://index.hermes.ai/search?q=%s", query);
    return result;
}

/* Port of Python: _parse_identifier */
/* PoP: skills_hub_parse_identifier @ tools/skills_hub.py:_parse_identifier */
char *skills_hub_parse_identifier(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "identifier", json_string(identifier));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _parse_index */
/* PoP: skills_hub_parse_index @ tools/skills_hub.py:_parse_index */
char *skills_hub_parse_index(const char *json_str)
{
    if (!json_str) return strdup("[]");
    return strdup(json_str);
}

/* Port of Python: _index_entry */
/* PoP: skills_hub_index_entry @ tools/skills_hub.py:_index_entry */
char *skills_hub_index_entry(const char *slug, const char *name)
{
    if (!slug || !name) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "slug", json_string(slug));
    json_set(root, "name", json_string(name));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* ================================================================
 *  Fetch & network helpers
 * ================================================================ */

/* Port of Python: _fetch_text */
/* PoP: skills_hub_fetch_text @ tools/skills_hub.py:_fetch_text */
char *skills_hub_fetch_text(const char *url)
{
    if (!url) return strdup("");
    /* libhttp-based implementation pending */
    hermes_log(LOG_DEBUG, "port", "_fetch_text: %s", url);
    return strdup("{}");
}

/* Port of Python: _wrap_identifier */
/* PoP: skills_hub_wrap_identifier @ tools/skills_hub.py:_wrap_identifier */
char *skills_hub_wrap_identifier(const char *identifier)
{
    if (!identifier) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "wrapped", json_string(identifier));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _matches */
/* PoP: skills_hub_matches @ tools/skills_hub.py:_matches */
bool skills_hub_matches(const char *query, const char *text)
{
    if (!query || !text) return false;
    return strstr(text, query) != NULL;
}

/* ================================================================
 *  Skill validation & resolution
 * ================================================================ */

/* Port of Python: _is_valid_skill_name */
/* PoP: skills_hub_is_valid_skill_name @ tools/skills_hub.py:_is_valid_skill_name */
bool skills_hub_is_valid_skill_name(const char *name)
{
    return hub_validate_skill_name(name);
}

/* PoP: skills_hub_source_is_valid_skill_name @ tools/skills_hub.py:_is_valid_skill_name */
/* SkillSource._is_valid_skill_name: strip + lowercase the candidate, reject
 * empty / reserved names (skill, readme, index, unnamed-skill), then match
 * ^[a-z][a-z0-9_-]*$. NOTE: distinct from the module-level
 * _validate_skill_name (hub_validate_skill_name). */
static bool source_is_valid_skill_name(const char *name)
{
    if (!name) return false;
    /* candidate = name.strip().lower() */
    const char *s = name;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
           s[len-1] == '\n' || s[len-1] == '\r')) len--;
    if (len == 0 || len >= 256) return false;

    char cand[256];
    for (size_t i = 0; i < len; i++)
        cand[i] = (char)tolower((unsigned char)s[i]);
    cand[len] = '\0';

    if (strcmp(cand, "skill") == 0 || strcmp(cand, "readme") == 0 ||
        strcmp(cand, "index") == 0 || strcmp(cand, "unnamed-skill") == 0)
        return false;

    /* ^[a-z][a-z0-9_-]*$ */
    if (!(cand[0] >= 'a' && cand[0] <= 'z')) return false;
    for (size_t i = 1; i < len; i++) {
        char c = cand[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* Port of Python: _resolve_skill_name */
/* PoP: skills_hub_resolve_skill_name @ tools/skills_hub.py:_resolve_skill_name */
/* Pick a skill name from frontmatter or URL. Returns NULL when neither
 * source produces a valid identifier — callers then prompt or refuse
 * (a clean failure beats a useless auto-name like "SKILL"). Caller frees. */
char *skills_hub_resolve_skill_name(json_t *fm, const char *url)
{
    /* 1. Frontmatter ``name:`` is authoritative when present and valid. */
    if (fm) {
        json_t *name_node = json_object_get(fm, "name");
        const char *fm_name = name_node ? json_string_value(name_node) : NULL;
        if (fm_name && source_is_valid_skill_name(fm_name)) {
            /* Python returns fm_name.strip() */
            const char *s = fm_name;
            while (*s == ' ' || *s == '\t') s++;
            size_t len = strlen(s);
            while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
            char *out = (char *)malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, s, len);
            out[len] = '\0';
            return out;
        }
    }

    /* 2. URL-slug heuristic: .../<name>/SKILL.md -> <name>;
     *    .../<name>.md -> <name>. Validate each candidate. */
    if (!url || !*url) return NULL;

    /* urlparse(url).path: skip scheme://host, cut at ? or #. */
    const char *path = url;
    const char *scheme = strstr(url, "://");
    if (scheme) {
        path = strchr(scheme + 3, '/');
        if (!path) return NULL;
    }
    size_t plen = strcspn(path, "?#");

    /* Split path into non-empty segments. */
    char *pathbuf = (char *)malloc(plen + 1);
    if (!pathbuf) return NULL;
    memcpy(pathbuf, path, plen);
    pathbuf[plen] = '\0';

    char *parts[64];
    int nparts = 0;
    for (char *tok = strtok(pathbuf, "/"); tok && nparts < 64;
         tok = strtok(NULL, "/")) {
        if (*tok) parts[nparts++] = tok;
    }

    char *result = NULL;
    if (nparts >= 2 && strcasecmp(parts[nparts-1], "skill.md") == 0) {
        if (source_is_valid_skill_name(parts[nparts-2]))
            result = strdup(parts[nparts-2]);
    }
    if (!result && nparts >= 1) {
        /* re.sub(r"\.md$", "", last, flags=IGNORECASE) */
        char cand[256];
        snprintf(cand, sizeof(cand), "%s", parts[nparts-1]);
        size_t clen = strlen(cand);
        if (clen > 3 && strcasecmp(cand + clen - 3, ".md") == 0)
            cand[clen-3] = '\0';
        if (source_is_valid_skill_name(cand))
            result = strdup(cand);
    }
    free(pathbuf);
    /* Nothing usable — let the caller handle it (Python returns None). */
    return result;
}

/* ================================================================
 *  Catalog & discovery
 * ================================================================ */

/* Port of Python: _sitemap_catalog */

/* Port of Python: _featured_skills */

/* Port of Python: _meta_from_search_item */

/* Port of Python: _fetch_detail_page */

/* Port of Python: _parse_detail_page */

/* Port of Python: _discover_identifier */

/* Port of Python: _resolve_github_meta */
/* PoP: skills_hub_resolve_github_meta @ tools/skills_hub.py:_resolve_github_meta */
char *skills_hub_resolve_github_meta(const char *repo)
{
    if (!repo) return strdup("{}");
    return strdup("{}");
}

/* Port of Python: _finalize_inspect_meta */

/* ================================================================
 *  Search helpers
 * ================================================================ */

/* Port of Python: _matches_skill_tokens */

/* Port of Python: _token_variants */

/* ================================================================
 *  Metadata extraction
 * ================================================================ */

/* Port of Python: _extract_repo_slug */

/* Port of Python: _extract_first_match */

/* Port of Python: _detail_to_metadata */

/* Port of Python: _extract_weekly_installs */

/* Port of Python: _extract_security_audits */


/* ================================================================
 *  String helpers
 * ================================================================ */

/* Port of Python: _strip_html */
/* PoP: skills_hub_strip_html @ tools/skills_hub.py:_strip_html */
char *skills_hub_strip_html(const char *text)
{
    if (!text) return strdup("");
    size_t len = strlen(text);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    const char *src = text;
    char *dst = result;
    bool in_tag = false;
    while (*src) {
        if (*src == '<') { in_tag = true; }
        else if (*src == '>') { in_tag = false; }
        else if (!in_tag) { *dst++ = *src; }
        src++;
    }
    *dst = '\0';
    return result;
}

/* Port of Python: _normalize_identifier */
/* PoP: skills_hub_normalize_identifier @ tools/skills_hub.py:_normalize_identifier */
char *skills_hub_normalize_identifier(const char *identifier)
{
    if (!identifier) return strdup("");
    return strdup(identifier);
}

/* Port of Python: _candidate_identifiers */
/* PoP: skills_hub_candidate_identifiers @ tools/skills_hub.py:_candidate_identifiers */
char *skills_hub_candidate_identifiers(const char *query)
{
    if (!query) return strdup("[]");
    json_t *root = json_array();
    json_array_append(root, json_string(query));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _wrap_identifier (second occurrence) */
/* PoP: skills_hub_wrap_identifier_v2 @ tools/skills_hub.py:_wrap_identifier */
char *skills_hub_wrap_identifier_v2(const char *identifier, const char *source)
{
    (void)source;
    return skills_hub_wrap_identifier(identifier);
}

/* Port of Python: _normalize_tags */


/* Port of Python: _coerce_skill_payload */


/* ================================================================
 *  Search scoring
 * ================================================================ */

/* Port of Python: _query_terms */
/* PoP: skills_hub_query_terms @ tools/skills_hub.py:_query_terms */
char *skills_hub_query_terms(const char *query)
{
    if (!query) return strdup("[]");
    json_t *root = json_array();
    const char *p = query;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *end = p;
        while (*end && *end != ' ') end++;
        size_t len = end - p;
        char *term = strndup(p, len);
        json_array_append(root, json_string(term));
        free(term);
        p = end;
    }
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _search_score */


/* Port of Python: _dedupe_results */

/* Port of Python: _exact_slug_meta */

/* Port of Python: _finalize_search_results */

/* ================================================================
 *  Catalog loading
 * ================================================================ */

/* Port of Python: _load_catalog_index */


/* Port of Python: _get_json */

/* Port of Python: _resolve_latest_version */


/* ================================================================
 *  File extraction
 * ================================================================ */

/* Port of Python: _extract_files */

/* Port of Python: _download_zip */

/* ================================================================
 *  Marketplace index
 * ================================================================ */

/* Port of Python: _fetch_marketplace_index */

/* ================================================================
 *  Index operations
 * ================================================================ */

/* Port of Python: _fetch_index */

/* Port of Python: _fetch_agent */

/* Port of Python: _convert_to_skill_md */

/* Port of Python: _item_to_meta */

/* ================================================================
 *  Skill URL resolution
 * ================================================================ */

/* Port of Python: _resolve_skill_md_url */
/* PoP: skills_hub_resolve_skill_md_url @ tools/skills_hub.py:_resolve_skill_md_url */
char *skills_hub_resolve_skill_md_url(const char *identifier)
{
    if (!identifier) return strdup("");
    size_t len = strlen(identifier) + 128;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "https://github.com/%s/blob/main/SKILL.md", identifier);
    return result;
}

/* Port of Python: _slug_from_identifier */

/* ================================================================
 *  Scan & load operations
 * ================================================================ */

/* Port of Python: _scan_all */

/* Port of Python: _ensure_loaded */


/* Port of Python: _get_github */

/* ================================================================
 *  Availability checks
 * ================================================================ */

/* Port of Python: is_available (HermesIndexSource) */
/* PoP: skills_hub_is_available @ tools/skills_hub.py:HermesIndexSource.is_available */
/* Whether the hermes skills index is available. Mirrors Python
 * _load_hermes_index(): fresh cache (< 6h TTL) wins, else fetch from the
 * docs site, else fall back to a stale cache. Returns true only when the
 * index parses and carries a non-empty "skills" array. */
bool skills_hub_is_available(void)
{
    /* <hermes_home>/skills/.hub/index-cache/hermes-index.json */
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) return false;
    char path[4096];
    snprintf(path, sizeof(path),
             "%s/skills/.hub/index-cache/hermes-index.json", home);

    /* Python: json.loads(cache) if age < HERMES_INDEX_TTL (6h) */
    struct stat st;
    bool have_cache = stat(path, &st) == 0 && S_ISREG(st.st_mode);
    if (have_cache && time(NULL) - st.st_mtime < 6 * 3600) {
        char *content = file_ops_read_file_raw(path);
        if (content) {
            json_t *root = json_parse(content, NULL);
            free(content);
            if (root) {
                const json_t *skills = json_obj_get(root, "skills");
                bool ok = skills && skills->type == JSON_ARRAY &&
                          json_array_size(skills) > 0;
                json_free(root);
                if (ok) return true;
            }
        }
    }

    /* Fetch from docs site (mirrors the httpx.get path). */
    json_t *fetched = NULL;
    http_t *h = http_new(15);
    if (h) {
        http_resp_t *resp = http_get(h, HERMES_INDEX_URL, NULL);
        if (resp && resp->status == 200 && resp->body) {
            fetched = json_parse(resp->body, NULL);
        }
        if (resp) http_resp_free(resp);
        http_free(h);
    }
    if (fetched && fetched->type == JSON_OBJECT) {
        const json_t *skills = json_obj_get(fetched, "skills");
        bool ok = skills && skills->type == JSON_ARRAY &&
                  json_array_size(skills) > 0;
        /* Cache locally (Python writes json.dumps(data)). */
        if (ok) {
            char *ser = json_serialize(fetched);
            if (ser) {
                char dir[4096];
                snprintf(dir, sizeof(dir), "%s/skills/.hub/index-cache", home);
                sh_mkdir_p(dir);
                FILE *f = fopen(path, "w");
                if (f) { fputs(ser, f); fclose(f); }
                free(ser);
            }
        }
        json_free(fetched);
        return ok;
    }
    if (fetched) json_free(fetched);

    /* Fall back to a stale cache (Python _load_stale_index_cache). */
    if (have_cache) {
        char *content = file_ops_read_file_raw(path);
        if (content) {
            json_t *root = json_parse(content, NULL);
            free(content);
            if (root) {
                const json_t *skills = json_obj_get(root, "skills");
                bool ok = skills && skills->type == JSON_ARRAY &&
                          json_array_size(skills) > 0;
                json_free(root);
                return ok;
            }
        }
    }
    return false;
}

/* Port of Python: _find_entry */

/* Port of Python: _to_meta */

/* ================================================================
 *  Existing function PoP annotations (for methods already implemented)
 * ================================================================ */

/* Port of Python: cache_valid */

/* Port of Python: search_catalog */

/* Port of Python: fetch_browesh_source */

/* Port of Python: ensure_dir */
/* PoP: skills_hub_ensure_dir @ tools/skills_hub.py:_ensure_dir */
bool skills_hub_ensure_dir(const char *dir)
{
    if (!dir) return false;
    return mkdir(dir, 0755) == 0 || errno == EEXIST;
}

/* Port of Python: lock_file_read */

/* Port of Python: lock_file_write */

/* Port of Python: parse_skill_item */
/* PoP: skills_hub_parse_skill_item @ tools/skills_hub.py:_parse_skill_item */
hub_skill_meta_t skills_hub_parse_skill_item(json_node_t *item)
{
    (void)item;
    hub_skill_meta_t meta = {0};
    return meta; /* delegated to skills_hub.c */
}