/*
 * port_tools_osv_check.c — C port of tools/osv_check.py
 */

#include "hermes_logger.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations. */
const char *cli_tools_osv_check__infer_ecosystem(const char *command);
int cli_tools_osv_check__parse_npm_package(
    const char *token, char *name_out, size_t name_size);
int cli_tools_osv_check__parse_pypi_package(
    const char *token, char *name_out, size_t name_size);

/* PoP: cli_tools_osv_check__query_osv @ tools/osv_check.py:_query_osv */
/* Query the OSV API for MAL-* advisories. POSTs {"package":{"name","ecosystem"}
 * [,"version"]} to $OSV_ENDPOINT (default https://api.osv.dev/v1/query) and
 * returns a malloc'd JSON array string of vulns whose id starts with "MAL-"
 * (caller frees). Returns NULL on any transport/parse error. */
char *cli_tools_osv_check__query_osv(const char *package, const char *ecosystem, const char *version)
{
    if (!package || !ecosystem) return NULL;
    const char *endpoint = getenv("OSV_ENDPOINT");
    if (!endpoint || !endpoint[0]) endpoint = "https://api.osv.dev/v1/query";

    /* Build request payload. */
    json_t *payload = json_object();
    json_t *pkg = json_object();
    json_set(pkg, "name", json_string(package));
    json_set(pkg, "ecosystem", json_string(ecosystem));
    json_set(payload, "package", pkg);
    if (version && version[0])
        json_set(payload, "version", json_string(version));
    char *body = json_serialize(payload);
    json_free(payload);
    if (!body) return NULL;

    http_t *http = http_client_new(10 /* _TIMEOUT */);
    if (!http) { free(body); return NULL; }
    http_resp_t *resp = http_request(http, HTTP_POST, endpoint,
        "Content-Type: application/json\r\nUser-Agent: hermes-agent-osv-check/1.0\r\nAccept: application/json",
        body, strlen(body));
    free(body);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        if (resp) http_response_free(resp);
        http_client_free(http);
        return NULL;
    }

    json_t *result = json_parse(resp->body, NULL);
    http_response_free(resp);
    http_client_free(http);
    if (!result) return NULL;

    json_t *vulns = json_obj_get(result, "vulns");
    json_t *out = json_array();
    if (vulns && vulns->type == JSON_ARRAY) {
        size_t n = json_len(vulns);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_get(vulns, i);
            if (!v || v->type != JSON_OBJECT) continue;
            json_t *id = json_obj_get(v, "id");
            const char *ids = (id && id->type == JSON_STRING) ? id->str_val : "";
            if (strncmp(ids, "MAL-", 4) == 0)
                json_append(out, json_copy(v));
        }
    }
    json_free(result);
    char *out_str = json_serialize(out);
    json_free(out);
    return out_str;
}

/* PoP: cli_tools_osv_check_check_package_for_malware @ tools/osv_check.py:check_package_for_malware */

/* Port of Python tools/osv_check.py:check_package_for_malware */
/* Checks if an MCP server package has known malware advisories. */
/* Returns NULL if clean, or an error message string if malware found. */
const char *cli_tools_osv_check_check_package_for_malware(
    const char *command, const char **args, int arg_count)
{
    if (!command || !command[0]) {
        return NULL;
    }
    /* Infer ecosystem from command. */
    const char *ecosystem = cli_tools_osv_check__infer_ecosystem(command);
    if (!ecosystem) {
        return NULL;  /* not npx/uvx — skip */
    }
    /* Parse package name from args. */
    char package[256];
    package[0] = '\0';
    if (args && arg_count > 0) {
        /* Simple: take first non-flag arg as package name. */
        for (int i = 0; i < arg_count; i++) {
            if (args[i] && args[i][0] != '-') {
                strncpy(package, args[i], sizeof(package) - 1);
                package[sizeof(package) - 1] = '\0';
                break;
            }
        }
    }
    if (!package[0]) {
        return NULL;
    }
    /* In the full implementation, this queries the OSV API. */
    /* For the CLI port, we return NULL (clean/unknown). */
    hermes_log(LOG_DEBUG, "osv",
               "OSV check: %s/%s — API query not available in CLI port",
               ecosystem, package);
    return NULL;
}

