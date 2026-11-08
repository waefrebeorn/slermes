/*
 * web_search.c - web search provider backends.
 *
 * Split from web.c (soul.md: self-contained modules). One function per
 * provider (google, searxng, brave, tavily, firecrawl, xai). No file-scope
 * state.
 */
#include "web_search.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_tool_config.h"
#include "base64.h"
#include "hermes_url_safety.h"
#include "html.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>


/* ================================================================
 *  Web search handler (F16-F20)
 * ================================================================ */

/* Perform a SearXNG search */ char *search_searxng(http_client_t *client, const char *query, int count, const char *lang) {
    const char *base_url = tool_config_get("searxng", "base_url");
    if (!base_url) base_url = getenv("SEARXNG_BASE_URL");
    if (!base_url) return strdup("{\"error\":\"SearXNG: SEARXNG_BASE_URL not set\"}");

    char url[1024];
    int n = snprintf(url, sizeof(url), "%s/search?q=%s&format=json&limit=%d",
             base_url, query, count);
    if (lang && lang[0]) {
        snprintf(url + n, sizeof(url) - n, "&lang=%s", lang);
    }

    http_response_t *resp = http_request(client, HTTP_GET, url, NULL, NULL, 0);
    if (!resp) return strdup("{\"error\":\"SearXNG: request failed\"}");

    /* Parse JSON response */
    char *out = NULL;
    if (resp->body) {
        /* Pass through the JSON results */
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("searxng"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"SearXNG: empty response\"}");
}


/* Perform a Google Custom Search */ char *search_google(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("google", "api_key");
    const char *cx = tool_config_get("google", "cx");
    if (!api_key) api_key = getenv("GOOGLE_API_KEY");
    if (!cx) cx = getenv("GOOGLE_CX");
    if (!api_key || !cx) return strdup("{\"error\":\"Google: GOOGLE_API_KEY and GOOGLE_CX required\"}");

    char url[2048];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%d",
             api_key, cx, query, count > 10 ? 10 : count);

    http_response_t *resp = http_request(client, HTTP_GET, url, NULL, NULL, 0);
    if (!resp) return strdup("{\"error\":\"Google: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("google"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Google: empty response\"}");
}


/* Perform a Brave Search */ char *search_brave(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("brave", "api_key");
    if (!api_key) api_key = getenv("BRAVE_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Brave: BRAVE_API_KEY not set\"}");

    char url[1024];
    snprintf(url, sizeof(url),
             "https://api.search.brave.com/res/v1/web/search?q=%s&count=%d",
             query, count);

    char headers[256];
    snprintf(headers, sizeof(headers), "Accept: application/json, X-Subscription-Token: %s", api_key);

    http_response_t *resp = http_request(client, HTTP_GET, url, headers, NULL, 0);
    if (!resp) return strdup("{\"error\":\"Brave: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("brave"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Brave: empty response\"}");
}


/* Perform a Tavily search */ char *search_tavily(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("tavily", "api_key");
    if (!api_key) api_key = getenv("TAVILY_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Tavily: TAVILY_API_KEY not set\"}");

    json_node_t *body = json_new_object();
    json_object_set(body, "api_key", json_new_string(api_key));
    json_object_set(body, "query", json_new_string(query));
    json_object_set(body, "max_results", json_new_number((double)count));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(client, HTTP_POST,
        "https://api.tavily.com/search", payload);
    free(payload);

    if (!resp) return strdup("{\"error\":\"Tavily: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("tavily"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Tavily: empty response\"}");
}


/* Perform a Firecrawl scrape */ char *search_firecrawl(http_client_t *client, const char *query, int count) {
    (void)count;
    const char *api_key = tool_config_get("firecrawl", "api_key");
    if (!api_key) api_key = getenv("FIRECRAWL_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Firecrawl: FIRECRAWL_API_KEY not set\"}");

    /* Firecrawl is URL-based scraping — treat query as URL or use search endpoint */
    json_node_t *body = json_new_object();
    json_object_set(body, "url", json_new_string(query));

    char *payload = json_serialize(body);
    json_free(body);

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s, Content-Type: application/json", api_key);

    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.firecrawl.dev/v1/scrape", auth_header, payload, strlen(payload));
    free(payload);

    if (!resp) return strdup("{\"error\":\"Firecrawl: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("firecrawl"));
        json_object_set(result, "url", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Firecrawl: empty response\"}");
}


/* L03: xAI Web Search via Responses API */ char *search_xai(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("xai", "api_key");
    if (!api_key) api_key = getenv("XAI_API_KEY");
    if (!api_key) return strdup("{\"error\":\"xAI: XAI_API_KEY not set\"}");

    char headers[256];
    snprintf(headers, sizeof(headers), "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    json_node_t *body = json_new_object();
    json_object_set(body, "model", json_new_string("grok-4.3"));
    json_object_set(body, "input", json_new_string(query));
    json_t *tools = json_new_array();
    json_t *ws_tool = json_new_object();
    json_object_set(ws_tool, "type", json_new_string("web_search"));
    json_array_append(tools, ws_tool);
    json_object_set(body, "tools", tools);
    json_object_set(body, "max_results", json_new_number((double)count));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.x.ai/v1/responses", headers, payload, strlen(payload));
    free(payload);

    if (!resp) return strdup("{\"error\":\"xAI: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("xai"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"xAI: empty response\"}");
}
