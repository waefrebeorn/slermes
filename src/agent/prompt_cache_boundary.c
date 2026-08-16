/*
 * port_prompt_cache_boundary.c — Port of agent/prompt_cache_boundary.py.
 *
 * Builder-declared stable prefixes for Anthropic prompt caching (#81867).
 * A process-local, thread-safe LRU registry of scaffold prefixes. The cache
 * planner consults it to place a cache breakpoint at a builder-declared
 * boundary instead of caching a whole (scaffold+tail) user message.
 *
 * - OrderedDict[0] = oldest, [-1] = newest (LRU via move_to_end on access/hit).
 * - Count cap: _MAX_ENTRIES = 32 (popitem(last=False) when exceeded).
 * - Char cap: _MAX_CHARS = 4 MiB retained (popitem(last=False) while over).
 * - find_stable_prefix returns the LONGEST registered prefix that is a proper
 *   prefix of `content` (len(prefix) < len(content), so the split never yields
 *   an empty volatile text block).
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define PCACHE_BOUNDARY_MAX_ENTRIES 32
#define PCACHE_BOUNDARY_MAX_CHARS (4 * 1024 * 1024)

struct pcache_prefix_entry {
    char  *prefix;     /* owned copy */
    size_t len;        /* strlen(prefix) (cached)  */
};

struct pcache_prefix_registry {
    struct pcache_prefix_entry entries[PCACHE_BOUNDARY_MAX_ENTRIES];
    size_t count;
    pthread_mutex_t lock;
};

static struct pcache_prefix_registry g_pcache = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

/* PoP: pcache_register_stable_prefix @ agent/prompt_cache_boundary.py:register_stable_prefix */
void pcache_register_stable_prefix(const char *prefix)
{
    if (!prefix || !*prefix) return;
    pthread_mutex_lock(&g_pcache.lock);
    size_t plen = strlen(prefix);
    /* move_to_end semantics: remove existing entry if present, re-insert newest */
    for (size_t i = 0; i < g_pcache.count; i++) {
        if (g_pcache.entries[i].len == plen &&
            strcmp(g_pcache.entries[i].prefix, prefix) == 0) {
            /* shift the entry (and everything after) up, then drop the tail */
            free(g_pcache.entries[i].prefix);
            for (size_t j = i; j + 1 < g_pcache.count; j++)
                g_pcache.entries[j] = g_pcache.entries[j + 1];
            g_pcache.count--;
            break;
        }
    }
    /* eviction to honour count cap */
    while (g_pcache.count >= PCACHE_BOUNDARY_MAX_ENTRIES) {
        free(g_pcache.entries[0].prefix);
        for (size_t j = 0; j + 1 < g_pcache.count; j++)
            g_pcache.entries[j] = g_pcache.entries[j + 1];
        g_pcache.count--;
    }
    /* eviction to honour char cap (keep newest; evict oldest first) */
    size_t total = 0;
    for (size_t i = 0; i < g_pcache.count; i++) total += g_pcache.entries[i].len;
    total += plen;
    while (g_pcache.count > 1 && total > PCACHE_BOUNDARY_MAX_CHARS) {
        total -= g_pcache.entries[0].len;
        free(g_pcache.entries[0].prefix);
        for (size_t j = 0; j + 1 < g_pcache.count; j++)
            g_pcache.entries[j] = g_pcache.entries[j + 1];
        g_pcache.count--;
    }
    /* append newest */
    char *copy = strdup(prefix);
    if (!copy) goto out;
    g_pcache.entries[g_pcache.count].prefix = copy;
    g_pcache.entries[g_pcache.count].len = plen;
    g_pcache.count++;
out:
    pthread_mutex_unlock(&g_pcache.lock);
}

/* PoP: pcache_find_stable_prefix @ agent/prompt_cache_boundary.py:find_stable_prefix */
/* Returns the longest registered prefix that is a PROPER prefix of content
 * (len(prefix) < len(content)). Returns a heap-allocated copy the caller must
 * free, or NULL on no match. A hit refreshes the entry's LRU position. */
char *pcache_find_stable_prefix(const char *content)
{
    if (!content) return NULL;
    pthread_mutex_lock(&g_pcache.lock);
    size_t clen = strlen(content);
    ssize_t best_idx = -1;
    size_t best_len = 0;
    for (size_t i = 0; i < g_pcache.count; i++) {
        size_t plen = g_pcache.entries[i].len;
        if (clen > plen && plen > best_len &&
            strncmp(content, g_pcache.entries[i].prefix, plen) == 0) {
            best_idx = (ssize_t)i;
            best_len = plen;
        }
    }
    char *result = NULL;
    if (best_idx >= 0) {
        result = strdup(g_pcache.entries[best_idx].prefix);
        /* move_to_end: shift entry to newest position AFTER the scan */
        struct pcache_prefix_entry tmp = g_pcache.entries[best_idx];
        for (ssize_t j = best_idx; j + 1 < (ssize_t)g_pcache.count; j++)
            g_pcache.entries[j] = g_pcache.entries[j + 1];
        g_pcache.entries[g_pcache.count - 1] = tmp;
    }
    pthread_mutex_unlock(&g_pcache.lock);
    return result;
}

/* PoP: pcache_clear_stable_prefixes @ agent/prompt_cache_boundary.py:clear_stable_prefixes */
void pcache_clear_stable_prefixes(void)
{
    pthread_mutex_lock(&g_pcache.lock);
    for (size_t i = 0; i < g_pcache.count; i++) {
        free(g_pcache.entries[i].prefix);
        g_pcache.entries[i].prefix = NULL;
        g_pcache.entries[i].len = 0;
    }
    g_pcache.count = 0;
    pthread_mutex_unlock(&g_pcache.lock);
}
