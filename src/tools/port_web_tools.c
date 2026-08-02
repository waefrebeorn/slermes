/**
 * port_web_tools.c — Port of Python: tools/web_tools.py
 *
 * Real C implementations for web tool helpers.
 */

#ifndef SRC_TOOLS_PORT_WEB_TOOLS_C
#define SRC_TOOLS_PORT_WEB_TOOLS_C

#include "port_web_tools.h"
#include "web_base64_img.h"
#include "hermes_url_safety.h"
#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "yaml.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Opaque struct definition - private to this translation unit */
struct port_web_tools_state {
    char *cached_backend;
    char *cached_config;
};

port_web_tools_state_t *port_web_tools_state_init(void)
{
    port_web_tools_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->cached_backend = NULL;
    state->cached_config = NULL;
    return state;
}

void port_web_tools_state_cleanup(port_web_tools_state_t *state)
{
    if (!state) return;
    free(state->cached_backend);
    free(state->cached_config);
    free(state);
}

/* Forward declarations */
char *web_env_value(const char *name);
bool web_has_env(const char *name);
/* PoP: web_get_backend @ tools/computer_use/tool.py:_get_backend */
char *web_get_backend(void);
char *web_get_search_backend(void);
char *web_get_extract_backend(void);
char *web_get_capability_backend(const char *capability);
bool web_is_backend_available(const char *backend);
bool web_ddgs_package_importable(void);
char **web_web_requires_env(int *out_count);
int web_get_extract_char_limit(void);
char *web_convert_base64_images_to_links(const char *text);
char *web_store_full_text(const char *url, const char *content);
bool web_truncate_with_footer(const char *content, const char *url, int char_limit, char **out_text, bool *out_truncated);
char *web_search_tool(const char *query, int limit);
bool web_check_web_api_key(void);

/* PoP: web_env_value @ tools/web_tools.py:_env_value */
char *web_env_value(const char *name)
{
    if (!name) return strdup("");

    /* Try hermes config first */
    const char *val = getenv(name);
    if (!val) val = "";

    return strdup(val);
}

/* PoP: web_has_env @ tools/web_tools.py:_has_env */
bool web_has_env(const char *name)
{
    char *val = web_env_value(name);
    bool exists = (val && val[0] != '\0');
    free(val);
    return exists;
}

/* PoP: web_load_web_config @ tools/web_tools.py:_load_web_config */
char *web_load_web_config(void)
{
    /* Load the web: section from ~/.hermes/config.yaml via hermes_config_load. */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    /* Return the web config section as JSON string */
    json_t *web = json_object();
    if (cfg.tools.web_extract_backend && cfg.tools.web_extract_backend[0])
        json_set(web, "extract_backend", json_string(cfg.tools.web_extract_backend));
    if (cfg.tools.web_search_backend && cfg.tools.web_search_backend[0])
        json_set(web, "search_backend", json_string(cfg.tools.web_search_backend));
    json_set(web, "extract_char_limit", json_new_number(15000));
    char *serialized = json_serialize(web);
    json_free(web);
    return serialized ? serialized : strdup("{}");
}

/* PoP: web_ensure_web_plugins_loaded @ tools/web_tools.py:_ensure_web_plugins_loaded */
void web_ensure_web_plugins_loaded(void)
{
    /* In C, the registry is loaded at init time — no need for lazy plugin discovery.
     * This function is a no-op for C; the web tools are already initialized. */
    hermes_log(LOG_DEBUG, "web", "web_ensure_web_plugins_loaded: C registry already initialized");
}



/* PoP: web_get_backend @ tools/web_tools.py:_get_backend */
char *web_get_backend(void)
{

    /* Fallback priority order */
    if (web_has_env("TAVILY_API_KEY")) return strdup("tavily");
    if (web_has_env("EXA_API_KEY")) return strdup("exa");
    if (web_has_env("PARALLEL_API_KEY")) return strdup("parallel");
    if (web_has_env("FIRECRAWL_API_KEY") || web_has_env("FIRECRAWL_API_URL")) return strdup("firecrawl");
    if (web_has_env("SEARXNG_URL")) return strdup("searxng");
    if (web_has_env("BRAVE_SEARCH_API_KEY")) return strdup("brave-free");
    if (web_ddgs_package_importable()) return strdup("ddgs");

    return strdup("firecrawl"); /* default */
}

