
/* PoP: get_provider_env @ agent/web_search_provider.py:get_provider_env */
void *agent_web_search_provider_get_provider_env(void) { return NULL; }

/* PoP: _secret_scope_state @ agent/web_search_provider.py:_secret_scope_state */
void *agent_web_search_provider_secret_scope_state(void) { return NULL; }

/* PoP: is_keyless_available @ agent/web_search_provider.py:is_keyless_available */
int agent_web_search_provider_is_keyless_available(void) { return 0; }
ovider_id)
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
