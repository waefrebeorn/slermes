/*
 * web_search.h - web search provider backends.
 *
 * Module-internal header (web.c <-> web_search.c).
 */
#ifndef WEB_SEARCH_H
#define WEB_SEARCH_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_http.h"   /* http_client_t */

char * search_brave(http_client_t *client, const char *query, int count);
char * search_firecrawl(http_client_t *client, const char *query, int count);
char * search_google(http_client_t *client, const char *query, int count);
char * search_searxng(http_client_t *client, const char *query, int count, const char *lang);
char * search_tavily(http_client_t *client, const char *query, int count);
char * search_xai(http_client_t *client, const char *query, int count);

#endif /* WEB_SEARCH_H */