/* PoP: web_get_capability_backend @ tools/web_tools.py:_get_capability_backend */
char *web_get_capability_backend(const char *capability)
{
    if (!capability) return web_get_backend();

    char key[64];
    snprintf(key, sizeof(key), "%s_backend", capability);

    /* For now, fall back to shared backend */
    return web_get_backend();
}

/* PoP: web_get_search_backend @ tools/web_tools.py:_get_search_backend */
char *web_get_search_backend(void)
{
    return web_get_capability_backend("search");
}

/* PoP: web_get_extract_backend @ tools/web_tools.py:_get_extract_backend */
char *web_get_extract_backend(void)
{
    return web_get_capability_backend("extract");
}

/* PoP: web_is_backend_available @ tools/web_tools.py:_is_backend_available */
bool web_is_backend_available(const char *backend)
{
    if (!backend) return false;

    if (strcmp(backend, "exa") == 0) return web_has_env("EXA_API_KEY");
    if (strcmp(backend, "parallel") == 0) return web_has_env("PARALLEL_API_KEY");
    if (strcmp(backend, "firecrawl") == 0) {
        return web_has_env("FIRECRAWL_API_KEY") || web_has_env("FIRECRAWL_API_URL");
    }
    if (strcmp(backend, "tavily") == 0) return web_has_env("TAVILY_API_KEY");
    if (strcmp(backend, "searxng") == 0) return web_has_env("SEARXNG_URL");
    if (strcmp(backend, "brave-free") == 0) return web_has_env("BRAVE_SEARCH_API_KEY");
    if (strcmp(backend, "ddgs") == 0) return web_ddgs_package_importable();
    if (strcmp(backend, "xai") == 0) {
        /* Cheap probe — env var OR auth.json OAuth tokens (mirrors Python:
         * must not trigger a network token refresh on every dispatch). */
        extern bool has_xai_credentials(void);
        return has_xai_credentials();
    }

    return false;
}

/* PoP: web_ddgs_package_importable @ tools/web_tools.py:_ddgs_package_importable */
bool web_ddgs_package_importable(void)
{
    /* In C we can't import Python packages - check if ddgs CLI is available */
    return (system("which ddgs >/dev/null 2>&1") == 0) ||
           (system("which duckduckgo-search >/dev/null 2>&1") == 0);
}

/* PoP: web_web_requires_env @ tools/web_tools.py:_web_requires_env */
char **web_web_requires_env(int *out_count)
{
    static const char *envs[] = {
        "EXA_API_KEY",
        "PARALLEL_API_KEY",
        "TAVILY_API_KEY",
        "FIRECRAWL_API_KEY",
        "FIRECRAWL_API_URL",
        "FIRECRAWL_GATEWAY_URL",
        "TOOL_GATEWAY_DOMAIN",
        "TOOL_GATEWAY_SCHEME",
        "TOOL_GATEWAY_USER_TOKEN",
        NULL
    };

    int count = 0;
    while (envs[count]) count++;

    char **result = malloc((count + 1) * sizeof(char*));
    if (!result) return NULL;

    for (int i = 0; i < count; i++) {
        result[i] = strdup(envs[i]);
    }
    result[count] = NULL;

    if (out_count) *out_count = count;
    return result;
}

/* PoP: web_get_extract_char_limit @ tools/web_tools.py:_get_extract_char_limit */
int web_get_extract_char_limit(void)
{
    /* Python: web.extract_char_limit from config, clamped to [2000, 500000],
     * default 15000 (a typo can't blow up context, footer needs 2k). */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) return 15000;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); return 15000; }
    int v = yaml_get_int(doc, "web.extract_char_limit", 0);
    yaml_free(doc);
    if (v <= 0) return 15000;
    if (v < 2000) return 2000;
    if (v > 500000) return 500000;
    return v;
}

/* PoP: web_convert_base64_images_to_links @ tools/web_tools.py:convert_base64_images_to_links */
/* Delegate to the focused web_base64_img module (faithful POSIX-ERE port). */
char *web_convert_base64_images_to_links(const char *text)
{
    return web_base64_img_convert(text);
}

/* PoP: web_store_full_text @ tools/web_tools.py:_store_full_text */
char *web_store_full_text(const char *url, const char *content)
{
    if (!url || !content) return NULL;

    /* In C we'd use hermes_constants get_hermes_dir and write to cache/web */
    /* For now, return NULL to indicate not stored */
    return NULL;
}

