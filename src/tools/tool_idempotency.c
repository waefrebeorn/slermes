/*
 * tool_idempotency.c — implementation. Backed by libomap (string key -> entry).
 * See tool_idempotency.h for the contract.
 */
#include "tool_idempotency.h"
#include "hermes_json.h"   /* json_parse / json_get_str / json_free */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>        /* snprintf */
#include <time.h>

#include "omap.h"         /* libomap: omap_new/set/get/pop/erase/free */

struct tool_idem_entry {
    char *result;     /* strdup'd */
    time_t expiry;    /* epoch seconds */
};

struct tool_idem_cache {
    omap_t *map;      /* key -> struct tool_idem_entry* */
};

/* Default TTL when caller passes 0. */
#define IDEM_DEFAULT_TTL 600

static void entry_free(void *p) {
    struct tool_idem_entry *e = (struct tool_idem_entry *)p;
    if (!e) return;
    free(e->result);
    free(e);
}

tool_idem_cache_t *tool_idem_new(void) {
    tool_idem_cache_t *c = (tool_idem_cache_t *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->map = omap_new(entry_free);
    if (!c->map) { free(c); return NULL; }
    return c;
}

void tool_idem_free(tool_idem_cache_t *c) {
    if (!c) return;
    omap_free(c->map);   /* omap_free invokes entry_free on each value */
    free(c);
}

char *tool_idem_key(const char *tool, const char *args) {
    if (!tool) tool = "";
    if (!args) args = "";
    /* If the caller handed us an explicit idempotency_key, prefer it. */
    const char *explicit = NULL;
    json_node_t *root = json_parse(args, NULL);
    if (root) {
        explicit = json_get_str(root, "idempotency_key", NULL);
    }
    if (explicit && *explicit) {
        size_t n = strlen(tool) + 1 + strlen(explicit) + 1;
        char *k = (char *)malloc(n);
        if (k) snprintf(k, n, "%s|%s", tool, explicit);
        if (root) json_free(root);
        return k;
    }
    if (root) json_free(root);
    /* Fall back to tool + raw args. */
    size_t n = strlen(tool) + 1 + strlen(args) + 1;
    char *k = (char *)malloc(n);
    if (k) snprintf(k, n, "%s|%s", tool, args);
    return k;
}

bool tool_idem_put(tool_idem_cache_t *c, const char *key,
                   const char *result, int ttl_sec) {
    if (!c || !key || !result) return false;
    struct tool_idem_entry *e = (struct tool_idem_entry *)calloc(1, sizeof(*e));
    if (!e) return false;
    e->result = strdup(result);
    if (!e->result) { free(e); return false; }
    e->expiry = time(NULL) + (ttl_sec > 0 ? ttl_sec : IDEM_DEFAULT_TTL);
    /* omap_set replaces and invokes the destructor on the displaced value. */
    if (!omap_set(c->map, key, e)) {
        entry_free(e);
        return false;
    }
    return true;
}

char *tool_idem_get(tool_idem_cache_t *c, const char *key) {
    if (!c || !key) return NULL;
    struct tool_idem_entry *e = (struct tool_idem_entry *)omap_get(c->map, key);
    if (!e) return NULL;
    if (e->expiry <= time(NULL)) {
        /* Expired: drop it (pop so the destructor frees it) and report absent. */
        omap_pop(c->map, key);
        return NULL;
    }
    return strdup(e->result);
}

bool tool_idem_invalidate(tool_idem_cache_t *c, const char *key) {
    if (!c || !key) return false;
    return omap_erase(c->map, key);  /* erase invokes entry_free */
}

size_t tool_idem_count(const tool_idem_cache_t *c) {
    if (!c) return 0;
    return omap_size(c->map);
}
