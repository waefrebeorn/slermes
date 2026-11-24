/*
 * provider.c - C11 port of upstream plugins/web/perplexity/provider.py
 * (gate w26_provider_9eacd0).
 *
 * PoP: w26_provider_9eacd0_normalize_search_results @ plugins/web/perplexity/provider.py:_normalize_search_results
 * PoP: w26_provider_9eacd0_normalize_snippets @ plugins/web/perplexity/provider.py:_normalize_snippets
 * PoP: w26_provider_9eacd0_query_for_urls @ plugins/web/perplexity/provider.py:_query_for_urls
 * PoP: w26_provider_9eacd0_constants @ plugins/web/perplexity/provider.py:<limits, endpoints>
 *
 * `httpx.post`, the env key lookup and the managed gateway are seams: the body the
 * Search API answered is a fixed input, so what is ported is exactly how a 200
 * body becomes the tool shape -- the position numbering, the per-URL error
 * documents a partial success produces, and the path-word relevance query.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_provider_9eacd0.h"

/* _DEFAULT_BASE_URL / _KEY_URL */
static const char *const PRX_DEFAULT_BASE_URL = "https://api.perplexity.ai";
static const char *const PRX_KEY_URL = "https://www.perplexity.ai/account/api";

/* _MAX_SEARCH_RESULTS / _MAX_TOKENS / _MAX_TOKENS_PER_PAGE */
#define PRX_MAX_SEARCH_RESULTS 20
#define PRX_MAX_TOKENS 16384
#define PRX_MAX_TOKENS_PER_PAGE 4096
#define PRX_QUERY_MAX 500

/* Python truthiness of a JSON value. */
static int prx_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python `value or ""`: falsy collapses to the empty string, else the value. */
static json_t *prx_or_empty(const json_t *v) {
    return prx_truthy(v) ? json_copy(v) : json_string("");
}

/* str(value) for the scalar shapes a result row carries. */
static char *prx_pystr(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        return strdup(tmp);
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return strdup("[]");
    case JSON_OBJECT: return strdup("{}");
    }
    return strdup("");
}

json_t *w26_provider_9eacd0_normalize_search_results(const json_t *response) {
    const json_t *results;
    json_t *out, *data, *web;
    size_t i;

    results = response && response->type == JSON_OBJECT ? json_obj_get(response, "results") : NULL;
    if (!prx_truthy(results) || results->type != JSON_ARRAY) results = NULL;
    out = json_object();
    json_set(out, "success", json_bool(1));
    data = json_object();
    web = json_array();
    if (results) {
        for (i = 0; i < json_len(results); i++) {
            const json_t *result = json_get(results, i);
            json_t *row = json_object();
            json_set(row, "title", prx_or_empty(json_obj_get(result, "title")));
            json_set(row, "url", prx_or_empty(json_obj_get(result, "url")));
            json_set(row, "description", prx_or_empty(json_obj_get(result, "snippet")));
            json_set(row, "position", json_number((double)(i + 1)));
            json_append(web, row);
        }
    }
    json_set(data, "web", web);
    json_set(out, "data", data);
    return out;
}

/* Python string equality (None == None, else exact bytes). */
static int prx_streq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b) return 0;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* by_url: {r.get("url", ""): r for r in results if isinstance(r, dict)} -- last wins. */
static const json_t *prx_by_url(const json_t *results, const json_t *url) {
    size_t i, n = results ? json_len(results) : 0;
    const json_t *found = NULL;
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(results, i);
        if (!r || r->type != JSON_OBJECT) continue;
        if (prx_streq(json_obj_get(r, "url"), url)) found = r;
    }
    return found;
}

json_t *w26_provider_9eacd0_normalize_snippets(const json_t *req) {
    const json_t *response = json_obj_get(req, "response");
    const json_t *urls = json_obj_get(req, "urls");
    const json_t *results;
    json_t *documents;
    size_t i;

    results = response && response->type == JSON_OBJECT ? json_obj_get(response, "results") : NULL;
    if (!prx_truthy(results) || results->type != JSON_ARRAY) results = NULL;
    documents = json_array();
    if (!urls || urls->type != JSON_ARRAY) return documents;
    for (i = 0; i < json_len(urls); i++) {
        const json_t *url = json_get(urls, i);
        const json_t *result = prx_by_url(results, url);
        const json_t *text = result ? json_obj_get(result, "text") : NULL;
        const json_t *error = result ? json_obj_get(result, "error") : NULL;
        json_t *doc = json_object();
        json_t *metadata = json_object();

        json_set(doc, "url", json_copy(url));
        json_set(doc, "title", json_string(""));
        json_set(doc, "content", prx_or_empty(text));
        json_set(doc, "raw_content", prx_or_empty(text));
        json_set(metadata, "sourceURL", json_copy(url));
        json_set(doc, "metadata", metadata);
        if (prx_truthy(error) || !prx_truthy(text)) {
            if (prx_truthy(error)) {
                char *s = prx_pystr(error);
                json_set(doc, "error", json_string(s));
                free(s);
            } else {
                json_set(doc, "error", json_string("no content returned"));
            }
        }
        json_append(documents, doc);
    }
    return documents;
}