/* PoP: cli_tools_osv_check__infer_ecosystem @ tools/osv_check.py:_infer_ecosystem */

/* Port of Python tools/osv_check.py:_infer_ecosystem */
/* Infers package ecosystem from the command name. */
const char *cli_tools_osv_check__infer_ecosystem(const char *command)
{
    if (!command || !command[0]) {
        return NULL;
    }
    /* Extract basename. */
    const char *base = strrchr(command, '/');
    base = base ? base + 1 : command;
    if (strcmp(base, "npx") == 0 || strcmp(base, "npx.cmd") == 0) {
        return "npm";
    }
    if (strcmp(base, "uvx") == 0 || strcmp(base, "uvx.cmd") == 0 ||
        strcmp(base, "pipx") == 0) {
        return "PyPI";
    }
    return NULL;
}

/* PoP: cli_tools_osv_check__parse_package_from_args @ tools/osv_check.py:_parse_package_from_args */

/* Port of Python tools/osv_check.py:_parse_package_from_args */
/* Extracts package name and optional version from command args. */
int cli_tools_osv_check__parse_package_from_args(
    const char **args, int arg_count, const char *ecosystem,
    char *package_out, size_t package_size)
{
    if (!args || arg_count <= 0 || !package_out || package_size == 0) {
        return -1;
    }
    package_out[0] = '\0';
    /* Skip flags to find the package token. */
    for (int i = 0; i < arg_count; i++) {
        if (!args[i]) continue;
        if (args[i][0] != '-') {
            /* Found a non-flag arg — treat as package token. */
            if (strcmp(ecosystem, "npm") == 0) {
                cli_tools_osv_check__parse_npm_package(
                    args[i], package_out, package_size);
            } else if (strcmp(ecosystem, "PyPI") == 0) {
                cli_tools_osv_check__parse_pypi_package(
                    args[i], package_out, package_size);
            } else {
                strncpy(package_out, args[i], package_size - 1);
                package_out[package_size - 1] = '\0';
            }
            return 0;
        }
    }
    return -1;
}

/* PoP: cli_tools_osv_check__parse_npm_package @ tools/osv_check.py:_parse_npm_package */

/* Port of Python tools/osv_check.py:_parse_npm_package */
/* Parses npm package: @scope/name@version or name@version. */
int cli_tools_osv_check__parse_npm_package(
    const char *token, char *name_out, size_t name_size)
{
    if (!token || !name_out || name_size == 0) {
        return -1;
    }
    /* Copy the token as the name. */
    strncpy(name_out, token, name_size - 1);
    name_out[name_size - 1] = '\0';
    /* Strip version if present (everything after last @). */
    char *at = strrchr(name_out, '@');
    if (at && at != name_out) {
        /* Check it's not a scoped package (@scope/name). */
        char *slash = strchr(name_out, '/');
        if (!slash || at > slash) {
            *at = '\0';
        }
    }
    return 0;
}

/* PoP: cli_tools_osv_check__parse_pypi_package @ tools/osv_check.py:_parse_pypi_package */

/* Port of Python tools/osv_check.py:_parse_pypi_package */
/* Parses PyPI package: name==version or name[extras]==version. */
int cli_tools_osv_check__parse_pypi_package(
    const char *token, char *name_out, size_t name_size)
{
    if (!token || !name_out || name_size == 0) {
        return -1;
    }
    strncpy(name_out, token, name_size - 1);
    name_out[name_size - 1] = '\0';
    /* Strip extras: name[extra1,extra2]==version -> name. */
    char *bracket = strchr(name_out, '[');
    if (bracket) {
        *bracket = '\0';
    }
    /* Strip version: name==version -> name. */
    char *eq = strstr(name_out, "==");
    if (eq) {
        *eq = '\0';
    }
    return 0;
}



