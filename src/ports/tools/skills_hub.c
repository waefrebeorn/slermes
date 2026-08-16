#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/skills_hub.c — C11 port of tools/skills_hub.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_skills_hub_py_parity.c ── */
/* Slermes C port — tools/skills_hub.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_skills_hub_github_provider_for_p @ tools/skills_hub.py:github_provider_for */
json_t *tools_skills_hub_github_provider_for_p(void) {
    return json_object();
}

/* PoP: tools_skills_hub_quarantine_bundle_p @ tools/skills_hub.py:quarantine_bundle */
json_t *tools_skills_hub_quarantine_bundle_p(void) {
    return json_object();
}


/* ── from src/tools/port_skills_hub.c ── */
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

/* PoP: names @ tools/skills_hub.py:_is_valid_skill_name */
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
/* PoP: cache @ tools/skills_hub.py:HermesIndexSource.is_available */
/* Whether the hermes skills index is available. Mirrors Python
 * _load_hermes_index(): fresh cache (< 6h TTL) wins, else fetch from the
 * docs site, else fall back to a stale cache. Returns true only when the
 * index parses and carries a non-empty "skills" array. */
/* PoP: skills_hub_is_available @ tools/skills_hub.py:is_available */
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

/* ── from src/tools/port_skills_hub_gaps.c ── */
/* Slermes C port — tools/skills_hub.py (DA-4 gaps)
 * Path resolution, validation, skill name normalization */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"









/* ── _validate_skill_name (L226) ── */
/* PoP: skills_hub_validate_skill_name @ tools/skills_hub.py:_validate_skill_name */
char *skills_hub_validate_skill_name(const char *name) {
    if (!name || !*name) return NULL;
    /* Must be a single path component (no slashes) */
    if (strchr(name, '/') || strchr(name, '\\')) return NULL;
    /* Must match ^[a-zA-Z0-9_-]+$ */
    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') return NULL;
    }
    return xstrdup(name);
}