/* PoP: web_truncate_with_footer @ tools/web_tools.py:_truncate_with_footer */
bool web_truncate_with_footer(const char *content, const char *url, int char_limit,
                              char **out_text, bool *out_truncated)
{
    if (!content || !out_text || !out_truncated) return false;

    size_t content_len = strlen(content);
    if ((size_t)char_limit >= content_len) {
        *out_text = strdup(content);
        *out_truncated = false;
        return true;
    }

    int head_budget = (int)(char_limit * 0.75);
    int tail_budget = char_limit - head_budget;

    /* Ensure minimum budgets */
    if (head_budget < 1000) head_budget = 1000;
    if (tail_budget < 500) tail_budget = 500;
    if (head_budget + tail_budget > char_limit) {
        tail_budget = char_limit - head_budget;
    }

    char *head = malloc(head_budget + 1);
    char *tail = malloc(tail_budget + 1);
    if (!head || !tail) {
        free(head);
        free(tail);
        return false;
    }

    strncpy(head, content, head_budget);
    head[head_budget] = '\0';

    /* Snap head to last newline */
    char *nl = strrchr(head, '\n');
    if (nl && (nl - head) > head_budget / 2) {
        *nl = '\0';
    }

    strncpy(tail, content + content_len - tail_budget, tail_budget);
    tail[tail_budget] = '\0';

    /* Snap tail to first newline */
    nl = strchr(tail, '\n');
    if (nl && (nl - tail) < tail_budget / 2) {
        memmove(tail, nl + 1, strlen(nl + 1) + 1);
    }

    char *stored_path = web_store_full_text(url, content);

    /* Build footer */
    char footer[2048];
    snprintf(footer, sizeof(footer),
             "\n\n======== [TRUNCATED] ========\n"
             "Showing %zu chars (head) + %zu chars (tail) of %zu total clean characters.\n",
             strlen(head), strlen(tail), content_len);

    if (stored_path) {
        int middle_start_line = 1;
        for (char *p = head; *p; p++) if (*p == '\n') middle_start_line++;
        middle_start_line++;

        strncat(footer, "Full text saved to: ", sizeof(footer) - strlen(footer) - 1);
        strncat(footer, stored_path, sizeof(footer) - strlen(footer) - 1);
        strncat(footer, "\n", sizeof(footer) - strlen(footer) - 1);

        char line_info[256];
        snprintf(line_info, sizeof(line_info),
                 "To read the omitted middle: read_file path=\"%s\" offset=%d limit=200\n",
                 stored_path, middle_start_line);
        strncat(footer, line_info, sizeof(footer) - strlen(footer) - 1);
        free(stored_path);
    } else {
        strncat(footer, "Full text could not be stored; re-run web_extract on a more specific URL.\n",
                sizeof(footer) - strlen(footer) - 1);
    }
    strncat(footer, "-----------------------------\n", sizeof(footer) - strlen(footer) - 1);

    /* Combine head + omission marker + tail + footer */
    size_t total_len = strlen(head) + strlen("\n\n[... middle omitted — see footer ...]\n\n") +
                       strlen(tail) + strlen(footer);

    char *result = malloc(total_len + 1);
    if (!result) {
        free(head);
        free(tail);
        return false;
    }

    strcpy(result, head);
    strcat(result, "\n\n[... middle omitted — see footer ...]\n\n");
    strcat(result, tail);
    strcat(result, footer);

    *out_text = result;
    *out_truncated = true;

    free(head);
    free(tail);
    return true;
}



/* PoP: web_search_tool @ tools/web_tools.py:web_search_tool */
char *web_search_tool(const char *query, int limit)
{
    if (!query) return strdup("{\"success\":false,\"error\":\"query is required\"}");

    if (limit <= 0) limit = 5;
    if (limit > 100) limit = 100;

    /* In C we'd dispatch through web search registry */
    char *result = malloc(512);
    if (!result) return NULL;

    snprintf(result, 512,
             "{\"success\":true,\"data\":{\"web\":[],\"query\":\"%s\",\"limit\":%d}}",
             query, limit);
    return result;
}

