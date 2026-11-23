/*
 * data_use.c — faithful C11 port of tools/connectors/data_use.py.
 * Gate: connuse.
 * PoP: connuse_begin @ tools/connectors/data_use.py:collect_data_use
 * PoP: connuse_end @ tools/connectors/data_use.py:collect_data_use
 * PoP: connuse_record @ tools/connectors/data_use.py:record_data_use
 *
 * collect_data_use() binds a fresh set into the _sink ContextVar and yields it;
 * record_data_use(requires) does sink.update(requires) when a sink is bound and
 * is a no-op otherwise (execute_code RPC / MCP serve). The sink is an active
 * flag plus a de-duplicated set of strings that survives end(), matching the
 * yielded object the caller keeps after the context exits.
 */
#include "noc_connuse.h"
#include <stdlib.h>
#include <string.h>

#define CONNUSE_CAP 512
static char *g_items[CONNUSE_CAP];
static size_t g_count = 0;
static int g_active = 0;

void connuse_clear(void) {
    for (size_t i = 0; i < g_count; i++) free(g_items[i]);
    g_count = 0;
    g_active = 0;
}

void connuse_begin(void) {
    connuse_clear();
    g_active = 1;
}

void connuse_end(void) { g_active = 0; }

void connuse_record(const char *item) {
    if (!g_active || !item) return;         /* no sink bound -> no-op */
    if (connuse_contains(item)) return;     /* set.update dedups */
    if (g_count >= CONNUSE_CAP) return;
    g_items[g_count++] = strdup(item);
}

int connuse_contains(const char *item) {
    if (!item) return 0;
    for (size_t i = 0; i < g_count; i++)
        if (g_items[i] && !strcmp(g_items[i], item)) return 1;
    return 0;
}

size_t connuse_count(void) { return g_count; }

const char *connuse_at(size_t i) { return i < g_count ? g_items[i] : ""; }

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

void connuse_sort(void) {
    if (g_count > 1) qsort(g_items, g_count, sizeof(g_items[0]), cmp_str);
}
