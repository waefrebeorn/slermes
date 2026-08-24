/*
 * port_agent_web_search_provider.c — C port of agent/web_search_provider.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_web_search_provider_display_name @ agent/web_search_provider.py:display_name */

/* Port of Python agent/web_search_provider.py:display_name */
/* Return the display name for a web search provider. */
const char *cli_agent_web_search_provider_display_name(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return "Web Search";

    if (strcmp(provider_id, "brave") == 0) return "Brave Search";
    if (strcmp(provider_id, "tavily") == 0) return "Tavily";
    if (strcmp(provider_id, "serper") == 0) return "Serper";
    if (strcmp(provider_id, "firecrawl") == 0) return "Firecrawl Search";
    if (strcmp(provider_id, "x") == 0) return "X (Twitter) Search";

    hermes_log(LOG_DEBUG, "port",
               "web_search_provider: unknown provider '%s', using default", provider_id);
    return "Web Search";
}

/* PoP: cli_agent_web_search_provider_supports_search @ agent/web_search_provider.py:supports_search */

/* Port of Python agent/web_search_provider.py:supports_search */
/* Check if a provider supports web search. Returns 1 if yes, 0 if no. */
int cli_agent_web_search_provider_supports_search(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return 0;

    if (strcmp(provider_id, "brave") == 0) return 1;
    if (strcmp(provider_id, "tavily") == 0) return 1;
    if (strcmp(provider_id, "serper") == 0) return 1;
    if (strcmp(provider_id, "firecrawl") == 0) return 1;
    if (strcmp(provider_id, "x") == 0) return 1;

    hermes_log(LOG_DEBUG, "port",
               "web_search_provider: unknown provider '%s', assuming no search", provider_id);
    return 0;
}

/* PoP: cli_agent_web_search_provider_supports_extract @ agent/web_search_provider.py:supports_extract */

/* Port of Python agent/web_search_provider.py:supports_extract */
/* Check if a provider supports content extraction. Returns 1 if yes, 0 if no. */
int cli_agent_web_search_provider_supports_extract(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return 0;

    if (strcmp(provider_id, "firecrawl") == 0) return 1;
    if (strcmp(provider_id, "tavily") == 0) return 1;
    if (strcmp(provider_id, "brave") == 0) return 0;
    if (strcmp(provider_id, "serper") == 0) return 0;
    if (strcmp(provider_id, "x") == 0) return 0;

    hermes_log(LOG_DEBUG, "port",
               "web_search_provider: unknown provider '%s', assuming no extract", provider_id);
    return 0;
}

/* PoP: cli_agent_web_search_provider_get_setup_schema @ agent/web_search_provider.py:get_setup_schema */

/* Port of Python agent/web_search_provider.py:get_setup_schema */
/* Return the setup schema for a web search provider. Returns JSON object. */
char *cli_agent_web_search_provider_get_setup_schema(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    if (strcmp(provider_id, "brave") == 0) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"Brave Search API key\"},"
                "\"country\":{\"type\":\"string\",\"description\":\"2-letter country code\"},"
                "\"language\":{\"type\":\"string\",\"description\":\"Language code\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    if (strcmp(provider_id, "tavily") == 0) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"Tavily API key\"},"
                "\"max_results\":{\"type\":\"integer\",\"description\":\"Max results per query\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    if (strcmp(provider_id, "firecrawl") == 0) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"Firecrawl API key\"},"
                "\"base_url\":{\"type\":\"string\",\"description\":\"Custom base URL\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    return strdup("{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"}"
        "},"
        "\"required\":[\"api_key\"]"
    "}");
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_web_search_provider_get_provider_env @ agent/web_search_provider.py:get_provider_env */
int agent_web_search_provider_get_provider_env(const char *arg) {
    /* Python: config-aware env lookup. Arg = "name\tstate\tvalue". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}