/* urlparse(url): the lowercased netloc and the path, split as Python splits them. */
static void prx_urlparse(const char *url, char *netloc, size_t netloc_sz,
                         const char **path, size_t *path_len) {
    const char *auth, *end, *scheme;
    size_t i, n;

    netloc[0] = '\0';
    *path = "";
    *path_len = 0;
    if (!url) return;
    scheme = strstr(url, "://");
    if (scheme) {
        auth = scheme + 3;
    } else if (url[0] == '/' && url[1] == '/') {
        auth = url + 2;
    } else {
        /* No scheme and no authority: the whole pre-query text is the path. */
        *path = url;
        *path_len = strcspn(url, "?#");
        return;
    }
    end = auth + strcspn(auth, "/?#");
    n = (size_t)(end - auth);
    if (n >= netloc_sz) n = netloc_sz - 1;
    /* urlparse().netloc keeps the authority's original case (only the scheme
     * is lowercased), and the no-path branch appends it verbatim. */
    for (i = 0; i < n; i++) netloc[i] = auth[i];
    netloc[n] = '\0';
    if (*end == '/') {
        *path = end;
        *path_len = strcspn(end, "?#");
    }
}

typedef struct {
    char **items;
    size_t n, cap;
} prx_words;

static int prx_words_has(const prx_words *w, const char *word) {
    size_t i;
    for (i = 0; i < w->n; i++)
        if (strcmp(w->items[i], word) == 0) return 1;
    return 0;
}

static int prx_words_add(prx_words *w, const char *word) {
    char *dup;
    if (w->n == w->cap) {
        size_t cap = w->cap ? w->cap * 2 : 16;
        char **p = (char **)realloc(w->items, cap * sizeof(char *));
        if (!p) return 0;
        w->items = p;
        w->cap = cap;
    }
    dup = strdup(word);
    if (!dup) return 0;
    w->items[w->n++] = dup;
    return 1;
}

/* " ".join(items)[:limit] */
static json_t *prx_join(const char **items, size_t n, size_t limit) {
    char *buf;
    size_t i, len = 0, cap = 64;
    json_t *out;

    for (i = 0; i < n; i++) cap += strlen(items[i]) + 1;
    buf = (char *)malloc(cap);
    if (!buf) return json_null();
    buf[0] = '\0';
    for (i = 0; i < n; i++) {
        size_t l;
        if (i) buf[len++] = ' ';
        l = strlen(items[i]);
        if (len + l >= cap) l = cap - len - 1;
        memcpy(buf + len, items[i], l);
        len += l;
    }
    buf[len] = '\0';
    if (len > limit) buf[limit] = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w26_provider_9eacd0_query_for_urls(const json_t *req) {
    const json_t *urls = json_obj_get(req, "urls");
    prx_words words;
    size_t i, k;
    json_t *out;

    memset(&words, 0, sizeof(words));
    if (!urls || urls->type != JSON_ARRAY) return json_string("");

    for (i = 0; i < json_len(urls); i++) {
        const json_t *u = json_get(urls, i);
        const char *path = "";
        size_t path_len = 0, a, b;
        char netloc[256];
        char *work;
        char *p;

        if (!u || u->type != JSON_STRING) continue;
        prx_urlparse(u->str_val ? u->str_val : "", netloc, sizeof(netloc), &path, &path_len);
        work = (char *)malloc(path_len + 1);
        if (!work) continue;
        for (k = 0; k < path_len; k++) {
            char c = path[k];
            work[k] = (c == '-' || c == '_' || c == '/') ? ' ' : c;
        }
        work[path_len] = '\0';

        p = work;
        while (*p) {
            char *start, *low;
            size_t len, j;
            int digits = 1;

            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            len = (size_t)(p - start);
            low = (char *)malloc(len + 1);
            if (!low) break;
            for (j = 0; j < len; j++) {
                char c = start[j];
                low[j] = (char)tolower((unsigned char)c);
                if (!isdigit((unsigned char)c)) digits = 0;
            }
            low[len] = '\0';
            if (!digits && !prx_words_has(&words, low)) prx_words_add(&words, low);
            free(low);
        }
        free(work);

        /* if not parsed.path.strip("/"): words.append(parsed.netloc) */
        a = 0;
        b = path_len;
        while (a < b && path[a] == '/') a++;
        while (b > a && path[b - 1] == '/') b--;
        /* The netloc is appended WITHOUT the de-duplication the path words get:
         * two bare URLs of the same host contribute it twice. */
        if (a == b && netloc[0]) prx_words_add(&words, netloc);
    }

    if (words.n > 0) {
        out = prx_join((const char **)words.items, words.n, PRX_QUERY_MAX);
    } else {
        size_t n = json_len(urls), have = 0;
        const char **raw = (const char **)calloc(n + 1, sizeof(char *));
        if (!raw) {
            for (i = 0; i < words.n; i++) free(words.items[i]);
            free(words.items);
            return json_null();
        }
        for (i = 0; i < n; i++) {
            const json_t *u = json_get(urls, i);
            raw[have++] = (u && u->type == JSON_STRING) ? (u->str_val ? u->str_val : "") : "";
        }
        out = prx_join(raw, have, PRX_QUERY_MAX);
        free(raw);
    }
    for (i = 0; i < words.n; i++) free(words.items[i]);
    free(words.items);
    return out;
}

json_t *w26_provider_9eacd0_constants(void) {
    char missing[256];
    json_t *o = json_object();
    snprintf(missing, sizeof(missing),
             "PERPLEXITY_API_KEY is not set. Get a key at %s", PRX_KEY_URL);
    json_set(o, "default_base_url", json_string(PRX_DEFAULT_BASE_URL));
    json_set(o, "key_url", json_string(PRX_KEY_URL));
    json_set(o, "max_search_results", json_number((double)PRX_MAX_SEARCH_RESULTS));
    json_set(o, "max_tokens", json_number((double)PRX_MAX_TOKENS));
    json_set(o, "max_tokens_per_page", json_number((double)PRX_MAX_TOKENS_PER_PAGE));
    json_set(o, "missing_key_error", json_string(missing));
    return o;
}