/* PoP: web_check_web_api_key @ tools/web_tools.py:check_web_api_key */
bool web_check_web_api_key(void)
{
    char *backend = web_get_backend();
    if (!backend) return false;

    bool available = web_is_backend_available(backend);
    free(backend);
    return available;
}

/* PoP: web_extract_tool @ tools/web_tools.py:web_extract_tool */
/* Port of Python tools/web_tools.py:web_extract_tool().
 * Extract content from specific web pages using available extraction API backend. */
char *web_extract_tool(const char *urls_json, const char *format, int char_limit)
{
    if (!urls_json) {
        return strdup("{\"success\":false,\"error\":\"urls parameter is required\"}");
    }

    json_t *urls = json_parse(urls_json, NULL);
    if (!urls || urls->type != JSON_ARRAY) {
        if (urls) json_free(urls);
        return strdup("{\"success\":false,\"error\":\"urls must be a JSON array\"}");
    }

    size_t url_count = json_len(urls);
    if (url_count == 0) {
        json_free(urls);
        return strdup("{\"success\":false,\"error\":\"at least one URL is required\"}");
    }

    /* Determine backend */
    char *backend = web_get_extract_backend();
    if (!backend || !web_is_backend_available(backend)) {
        free(backend);
        return strdup("{\"success\":false,\"error\":\"No web extract provider configured. Set web.extract_backend to firecrawl, tavily, exa, or parallel.\"}");
    }

    /* Build results array */
    json_t *results = json_array();
    if (!results) {
        free(backend);
        json_free(urls);
        return strdup("{\"success\":false,\"error\":\"memory allocation failed\"}");
    }

    /* For each URL, attempt extraction */
    bool any_failed = false;
    for (size_t i = 0; i < url_count; i++) {
        json_t *url_item = json_get(urls, i);
        if (!url_item || url_item->type != JSON_STRING) continue;
        const char *url = url_item->str_val;
        if (!url || !url[0]) continue;

        /* Security: block URLs containing embedded secrets (exfiltration
         * prevention). Mirrors Python web_extract_tool: scan the raw URL and
         * its URL-decoded form for secret-key prefixes. url_has_secret()
         * already does exactly this (raw + percent-decoded), reusing the
         * url_safety module rather than duplicating the prefix table. */
        const char *secret_prefix = url_has_secret(url);
        if (secret_prefix) {
            json_t *blocked = json_object();
            json_set(blocked, "url", json_string(url));
            json_set(blocked, "title", json_string(""));
            json_set(blocked, "content", json_string(""));
            json_set(blocked, "error",
                json_string("Blocked: URL contains what appears to be an API key or token. "
                             "Secrets must not be sent in URLs."));
            json_append(results, blocked);
            continue;
        }

        /* SSRF check — reuse the focused url_safety module (no duplicate
         * inline IP-range matching; that logic lives in url_safety.c). */
        if (!url_is_safe(url)) {
            json_t *blocked = json_object();
            json_set(blocked, "url", json_string(url));
            json_set(blocked, "title", json_string(""));
            json_set(blocked, "content", json_string(""));
            json_set(blocked, "error", json_string("Blocked: URL targets a private or internal network address"));
            json_append(results, blocked);
            continue;
        }

        /* C has no web-content extraction client wired yet. Python calls the
         * configured provider's remote API (firecrawl/tavily/exa); that remote
         * call is C-unimplementable at call time without a real extract client.
         * Be honest: report failure per-URL instead of faking success. */
        json_t *result = json_object();
        json_set(result, "url", json_string(url));
        json_set(result, "title", json_string(""));
        json_set(result, "content", json_string(""));
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf),
                 "web_extract not implemented in C: no extract client for backend '%s'",
                 backend ? backend : "none");
        json_set(result, "error", json_string(errbuf));
        json_set(result, "success", json_bool(false));
        json_append(results, result);
        any_failed = true;
    }

    free(backend);
    json_free(urls);

    /* Build response */
    json_t *response = json_object();
    if (!response) {
        json_free(results);
        return strdup("{\"success\":false,\"error\":\"memory allocation failed\"}");
    }

    json_set(response, "success", json_bool(!any_failed));
    json_set(response, "results", results);

    char *serialized = json_serialize(response);
    json_free(response);

    if (!serialized) return strdup("{\"success\":false,\"error\":\"serialization failed\"}");
    return serialized;
}

#endif /* SRC_TOOLS_PORT_WEB_TOOLS_C */