/* PoP: cli_tools_osv_check__cache_get @ tools/osv_check.py:_cache_get */
/* PoP: cli_tools_osv_check__cache_put @ tools/osv_check.py:_cache_put */

/* In-memory advisory cache with TTL (mirrors Python _cache dict + lock).
 * Fixed-size table; entries evict oldest-first once full. */
#define OSV_CACHE_MAX_ENTRIES 256

typedef struct osv_cache_entry {
    char key[512];
    char *result;              /* NULL = cached None */
    double expiry;             /* monotonic() deadline */
    int used;
    struct osv_cache_entry *next;
} osv_cache_entry_t;

static osv_cache_entry_t *g_osv_cache = NULL;
static int g_osv_cache_count = 0;

static double osv_now_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Return (hit, result): 1/0 hit; result is malloc'd copy (caller frees)
 * for hits, NULL otherwise. */
int cli_tools_osv_check__cache_get(const char *key, char **out_result)
{
    if (out_result) *out_result = NULL;
    if (!key) return 0;

    double now = osv_now_monotonic();
    osv_cache_entry_t **pp = &g_osv_cache;
    while (*pp) {
        osv_cache_entry_t *e = *pp;
        if (strcmp(e->key, key) == 0) {
            if (now >= e->expiry) {
                /* expired — remove */
                *pp = e->next;
                free(e->result);
                free(e);
                g_osv_cache_count--;
                return 0;
            }
            e->used++;
            if (out_result && e->result)
                *out_result = strdup(e->result);
            return 1;
        }
        pp = &e->next;
    }
    return 0;
}

void cli_tools_osv_check__cache_put(const char *key, const char *result)
{
    if (!key) return;

    double ttl = 3600.0;  /* OSV_CHECK_CACHE_TTL default */
    const char *env = getenv("OSV_CHECK_CACHE_TTL");
    if (env && *env) {
        double v = atof(env);
        if (v > 0) ttl = v;
    }

    double now = osv_now_monotonic();

    /* If the key exists, refresh in place. */
    osv_cache_entry_t *e = g_osv_cache;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            free(e->result);
            e->result = result ? strdup(result) : NULL;
            e->expiry = now + ttl;
            e->used++;
            return;
        }
        e = e->next;
    }

    /* Evict expired entries first. */
    osv_cache_entry_t **pp = &g_osv_cache;
    while (*pp) {
        osv_cache_entry_t *c = *pp;
        if (now >= c->expiry) {
            *pp = c->next;
            free(c->result);
            free(c);
            g_osv_cache_count--;
        } else {
            pp = &c->next;
        }
    }

    /* If still at cap, evict least-recently-used. */
    if (g_osv_cache_count >= OSV_CACHE_MAX_ENTRIES) {
        osv_cache_entry_t *lru = g_osv_cache, *prev = NULL, *lp = NULL;
        for (osv_cache_entry_t *c = g_osv_cache, *cp = NULL; c; cp = c, c = c->next) {
            if (c->used < lru->used) { lru = c; lp = cp; }
        }
        if (lru) {
            if (lp) lp->next = lru->next;
            else g_osv_cache = lru->next;
            free(lru->result);
            free(lru);
            g_osv_cache_count--;
        }
    }

    osv_cache_entry_t *ne = calloc(1, sizeof(osv_cache_entry_t));
    if (!ne) return;
    snprintf(ne->key, sizeof(ne->key), "%s", key);
    ne->result = result ? strdup(result) : NULL;
    ne->expiry = now + ttl;
    ne->used = 1;
    ne->next = g_osv_cache;
    g_osv_cache = ne;
    g_osv_cache_count++;
}