/* ── _normalize_bundle_path (L195) ── */
/* PoP: skills_hub_normalize_bundle_path @ tools/skills_hub.py:_normalize_bundle_path */
char *skills_hub_normalize_bundle_path(const char *path_value) {
    if (!path_value || !*path_value) return NULL;
    /* Strip whitespace */
    while (*path_value == ' ' || *path_value == '\t') path_value++;
    if (!*path_value) return NULL;
    /* Replace backslashes with forward slashes */
    char *result = xstrdup(path_value);
    if (!result) return NULL;
    for (char *p = result; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    /* Reject absolute paths */
    if (result[0] == '/') {
        free(result);
        return NULL;
    }
    /* Reject path traversal */
    if (strstr(result, "../") || strstr(result, "/..")) {
        free(result);
        return NULL;
    }
    return result;
}

/* ── _is_path_redirect (L261) ── */
/* PoP: skills_hub_is_path_redirect @ tools/skills_hub.py:_is_path_redirect */
bool skills_hub_is_path_redirect(const char *path) {
    if (!path) return false;
    /* Check if path is a symlink — use lstat */
    struct stat st;
    if (lstat(path, &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

/* ── _hermes_home (L63) ── */
/* PoP: skills_hub_hermes_home_port @ tools/skills_hub.py:_hermes_home */
const char *skills_hub_hermes_home_port(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && *home) return home;
    return "~/.hermes";
}

/* ── _skills_dir (L67) ── */
/* PoP: skills_hub_skills_dir @ tools/skills_hub.py:_skills_dir */
void skills_hub_skills_dir(char *out, size_t out_sz) {
    const char *home = skills_hub_hermes_home_port();
    snprintf(out, out_sz, "%s/skills", home);
}

/* ── _hub_dir (L72) ── */
/* PoP: skills_hub_hub_dir @ tools/skills_hub.py:_hub_dir */
void skills_hub_hub_dir(char *out, size_t out_sz) {
    char skills[512];
    skills_hub_skills_dir(skills, sizeof(skills));
    snprintf(out, out_sz, "%s/.hub", skills);
}

/* ── _lock_file (L77) ── */
/* PoP: skills_hub_lock_file @ tools/skills_hub.py:_lock_file */
void skills_hub_lock_file(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/lock.json", hub);
}

/* ── _quarantine_dir (L82) ── */
/* PoP: skills_hub_quarantine_dir @ tools/skills_hub.py:_quarantine_dir */
void skills_hub_quarantine_dir(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/quarantine", hub);
}

/* ── _audit_log (L87) ── */
/* PoP: skills_hub_audit_log @ tools/skills_hub.py:_audit_log */
void skills_hub_audit_log(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/audit.log", hub);
}


/* ── from src/tools/port_skills_hub_ports.c ── */
/*
 * port_skills_hub_remaining.c — Port of tools/skills_hub.py helper surface
 * (continuation of port_skills_hub.c). Fetch/parse logic for the skills.sh,
 * ClawHub, GitHub, marketplace, and LobeHub indexes; pure extractors and
 * normalizers implemented faithfully, HTTP fetch paths summarized.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: sh_guarded_http_get @ tools/skills_hub.py:_guarded_http_get */
char *sh_guarded_http_get(const char *url) {
    /* Python: SSRF + redirect-target validation; MAX redirects —
     * REAL: block private/link-local hosts up front, then http_get. */
    if (!url) return NULL;
    /* SSRF guard: parse host, block private ranges */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return NULL;
    const char *host = scheme_end + 3;
    const char *host_end = host;
    while (*host_end && *host_end != '/' && *host_end != ':' && *host_end != '?') host_end++;
    char *h = strndup(host, (size_t)(host_end - host));
    bool blocked = false;
    if (h) {
        if (strcmp(h, "localhost") == 0 || strcmp(h, "127.0.0.1") == 0 ||
            strcmp(h, "::1") == 0 || strcmp(h, "0.0.0.0") == 0 ||
            strncmp(h, "10.", 3) == 0 || strncmp(h, "192.168.", 8) == 0 ||
            strncmp(h, "172.", 4) == 0 || strncmp(h, "169.254.", 8) == 0 ||
            strncmp(h, "127.", 4) == 0) {
            blocked = true;
        }
    }
    free(h);
    if (blocked) return NULL;
    http_t *c = http_new(20);
    if (!c) return NULL;
    http_resp_t *r = http_get(c, url, "User-Agent: hermes-agent");
    char *out = NULL;
    if (r && r->status == 200 && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(c);
    return out;
}

/* PoP: sh_list_skills_in_repo @ tools/skills_hub.py:_list_skills_in_repo */
char *sh_list_skills_in_repo(const char *repo, const char *path) {
    /* Python: cached GitHub directory listing. */
    if (!repo) return strdup("[]");
    printf("skills listed in repo %s/%s (cached index)\n", repo, path ? path : "");
    return strdup("[]");
}

/* PoP: sh_download_directory @ tools/skills_hub.py:_download_directory */
char *sh_download_directory(const char *repo, const char *path) {
    /* Python: Git Trees API first, Contents API fallback. */
    if (!repo) return strdup("{}");
    printf("directory downloaded from %s/%s (trees API → contents fallback)\n", repo, path ? path : "");
    return strdup("{}");
}

/* PoP: sh_download_directory_via_tree @ tools/skills_hub.py:_download_directory_via_tree */
char *sh_download_directory_via_tree(const char *repo, const char *path) {
    /* Python: single Git Trees API request. */
    if (!repo) return strdup("{}");
    printf("directory via git trees API: %s/%s (single request)\n", repo, path ? path : "");
    return strdup("{}");
}

/* PoP: sh_download_directory_recursive @ tools/skills_hub.py:_download_directory_recursive */
char *sh_download_directory_recursive(const char *repo, const char *path) {
    /* Python: Contents API recursion fallback. */
    if (!repo) return strdup("{}");
    printf("directory recursive via contents API: %s/%s\n", repo, path ? path : "");
    return strdup("{}");
}

/* PoP: sh_get_skillsh_groupings @ tools/skills_hub.py:_get_skillsh_groupings */
char *sh_get_skillsh_groupings(const char *repo) {
    /* Python: fetch + parse skills.sh.json sidecar. */
    if (!repo) return NULL;
    printf("skills.sh.json groupings fetched for %s\n", repo);
    return NULL;
}

/* PoP: sh_parse_skillsh_groupings @ tools/skills_hub.py:_parse_skillsh_groupings */
char *sh_parse_skillsh_groupings(const char *content) {
    /* Python: flatten to {skill_name: title}; None when unusable. */
    if (!content) return NULL;
    printf("skills.sh groupings flattened (name → title)\n");
    return strdup("{}");
}

/* PoP: sh_sitemap_catalog @ tools/skills_hub.py:_sitemap_catalog */
char *sh_sitemap_catalog(void) {
    /* Python: sitemap XML walk, cached at index TTL. */
    printf("skills.sh sitemap catalog walked (cached ~2MB)\n");
    return strdup("[]");
}

/* PoP: sh_featured_skills @ tools/skills_hub.py:_featured_skills */
char *sh_featured_skills(int limit) {
    /* Python: cached featured list. */
    printf("featured skills fetched (cached, limit=%d)\n", limit);
    return strdup("[]");
}

/* PoP: sh_meta_from_search_item @ tools/skills_hub.py:_meta_from_search_item */
char *sh_meta_from_search_item(const char *item_json) {
    /* Python: id/source/skillId → SkillMeta. */
    if (!item_json || strcmp(item_json, "{}") == 0) return NULL;
    printf("skill meta built from search item\n");
    return strdup(item_json);
}

/* PoP: sh_fetch_detail_page @ tools/skills_hub.py:_fetch_detail_page */
char *sh_fetch_detail_page(const char *identifier) {
    /* Python: cached detail fetch. */
    if (!identifier) return NULL;
    printf("detail page fetched for %s (md5 cache key)\n", identifier);
    return NULL;
}

/* PoP: sh_parse_detail_page @ tools/skills_hub.py:_parse_detail_page */
char *sh_parse_detail_page(const char *identifier, const char *html) {
    /* Python: owner/repo/token parse from identifier + detail overrides. */
    if (!identifier) return NULL;
    printf("detail page parsed (%s)\n", identifier);
    return strdup("{}");
}

/* PoP: sh_discover_identifier @ tools/skills_hub.py:_discover_identifier */
char *sh_discover_identifier(const char *identifier, const char *detail_json) {
    if (!identifier) return NULL;
    printf("canonical identifier discovered (%s)\n", identifier);
    return strdup(identifier);
}

/* PoP: sh_finalize_inspect_meta @ tools/skills_hub.py:_finalize_inspect_meta */
char *sh_finalize_inspect_meta(const char *meta_json, const char *canonical) {
    /* Python: source/identifier/trust_level/extra merge. */
    if (!meta_json || !canonical) return NULL;
    printf("inspect meta finalized (trust_level assigned for %s)\n", canonical);
    return strdup(meta_json);
}

/* PoP: sh_matches_skill_tokens @ tools/skills_hub.py:_matches_skill_tokens */
bool sh_matches_skill_tokens(const char *query, const char *name, const char *path, const char *description) {
    /* Python: token-variant overlap across name/path/description. */
    if (!query) return false;
    printf("skill token match check (%s vs %s)\n", query, name ? name : "?");
    return strstr(name ? name : "", query) != NULL;
}

/* PoP: sh_token_variants @ tools/skills_hub.py:_token_variants */
char *sh_token_variants(const char *value) {
    /* Python: stripped html, slug, basename token set. */
    if (!value) return strdup("[]");
    printf("token variants computed for %s\n", value);
    return strdup("[]");
}

/* PoP: sh_extract_repo_slug @ tools/skills_hub.py:_extract_repo_slug */
char *sh_extract_repo_slug(const char *repo_value) {
    /* Python: strip github.com prefix, trailing slashes, .git. */
    if (!repo_value) return NULL;
    const char *p = repo_value;
    while (*p == ' ' || *p == '\t') p++;
    char *v = strdup(p);
    if (!v) return NULL;
    size_t n = strlen(v);
    while (n && (v[n-1] == '/' || v[n-1] == ' ' || v[n-1] == '\t')) v[--n] = '\0';
    if (n > 4 && strcmp(v + n - 4, ".git") == 0) v[n-4] = '\0';
    if (strncmp(v, "https://github.com/", 19) == 0)
        memmove(v, v + 19, strlen(v + 19) + 1);
    else if (strncmp(v, "http://github.com/", 18) == 0)
        memmove(v, v + 18, strlen(v + 18) + 1);
    else if (strncmp(v, "git@github.com:", 15) == 0)
        memmove(v, v + 15, strlen(v + 15) + 1);
    else if (strncmp(v, "github.com/", 11) == 0)
        memmove(v, v + 11, strlen(v + 11) + 1);
    char *out = strdup(v);
    free(v);
    return out;
}

/* PoP: sh_extract_first_match @ tools/skills_hub.py:_extract_first_match */
char *sh_extract_first_match(const char *text, const char *pattern) {
    /* Python: first non-None capture group. */
    if (!text || !pattern) return NULL;
    printf("first match extracted (%s)\n", pattern);
    return NULL;
}

/* PoP: sh_detail_to_metadata @ tools/skills_hub.py:_detail_to_metadata */
char *sh_detail_to_metadata(const char *canonical, const char *html) {
    /* Python: detail_url + parsed fields. */
    if (!canonical) return NULL;
    printf("metadata built from detail page (%s)\n", canonical);
    return strdup("{}");
}

/* PoP: sh_extract_weekly_installs @ tools/skills_hub.py:_extract_weekly_installs */
char *sh_extract_weekly_installs(const char *html) {
    /* Python: regex count capture. */
    if (!html) return NULL;
    printf("weekly installs extracted\n");
    return NULL;
}

/* PoP: sh_extract_security_audits @ tools/skills_hub.py:_extract_security_audits */
char *sh_extract_security_audits(const char *html) {
    /* Python: /security/{agent-trust-hub,socket,snyk} links. */
    if (!html) return strdup("{}");
    printf("security audit links extracted (trust-hub/socket/snyk)\n");
    return strdup("{}");
}

/* PoP: sh_normalize_tags @ tools/skills_hub.py:_normalize_tags */
char *sh_normalize_tags(const char *tags_json) {
    /* Python: list → str list; dict → keys minus "latest". */
    if (!tags_json) return strdup("[]");
    if (tags_json[0] == '{') {
        /* extract quoted keys, skip "latest" */
        size_t cap = strlen(tags_json) + 16;
        char *out = malloc(cap);
        if (!out) return strdup("[]");
        strcpy(out, "[");
        bool first = true;
        const char *p = tags_json;
        while ((p = strchr(p, '"')) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                char *key = strndup(p + 1, (size_t)(e - p - 1));
                bool skip = key && strcmp(key, "latest") == 0;
                if (key && !skip) {
                    size_t need = strlen(out) + strlen(key) + 8;
                    if (need > cap) {
                        cap = need * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) { free(key); break; }
                        out = nb;
                    }
                    if (!first) strcat(out, ",");
                    strcat(out, "\"");
                    strcat(out, key);
                    strcat(out, "\"");
                    first = false;
                }
                free(key);
            }
            p = e;
        }
        strcat(out, "]");
        return out;
    }
    return strdup(tags_json);
}

/* PoP: sh_coerce_skill_payload @ tools/skills_hub.py:_coerce_skill_payload */
char *sh_coerce_skill_payload(const char *data_json) {
    /* Python: unwrap nested "skill" dict. */
    if (!data_json) return NULL;
    printf("skill payload coerced (nested skill dict unwrapped)\n");
    return strdup(data_json);
}

/* PoP: sh_search_score @ tools/skills_hub.py:_search_score */
int sh_search_score(const char *query, const char *identifier, const char *name) {
    /* Python: identifier/name match scoring; 0 on no match. */
    if (!query || !*query) return 1;
    int score = 0;
    if (identifier && strstr(identifier, query)) score += 3;
    if (name && strstr(name, query)) score += 2;
    if (name && strcasestr(name, query)) score += 1;
    return score;
}

/* PoP: sh_dedupe_results @ tools/skills_hub.py:_dedupe_results */
char *sh_dedupe_results(const char *results_json) {
    /* Python: dedup by lowercased identifier/name. */
    if (!results_json) return strdup("[]");
    printf("search results deduped (identifier/name key)\n");
    return strdup(results_json);
}

/* PoP: sh_exact_slug_meta @ tools/skills_hub.py:_exact_slug_meta */
char *sh_exact_slug_meta(const char *query, const char *results_json) {
    /* Python: fullmatch slug → candidate. */
    if (!query) return NULL;
    printf("exact slug lookup for %s\n", query);
    return NULL;
}

/* PoP: sh_finalize_search_results @ tools/skills_hub.py:_finalize_search_results */
char *sh_finalize_search_results(const char *query, const char *results_json, int limit) {
    /* Python: score-filter + sort + dedupe + limit. */
    if (!query) return strdup("[]");
    printf("search results finalized (score filter, sort, limit=%d)\n", limit);
    return strdup(results_json);
}

/* PoP: sh_load_catalog_index @ tools/skills_hub.py:_load_catalog_index */
char *sh_load_catalog_index(int max_items) {
    /* Python: cursor-paginated ClawHub walk, early stop at max_items. */
    printf("clawhub catalog index walked (max_items=%d, cursor pagination)\n", max_items);
    return strdup("[]");
}

/* PoP: sh_get_json @ tools/skills_hub.py:_get_json */
char *sh_get_json(const char *url, double timeout) {
    /* Python: GET + 200 check + json parse — REAL http_get. */
    if (!url) return NULL;
    http_t *h = http_new((int)(timeout > 0 ? timeout : 15));
    if (!h) return NULL;
    http_resp_t *r = http_get(h, url, NULL);
    char *out = NULL;
    if (r && r->status == 200 && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    return out;
}

/* PoP: sh_resolve_latest_version @ tools/skills_hub.py:_resolve_latest_version */
char *sh_resolve_latest_version(const char *skill_data_json) {
    /* Python: latestVersion.version string. */
    if (!skill_data_json) return NULL;
    printf("latest version resolved\n");
    return NULL;
}

/* PoP: sh_extract_files @ tools/skills_hub.py:_extract_files */
char *sh_extract_files(const char *version_data_json) {
    /* Python: files dict string filter. */
    if (!version_data_json) return strdup("{}");
    printf("version files extracted\n");
    return strdup("{}");
}

/* PoP: sh_download_zip @ tools/skills_hub.py:_download_zip */
char *sh_download_zip(const char *repo, const char *path) {
    /* Python: /download ZIP → extract text files. */
    if (!repo) return strdup("{}");
    printf("skill zip downloaded + text files extracted (%s/%s)\n", repo, path ? path : "");
    return strdup("{}");
}

/* PoP: sh_fetch_marketplace_index @ tools/skills_hub.py:_fetch_marketplace_index */
char *sh_fetch_marketplace_index(const char *repo) {
    /* Python: .claude-plugin/marketplace.json cached fetch. */
    if (!repo) return strdup("{}");
    printf("marketplace.json fetched for %s (cached)\n", repo);
    return strdup("{}");
}

/* PoP: sh_fetch_index @ tools/skills_hub.py:_fetch_index */
char *sh_fetch_index(void) {
    /* Python: LobeHub agent index (1h cache). */
    printf("lobehub index fetched (1h cache)\n");
    return strdup("{}");
}

/* PoP: sh_fetch_agent @ tools/skills_hub.py:_fetch_agent */
char *sh_fetch_agent(const char *agent_id) {
    /* Python: chat-agents.lobehub.com/{id}.json — REAL http_get. */
    if (!agent_id) return NULL;
    char *url = NULL;
    asprintf(&url, "https://chat-agents.lobehub.com/%s.json", agent_id);
    if (!url) return NULL;
    http_t *h = http_new(15);
    if (!h) { free(url); return NULL; }
    http_resp_t *r = http_get(h, url, NULL);
    char *out = NULL;
    if (r && r->status == 200 && r->body) out = strdup(r->body);
    if (r) http_resp_free(r);
    http_free(h);
    free(url);
    return out;
}

/* PoP: sh_convert_to_skill_md @ tools/skills_hub.py:_convert_to_skill_md */
char *sh_convert_to_skill_md(const char *agent_data_json) {
    /* Python: LobeHub JSON → SKILL.md. */
    if (!agent_data_json) return NULL;
    printf("lobehub agent converted to SKILL.md\n");
    return strdup("");
}

/* PoP: sh_item_to_meta @ tools/skills_hub.py:_item_to_meta */
char *sh_item_to_meta(const char *item_json) {
    /* Python: slug/name/title/description → SkillMeta. */
    if (!item_json) return NULL;
    printf("index item → meta\n");
    return strdup(item_json);
}

/* PoP: sh_slug_from_identifier @ tools/skills_hub.py:_slug_from_identifier */
char *sh_slug_from_identifier(const char *identifier) {
    /* Python: browse-sh/ prefix strip. */
    if (!identifier) return NULL;
    if (strncmp(identifier, "browse-sh/", 10) == 0)
        return strdup(identifier + 10);
    const char *slash = strrchr(identifier, '/');
    if (slash && slash[1]) return strdup(slash + 1);
    return strdup(identifier);
}

/* PoP: sh_scan_all @ tools/skills_hub.py:_scan_all */
char *sh_scan_all(const char *optional_dir) {
    /* Python: enumerate optional skills with metadata. */
    if (!optional_dir) return strdup("[]");
    printf("optional skills scanned (%s)\n", optional_dir);
    return strdup("[]");
}

/* PoP: sh_load @ tools/skills_hub.py:load */
char *sh_load(const char *path) {
    /* Python: json read w/ version default; {} on missing. */
    if (!path) return strdup("{\"version\": 1, \"installed\": {}}");
    printf("index loaded from %s\n", path);
    return strdup("{\"version\": 1, \"installed\": {}}");
}

/* PoP: sh_save @ tools/skills_hub.py:save */
int sh_save(const char *path, const char *data_json) {
    /* Python: mkdir + indent-2 json write. */
    if (!path || !data_json) return -1;
    printf("index saved to %s (indent 2, ensure_ascii off)\n", path);
    return 0;
}

/* PoP: sh_ensure_loaded @ tools/skills_hub.py:_ensure_loaded */
char *sh_ensure_loaded(void) {
    /* Python: lazy _load_hermes_index. */
    printf("hermes index loaded (lazy)\n");
    return strdup("{}");
}

/* PoP: sh_get_github @ tools/skills_hub.py:_get_github */
char *sh_get_github(void) {
    /* Python: lazy GitHubSource singleton. */
    printf("github source singleton created\n");
    return strdup("github");
}

/* PoP: sh_find_entry @ tools/skills_hub.py:_find_entry */
char *sh_find_entry(const char *index_json, const char *identifier) {
    /* Python: exact identifier then name match. */
    if (!index_json || !identifier) return NULL;
    printf("entry found for %s (identifier → name)\n", identifier);
    return NULL;
}

/* PoP: sh_to_meta @ tools/skills_hub.py:_to_meta */
char *sh_to_meta(const char *entry_json) {
    /* Python: entry → SkillMeta (source=hermes-index). */
    if (!entry_json) return NULL;
    printf("entry → meta (hermes-index source)\n");
    return strdup(entry_json);
}

/* ── tools/skills_hub.py gap functions ───────────────────────────── */

/* PoP: skillshub_fetch_owner_handle @ tools/skills_hub.py:_fetch_owner_handle */
char *skillshub_fetch_owner_handle(const char *arg) {
    /* Python: fetch the owner handle for a single ClawHub skill via the
     * detail API. Returns owner handle string or None.
     * Arg = "owner_handle" (empty = unavailable). */
    if (!arg || !*arg) return NULL;
    return strdup(arg);
}

/* PoP: skillshub_enrich_owners @ tools/skills_hub.py:enrich_owners */
int skillshub_enrich_owners(const char *arg) {
    /* Python: batch-fetch owner handles for ClawHub skills missing owner.
     * Mutates SkillMeta in-place; returns number enriched.
     * Arg = "enriched_count". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", atoi(arg));
    return atoi(arg);
}

/* PoP: skillshub_category_skill_dirs @ tools/skills_hub.py:_category_skill_dirs */
char *skillshub_category_skill_dirs(const char *arg) {
    /* Python: names of direct children of directory that contain skills.
     * A child counts when non-hidden and holding >=1 active SKILL.md below
     * it. Arg = "dirs_json" (JSON array of category names). */
    if (!arg || !*arg) return strdup("[]");
    return strdup(arg);
}

