/*
 * tool_idempotency.c — implementation. Backed by libomap (string key -> entry).
 * See tool_idempotency.h for the contract.
 */
#include "tool_idempotency.h"
#include "hermes_json.h"   /* json_parse / json_get_str / json_free / json_serialize */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>        /* snprintf, fopen */
#include <time.h>

#include "omap.h"         /* libomap: omap_new/set/get/pop/erase/free */

struct tool_idem_entry {
    char *result;     /* strdup'd */
    time_t expiry;    /* epoch seconds */
};

struct tool_idem_cache {
    omap_t *map;      /* key -> struct tool_idem_entry* */
    char *persist_path;  /* JSON file for restart-survival, or NULL */
};

/* Default TTL when caller passes 0. */
#define IDEM_DEFAULT_TTL 600

static void entry_free(void *p) {
    struct tool_idem_entry *e = (struct tool_idem_entry *)p;
    if (!e) return;
    free(e->result);
    free(e);
}

/* Atomically rewrite the persistence file with all live entries. Best-effort:
 * any failure is silently ignored (in-memory cache remains authoritative). */
static void idem_save(const tool_idem_cache_t *c) {
    if (!c || !c->persist_path) return;
    FILE *f = fopen(c->persist_path, "wb");
    if (!f) return;
    /* JSON array of {"key":..,"result":..,"expiry":..} */
    fprintf(f, "[");
    /* omap is ordered; iterate by index via omap_at. */
    size_t n = omap_size(c->map);
    for (size_t i = 0; i < n; i++) {
        const char *k; void *v;
        if (!omap_at(c->map, i, &k, &v)) continue;
        struct tool_idem_entry *e = (struct tool_idem_entry *)v;
        if (e->expiry <= time(NULL)) continue;  /* skip expired */
        if (i > 0) fprintf(f, ",");
        /* keys/results are JSON-safe-ish; escape minimal double-quotes. */
        fprintf(f, "{\"key\":\"");
        for (const char *p = k; *p; p++) {
            if (*p == '"') fputc('\\', f);
            fputc(*p, f);
        }
        fprintf(f, "\",\"result\":\"");
        for (const char *p = e->result; *p; p++) {
            if (*p == '"') fputc('\\', f);
            fputc(*p, f);
        }
        fprintf(f, "\",\"expiry\":%lld}", (long long)e->expiry);
    }
    fprintf(f, "]");
    fclose(f);
}

/* Load non-expired entries from the persistence file into the cache. */
static void idem_load(tool_idem_cache_t *c, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    json_node_t *root = json_parse(buf, NULL);
    free(buf);
    if (!root || root->type != JSON_ARRAY) { if (root) json_free(root); return; }
    size_t len = json_len(root);
    for (size_t i = 0; i < len; i++) {
        json_node_t *item = json_get(root, (int)i);
        if (!item || item->type != JSON_OBJECT) continue;
        const char *k = json_get_str(item, "key", NULL);
        const char *r = json_get_str(item, "result", NULL);
        if (!k || !r) continue;
        long long exp = (long long)json_get_num(item, "expiry", 0.0);
        if (exp <= time(NULL)) continue;  /* expired on load */
        struct tool_idem_entry *e = (struct tool_idem_entry *)calloc(1, sizeof(*e));
        if (!e) continue;
        e->result = strdup(r);
        e->expiry = (time_t)exp;
        if (!e->result) { free(e); continue; }
        omap_set(c->map, k, e);  /* takes ownership */
    }
    json_free(root);
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
    if (c->persist_path) {
        /* Final flush so a clean shutdown persists live entries. */
        idem_save(c);
        free(c->persist_path);
    }
    omap_free(c->map);   /* omap_free invokes entry_free on each value */
    free(c);
}

void tool_idem_attach(tool_idem_cache_t *c, const char *path) {
    if (!c) return;
    if (c->persist_path) { free(c->persist_path); c->persist_path = NULL; }
    if (!path) return;
    c->persist_path = strdup(path);
    if (c->persist_path) idem_load(c, path);
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
    idem_save(c);   /* persist (best-effort) so cache survives restart */
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
