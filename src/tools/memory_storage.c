/*
 * memory_storage.c — Storage backends for Slermes memory.
 * Single-concern extraction from the monolithic memory.c:
 *   - in-memory backend (vtable_inmem_*)
 *   - file backend (vtable_file_*)
 *   - the static memory_storage_vtable_t instances wiring them in.
 *
 * The memory manager (memory_init / memory_storage_*_init) stays in memory.c.
 * Shared helpers memory_entry_expired / memory_entry_from_json are declared
 * in hermes_memory.h (non-static) so both translation units can use them.
 */

#include "hermes_memory.h"
#include "hermes_json.h"
#include "hermes_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sqlite3.h>


typedef struct {
    char    path[512];
    json_t *cache;   /* cached JSON object, NULL if not loaded */
    bool    dirty;   /* true if cache was modified since last persist */
} filedata_t;

static bool vtable_inmem_open(memory_storage_t *st, const char *uri);
static void vtable_inmem_close(memory_storage_t *st);
static bool vtable_inmem_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_inmem_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_inmem_delete(memory_storage_t *st, const char *key);
static void vtable_inmem_clear(memory_storage_t *st);
static size_t vtable_inmem_count(memory_storage_t *st);
static char **vtable_inmem_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_inmem_search(memory_storage_t *st, const char *query, int limit);
static int vtable_inmem_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_inmem_export_json(memory_storage_t *st);
static bool vtable_inmem_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_inmem_persist(memory_storage_t *st);
static bool vtable_inmem_load(memory_storage_t *st);
static int vtable_inmem_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_inmem_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

static bool vtable_file_open(memory_storage_t *st, const char *uri);
static void vtable_file_close(memory_storage_t *st);
static bool vtable_file_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_file_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_file_delete(memory_storage_t *st, const char *key);
static void vtable_file_clear(memory_storage_t *st);
static size_t vtable_file_count(memory_storage_t *st);
static char **vtable_file_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_file_search(memory_storage_t *st, const char *query, int limit);
static int vtable_file_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_file_export_json(memory_storage_t *st);
static bool vtable_file_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_file_persist(memory_storage_t *st);
static bool vtable_file_load(memory_storage_t *st);
static int vtable_file_compress_old(memory_storage_t *st, time_t before,
                                     char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_file_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

/* SQLite backend vtable functions */
static bool vtable_sqlite_open(memory_storage_t *st, const char *uri);
static void vtable_sqlite_close(memory_storage_t *st);
static bool vtable_sqlite_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_sqlite_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_sqlite_delete(memory_storage_t *st, const char *key);
static void vtable_sqlite_clear(memory_storage_t *st);
static size_t vtable_sqlite_count(memory_storage_t *st);
static char **vtable_sqlite_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_sqlite_search(memory_storage_t *st, const char *query, int limit);
static int vtable_sqlite_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_sqlite_export_json(memory_storage_t *st);
static bool vtable_sqlite_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_sqlite_persist(memory_storage_t *st);
static bool vtable_sqlite_load(memory_storage_t *st);
static int vtable_sqlite_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_sqlite_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

/* ================================================================
 *  Internal: In-memory storage data
 * ================================================================ */

#define INMEM_CAPACITY 256

typedef struct {
    memory_entry_t *entries;
    size_t          count;
    size_t          capacity;
} inmem_data_t;

memory_storage_vtable_t inmem_vtable = {
    .open              = vtable_inmem_open,
    .close             = vtable_inmem_close,
    .store             = vtable_inmem_store,
    .get               = vtable_inmem_get,
    .delete            = vtable_inmem_delete,
    .clear             = vtable_inmem_clear,
    .count             = vtable_inmem_count,
    .list_keys         = vtable_inmem_list_keys,
    .search            = vtable_inmem_search,
    .import_json       = vtable_inmem_import_json,
    .export_json       = vtable_inmem_export_json,
    .get_by_hash       = vtable_inmem_get_by_hash,
    .persist           = vtable_inmem_persist,
    .load              = vtable_inmem_load,
    .compress_old      = vtable_inmem_compress_old,
    .get_prioritized   = vtable_inmem_get_prioritized,
};

/* ================================================================
 *  In-memory backend implementation
 * ================================================================ */

bool vtable_inmem_open(memory_storage_t *st, const char *uri) {
    (void)uri;
    inmem_data_t *d = (inmem_data_t *)calloc(1, sizeof(inmem_data_t));
    if (!d) return false;
    d->capacity = INMEM_CAPACITY;
    d->entries = (memory_entry_t *)calloc(d->capacity, sizeof(memory_entry_t));
    if (!d->entries) { free(d); return false; }
    st->data = d;
    return true;
}

void vtable_inmem_close(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (d) {
        free(d->entries);
        free(d);
    }
    st->data = NULL;
}

/* Find entry by key using linear search */
static memory_entry_t *inmem_find(inmem_data_t *d, const char *key) {
    for (size_t i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].key, key) == 0)
            return &d->entries[i];
    }
    return NULL;
}

bool vtable_inmem_store(memory_storage_t *st, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !entry) return false;

    /* Auto-generate key if empty */
    if (!entry->key[0]) {
        snprintf(entry->key, sizeof(entry->key), "entry_%zu", d->count);
    }

    /* Check if key exists — update in place */
    memory_entry_t *existing = inmem_find(d, entry->key);
    if (existing) {
        memcpy(existing, entry, sizeof(memory_entry_t));
        return true;
    }

    /* Need to grow? */
    if (d->count >= d->capacity) {
        size_t newcap = d->capacity * 2;
        memory_entry_t *newents = (memory_entry_t *)realloc(d->entries, newcap * sizeof(memory_entry_t));
        if (!newents) return false;
        d->entries = newents;
        d->capacity = newcap;
    }

    memcpy(&d->entries[d->count], entry, sizeof(memory_entry_t));
    d->count++;
    return true;
}

bool vtable_inmem_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !key || !entry) return false;

    memory_entry_t *found = inmem_find(d, key);
    if (!found) return false;

    memcpy(entry, found, sizeof(memory_entry_t));
    return true;
}

bool vtable_inmem_delete(memory_storage_t *st, const char *key) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !key) return false;

    for (size_t i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].key, key) == 0) {
            /* Shift remaining left */
            if (i + 1 < d->count) {
                memmove(&d->entries[i], &d->entries[i + 1],
                        (d->count - i - 1) * sizeof(memory_entry_t));
            }
            d->count--;
            return true;
        }
    }
    return false;
}

void vtable_inmem_clear(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (d) d->count = 0;
}

size_t vtable_inmem_count(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    return d ? d->count : 0;
}

char **vtable_inmem_list_keys(memory_storage_t *st, size_t *count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !count) return NULL;

    *count = d->count;
    if (d->count == 0) return NULL;

    char **keys = (char **)calloc(d->count, sizeof(char *));
    if (!keys) { *count = 0; return NULL; }

    for (size_t i = 0; i < d->count; i++) {
        keys[i] = strdup(d->entries[i].key);
        if (!keys[i]) {
            for (size_t j = 0; j < i; j++) free(keys[j]);
            free(keys);
            *count = 0;
            return NULL;
        }
    }
    return keys;
}

memory_entry_t *vtable_inmem_search(memory_storage_t *st, const char *query, int limit) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !query) return NULL;

    if (limit <= 0) limit = 10;

    /* Simple keyword matching (case-insensitive substring) */
    memory_entry_t *results = (memory_entry_t *)calloc((size_t)limit, sizeof(memory_entry_t));
    if (!results) return NULL;

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < d->count && (int)found < limit; i++) {
        /* Skip expired */
        if (memory_entry_expired(&d->entries[i])) continue;

        /* Check content */
        bool matched = false;
        if (strstr(d->entries[i].content, query)) {
            matched = true;
        } else {
            /* Case-insensitive check */
            size_t clen = strlen(d->entries[i].content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)d->entries[i].content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }

        /* Check key */
        if (!matched && strstr(d->entries[i].key, query)) {
            matched = true;
        }

        /* Check tags */
        if (!matched) {
            for (int t = 0; t < d->entries[i].tag_count; t++) {
                if (strstr(d->entries[i].tags[t], query)) {
                    matched = true;
                    break;
                }
            }
        }

        if (matched) {
            memcpy(&results[found], &d->entries[i], sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) {
        free(results);
        return NULL;
    }

    return results; /* Caller must free — but count isn't returned here */
    /* Note: this returns up to `limit` entries, but caller doesn't know count.
       We fix this in the memory_search wrapper. */
}

/* Internal version of search that returns count correctly */
memory_entry_t *inmem_search_internal(memory_storage_t *st, const char *query, int limit, size_t *out_count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !query || !out_count) { if (out_count) *out_count = 0; return NULL; }

    if (limit <= 0) limit = 10;

    /* Allocate enough for all matches */
    memory_entry_t *results = (memory_entry_t *)calloc(d->count, sizeof(memory_entry_t));
    if (!results) { *out_count = 0; return NULL; }

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < d->count && (int)found < limit; i++) {
        if (memory_entry_expired(&d->entries[i])) continue;

        const memory_entry_t *e = &d->entries[i];
        bool matched = (strstr(e->content, query) != NULL);

        if (!matched) {
            size_t clen = strlen(e->content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)e->content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }

        if (!matched && strstr(e->key, query)) matched = true;
        if (!matched) {
            for (int t = 0; t < e->tag_count; t++) {
                if (strstr(e->tags[t], query)) { matched = true; break; }
            }
        }

        if (matched) {
            memcpy(&results[found], e, sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) {
        free(results);
        *out_count = 0;
        return NULL;
    }

    *out_count = found;
    return results;
}

int vtable_inmem_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d) return 0;

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            /* Auto-fill timestamps if missing */
            if (entry.created_at == 0) entry.created_at = time(NULL);
            if (entry.updated_at == 0) entry.updated_at = entry.created_at;

            /* Check dedup by hash — skip if duplicate */
            bool dup = false;
            for (size_t j = 0; j < d->count; j++) {
                if (d->entries[j].hash == entry.hash && entry.hash != 0) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;

            /* Auto-generate key if needed */
            if (!entry.key[0]) {
                snprintf(entry.key, sizeof(entry.key), "entry_%zu", d->count + imported);
            }

            /* Store */
            if (vtable_inmem_store(st, &entry)) {
                imported++;
            }
        }
    }
    return imported;
}

json_t *vtable_inmem_export_json(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d) return json_new_array();

    json_t *arr = json_new_array();
    if (!arr) return NULL;

    for (size_t i = 0; i < d->count; i++) {
        json_t *obj = memory_entry_to_json(&d->entries[i]);
        json_append(arr, obj);
    }
    return arr;
}

bool vtable_inmem_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !entry || hash == 0) return false;

    for (size_t i = 0; i < d->count; i++) {
        if (d->entries[i].hash == hash) {
            memcpy(entry, &d->entries[i], sizeof(memory_entry_t));
            return true;
        }
    }
    return false;
}

bool vtable_inmem_persist(memory_storage_t *st) {
    (void)st;
    return true; /* In-memory: no-op */
}

bool vtable_inmem_load(memory_storage_t *st) {
    (void)st;
    return true; /* In-memory: no-op */
}

int vtable_inmem_compress_old(memory_storage_t *st, time_t before,
                               char *(*compress_cb)(const char *content)) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !compress_cb) return 0;

    int compressed = 0;
    for (size_t i = 0; i < d->count; i++) {
        if (d->entries[i].compressed) continue;
        if (d->entries[i].created_at >= before) continue;

        char *compressed_content = compress_cb(d->entries[i].content);
        if (compressed_content) {
            snprintf(d->entries[i].content, sizeof(d->entries[i].content), "%s", compressed_content);
            d->entries[i].compressed = true;
            d->entries[i].updated_at = time(NULL);
            free(compressed_content);
            compressed++;
        }
    }
    return compressed;
}

static int entry_cmp_priority_desc(const void *a, const void *b) {
    const memory_entry_t *ea = (const memory_entry_t *)a;
    const memory_entry_t *eb = (const memory_entry_t *)b;
    if (eb->priority != ea->priority)
        return eb->priority - ea->priority; /* higher first */
    /* Secondary sort: LRU (last_accessed ascending = least recently used first, but
     * for sliding window we want most recently accessed first) */
    if (eb->last_accessed != ea->last_accessed)
        return (int)(eb->last_accessed - ea->last_accessed);
    return (int)(eb->created_at - ea->created_at);
}

memory_entry_t *vtable_inmem_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !count) { if (count) *count = 0; return NULL; }

    if (d->count == 0) { *count = 0; return NULL; }

    /* Copy entries */
    memory_entry_t *sorted = (memory_entry_t *)calloc(d->count, sizeof(memory_entry_t));
    if (!sorted) { *count = 0; return NULL; }

    size_t active = 0;
    for (size_t i = 0; i < d->count; i++) {
        if (!memory_entry_expired(&d->entries[i])) {
            memcpy(&sorted[active], &d->entries[i], sizeof(memory_entry_t));
            active++;
        }
    }

    if (active == 0) { free(sorted); *count = 0; return NULL; }

    /* Sort by priority (desc), then last_accessed (desc) */
    qsort(sorted, active, sizeof(memory_entry_t), entry_cmp_priority_desc);

    if (limit > 0 && active > limit) active = limit;

    memory_entry_t *result = (memory_entry_t *)calloc(active, sizeof(memory_entry_t));
    if (!result) { free(sorted); *count = 0; return NULL; }
    memcpy(result, sorted, active * sizeof(memory_entry_t));
    free(sorted);

    *count = active;
    return result;
}

/* ================================================================
 *  File backend: vtable implementations (P151)
 * ================================================================ */

memory_storage_vtable_t file_vtable = {
    .name              = "file",
    .open              = vtable_file_open,
    .close             = vtable_file_close,
    .store             = vtable_file_store,
    .get               = vtable_file_get,
    .delete            = vtable_file_delete,
    .clear             = vtable_file_clear,
    .count             = vtable_file_count,
    .list_keys         = vtable_file_list_keys,
    .search            = vtable_file_search,
    .import_json       = vtable_file_import_json,
    .export_json       = vtable_file_export_json,
    .get_by_hash       = vtable_file_get_by_hash,
    .persist           = vtable_file_persist,
    .load              = vtable_file_load,
    .compress_old      = vtable_file_compress_old,
    .get_prioritized   = vtable_file_get_prioritized,
};

/* ================================================================
 *  SQLite backend: vtable (F06)
 * ================================================================ */

memory_storage_vtable_t sqlite_vtable = {
    .name              = "sqlite",
    .open              = vtable_sqlite_open,
    .close             = vtable_sqlite_close,
    .store             = vtable_sqlite_store,
    .get               = vtable_sqlite_get,
    .delete            = vtable_sqlite_delete,
    .clear             = vtable_sqlite_clear,
    .count             = vtable_sqlite_count,
    .list_keys         = vtable_sqlite_list_keys,
    .search            = vtable_sqlite_search,
    .import_json       = vtable_sqlite_import_json,
    .export_json       = vtable_sqlite_export_json,
    .get_by_hash       = vtable_sqlite_get_by_hash,
    .persist           = vtable_sqlite_persist,
    .load              = vtable_sqlite_load,
    .compress_old      = vtable_sqlite_compress_old,
    .get_prioritized   = vtable_sqlite_get_prioritized,
};

/* ================================================================
 *  SQLite backend: implementations
 * ================================================================ */

/* Tags helpers: serialize to/from comma-separated string */
static char *tags_to_string(const memory_entry_t *entry) {
    static char buf[1024];
    buf[0] = '\0';
    for (int i = 0; i < entry->tag_count && i < MEMORY_TAGS_MAX; i++) {
        if (i > 0) strcat(buf, ",");
        strcat(buf, entry->tags[i]);
    }
    return buf;
}

static void string_to_tags(const char *s, memory_entry_t *entry) {
    entry->tag_count = 0;
    if (!s || !*s) return;
    char buf[1024];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, ",");
    while (tok && entry->tag_count < MEMORY_TAGS_MAX) {
        strncpy(entry->tags[entry->tag_count], tok, MEMORY_TAG_MAX - 1);
        entry->tags[entry->tag_count][MEMORY_TAG_MAX - 1] = '\0';
        entry->tag_count++;
        tok = strtok(NULL, ",");
    }
}

static bool vtable_sqlite_open(memory_storage_t *st, const char *uri) {
    if (!st || !uri) return false;
    sqlite3 *db = NULL;
    if (sqlite3_open(uri, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS memory_entries ("
        "key TEXT PRIMARY KEY,"
        "content TEXT NOT NULL,"
        "hash INTEGER NOT NULL DEFAULT 0,"
        "priority INTEGER NOT NULL DEFAULT 0,"
        "created_at INTEGER NOT NULL DEFAULT 0,"
        "updated_at INTEGER NOT NULL DEFAULT 0,"
        "expires_at INTEGER NOT NULL DEFAULT 0,"
        "tags TEXT NOT NULL DEFAULT '',"
        "compressed INTEGER NOT NULL DEFAULT 0,"
        "access_count INTEGER NOT NULL DEFAULT 0,"
        "last_accessed INTEGER NOT NULL DEFAULT 0"
    ")";
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }
    st->data = db;
    return true;
}

static void vtable_sqlite_close(memory_storage_t *st) {
    if (st && st->data) {
        sqlite3_close((sqlite3 *)st->data);
        st->data = NULL;
    }
}

/* Bind entry fields to a prepared INSERT OR REPLACE statement */
static void bind_entry(sqlite3_stmt *stmt, const memory_entry_t *entry) {
    sqlite3_bind_text(stmt, 1, entry->key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry->content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)entry->hash);
    sqlite3_bind_int(stmt, 4, entry->priority);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)entry->created_at);
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)entry->updated_at);
    sqlite3_bind_int64(stmt, 7, (sqlite3_int64)entry->expires_at);
    char *tags = tags_to_string(entry);
    sqlite3_bind_text(stmt, 8, tags, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, entry->compressed ? 1 : 0);
    sqlite3_bind_int(stmt, 10, entry->access_count);
    sqlite3_bind_int64(stmt, 11, (sqlite3_int64)entry->last_accessed);
}

/* Read a row into an entry. Returns true if row had data. */
static bool read_row(sqlite3_stmt *stmt, memory_entry_t *entry) {
    if (sqlite3_step(stmt) != SQLITE_ROW) return false;
    memset(entry, 0, sizeof(*entry));
    const char *k = (const char *)sqlite3_column_text(stmt, 0);
    if (k) strncpy(entry->key, k, sizeof(entry->key) - 1);
    const char *c = (const char *)sqlite3_column_text(stmt, 1);
    if (c) strncpy(entry->content, c, sizeof(entry->content) - 1);
    entry->hash = (uint64_t)sqlite3_column_int64(stmt, 2);
    entry->priority = sqlite3_column_int(stmt, 3);
    entry->created_at = (time_t)sqlite3_column_int64(stmt, 4);
    entry->updated_at = (time_t)sqlite3_column_int64(stmt, 5);
    entry->expires_at = (time_t)sqlite3_column_int64(stmt, 6);
    const char *t = (const char *)sqlite3_column_text(stmt, 7);
    if (t) string_to_tags(t, entry);
    entry->compressed = sqlite3_column_int(stmt, 8) != 0;
    entry->access_count = sqlite3_column_int(stmt, 9);
    entry->last_accessed = (time_t)sqlite3_column_int64(stmt, 10);
    return true;
}

static bool vtable_sqlite_store(memory_storage_t *st, memory_entry_t *entry) {
    if (!st || !st->data || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    const char *sql = "INSERT OR REPLACE INTO memory_entries "
        "(key,content,hash,priority,created_at,updated_at,expires_at,"
        "tags,compressed,access_count,last_accessed) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    bind_entry(stmt, entry);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

static bool vtable_sqlite_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    if (!st || !st->data || !key || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bool found = read_row(stmt, entry);
    sqlite3_finalize(stmt);
    return found;
}

static bool vtable_sqlite_delete(memory_storage_t *st, const char *key) {
    if (!st || !st->data || !key) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM memory_entries WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

static void vtable_sqlite_clear(memory_storage_t *st) {
    if (!st || !st->data) return;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_exec(db, "DELETE FROM memory_entries", NULL, NULL, NULL);
}

static size_t vtable_sqlite_count(memory_storage_t *st) {
    if (!st || !st->data) return 0;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM memory_entries", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    size_t n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) n = (size_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

static char **vtable_sqlite_list_keys(memory_storage_t *st, size_t *count) {
    if (!st || !st->data) { if (count) *count = 0; return NULL; }
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT key FROM memory_entries ORDER BY key", -1, &stmt, NULL) != SQLITE_OK) {
        if (count) *count = 0;
        return NULL;
    }
    size_t cap = 64, n = 0;
    char **keys = (char **)calloc(cap, sizeof(char *));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n >= cap) { cap *= 2; keys = (char **)realloc(keys, cap * sizeof(char *)); }
        const char *k = (const char *)sqlite3_column_text(stmt, 0);
        keys[n] = k ? strdup(k) : NULL;
        n++;
    }
    sqlite3_finalize(stmt);
    if (count) *count = n;
    return keys;
}

static memory_entry_t *vtable_sqlite_search(memory_storage_t *st, const char *query, int limit) {
    if (!st || !st->data || !query) return NULL;
    sqlite3 *db = (sqlite3 *)st->data;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM memory_entries WHERE content LIKE '%%%s%%' OR key LIKE '%%%s%%' ORDER BY priority DESC",
        query, query);
    if (limit > 0) { char *p = sql + strlen(sql); snprintf(p, sizeof(sql) - (size_t)(p - sql), " LIMIT %d", limit); }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    size_t cap = 16, n = 0;
    memory_entry_t *results = (memory_entry_t *)calloc(cap, sizeof(memory_entry_t));
    while (read_row(stmt, &results[n])) {
        n++;
        if (n >= cap) { cap *= 2; results = (memory_entry_t *)realloc(results, cap * sizeof(memory_entry_t)); memset(&results[n], 0, (cap - n) * sizeof(memory_entry_t)); }
    }
    sqlite3_finalize(stmt);
    return results;
}

static int vtable_sqlite_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !st->data || !entries) return 0;
    size_t n = json_len(entries);
    int imported = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(entries, i);
        memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        if (memory_entry_from_json(&entry, obj) && vtable_sqlite_store(st, &entry))
            imported++;
    }
    return imported;
}

static json_t *vtable_sqlite_export_json(memory_storage_t *st) {
    if (!st || !st->data) return NULL;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries ORDER BY key", -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    json_t *arr = json_new_array();
    memory_entry_t entry;
    while (read_row(stmt, &entry)) {
        json_t *obj = memory_entry_to_json(&entry);
        if (obj) json_append(arr, obj);
    }
    sqlite3_finalize(stmt);
    return arr;
}

static bool vtable_sqlite_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    if (!st || !st->data || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE hash=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)hash);
    bool found = read_row(stmt, entry);
    sqlite3_finalize(stmt);
    return found;
}

static bool vtable_sqlite_persist(memory_storage_t *st) { (void)st; return true; } /* SQLite auto-persists */
static bool vtable_sqlite_load(memory_storage_t *st) { (void)st; return true; }    /* SQLite auto-loads */

static int vtable_sqlite_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content)) {
    if (!st || !st->data || !compress_cb) return 0;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE updated_at<?1 AND compressed=0",
                           -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)before);
    int count = 0;
    memory_entry_t entry;
    while (read_row(stmt, &entry)) {
        char *compressed = compress_cb(entry.content);
        if (compressed) {
            strncpy(entry.content, compressed, sizeof(entry.content) - 1);
            entry.compressed = true;
            entry.updated_at = time(NULL);
            vtable_sqlite_store(st, &entry);
            free(compressed);
            count++;
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

static memory_entry_t *vtable_sqlite_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    if (!st || !st->data) { if (count) *count = 0; return NULL; }
    sqlite3 *db = (sqlite3 *)st->data;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM memory_entries ORDER BY priority DESC%s",
             limit > 0 ? " LIMIT ?1" : "");
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { if (count) *count = 0; return NULL; }
    if (limit > 0) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)limit);
    size_t cap = 16, n = 0;
    memory_entry_t *results = (memory_entry_t *)calloc(cap, sizeof(memory_entry_t));
    while (read_row(stmt, &results[n])) {
        n++;
        if (n >= cap) { cap *= 2; results = (memory_entry_t *)realloc(results, cap * sizeof(memory_entry_t)); memset(&results[n], 0, (cap - n) * sizeof(memory_entry_t)); }
    }
    sqlite3_finalize(stmt);
    if (count) *count = n;
    return results;
}

/* Convert JSON entry object (stored as {key: {content, ...}}) to memory_entry_t */
/* (file_json_to_entry replaced by memory_entry_from_json) */

bool vtable_file_open(memory_storage_t *st, const char *uri) {
    filedata_t *fd = (filedata_t *)calloc(1, sizeof(filedata_t));
    if (!fd) return false;

    if (uri) snprintf(fd->path, sizeof(fd->path), "%s", uri);

    /* Ensure directory exists */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", fd->path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }

    st->data = fd;

    /* Try to load existing file */
    vtable_file_load(st);
    return true;
}

void vtable_file_close(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (fd) {
        if (fd->dirty) vtable_file_persist(st);
        if (fd->cache) json_free(fd->cache);
        free(fd);
    }
    st->data = NULL;
}

/* Store entry as JSON object under its key. To maintain backward compatibility,
 * we store entries as {key: {content, metadata...}} inside the root object. */
bool vtable_file_store(memory_storage_t *st, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !entry) return false;

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return false;
    }

    /* Auto-generate key if empty */
    if (!entry->key[0]) {
        size_t n = json_len(fd->cache);
        snprintf(entry->key, sizeof(entry->key), "entry_%zu", n);
    }

    /* Set timestamps */
    if (entry->created_at == 0) entry->created_at = time(NULL);
    entry->updated_at = time(NULL);

    /* Compute hash if empty */
    if (entry->hash == 0) {
        entry->hash = memory_hash_content(entry->content);
    }

    json_t *obj = memory_entry_to_json(entry);
    if (!obj) return false;

    json_set(fd->cache, entry->key, obj);
    fd->dirty = true;
    return true;
}

bool vtable_file_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !key || !entry) return false;

    if (!fd->cache) return false;

    json_t *obj = json_obj_get(fd->cache, key);
    if (!obj) return false;

    return memory_entry_from_json(entry, obj);
}

bool vtable_file_delete(memory_storage_t *st, const char *key) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !key || !fd->cache) return false;

    json_t *existing = json_obj_get(fd->cache, key);
    if (!existing) return false;

    /* Remove by deleting and recreating the object */
    json_t *old = fd->cache;
    fd->cache = json_new_object();
    if (!fd->cache) { fd->cache = old; return false; }

    /* Copy all keys except the one to delete */
    size_t n = json_len(old);
    for (size_t i = 0; i < n; i++) {
        const char *k = old->c.keys[i];
        if (strcmp(k, key) != 0) {
            json_set(fd->cache, k, json_copy(json_get(old, i)));
        }
    }
    json_free(old);
    fd->dirty = true;
    return true;
}

void vtable_file_clear(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return;
    if (fd->cache) json_free(fd->cache);
    fd->cache = json_new_object();
    fd->dirty = true;
}

size_t vtable_file_count(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache) return 0;
    return json_len(fd->cache);
}

char **vtable_file_list_keys(memory_storage_t *st, size_t *count) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !count) {
        if (count) *count = 0;
        return NULL;
    }

    size_t n = json_len(fd->cache);
    if (n == 0) { *count = 0; return NULL; }

    char **keys = (char **)calloc(n, sizeof(char *));
    if (!keys) { *count = 0; return NULL; }

    size_t idx = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = fd->cache->c.keys[i];
        if (k) {
            keys[idx] = strdup(k);
            if (!keys[idx]) {
                for (size_t j = 0; j < idx; j++) free(keys[j]);
                free(keys);
                *count = 0;
                return NULL;
            }
            idx++;
        }
    }

    *count = idx;
    return keys;
}

memory_entry_t *vtable_file_search(memory_storage_t *st, const char *query, int limit) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !query) return NULL;

    if (limit <= 0) limit = 10;
    size_t n = json_len(fd->cache);

    memory_entry_t *results = (memory_entry_t *)calloc((size_t)limit, sizeof(memory_entry_t));
    if (!results) return NULL;

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < n && (int)found < limit; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (!memory_entry_from_json(&entry, obj)) continue;
        if (memory_entry_expired(&entry)) continue;

        bool matched = (strstr(entry.content, query) != NULL);
        if (!matched) {
            size_t clen = strlen(entry.content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)entry.content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }
        if (!matched && strstr(entry.key, query)) matched = true;
        if (!matched) {
            for (int t = 0; t < entry.tag_count; t++) {
                if (strstr(entry.tags[t], query)) { matched = true; break; }
            }
        }

        if (matched) {
            memcpy(&results[found], &entry, sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) { free(results); return NULL; }

    return results;
}

int vtable_file_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return 0;

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return 0;
    }

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            if (!entry.key[0]) continue; /* key required for file backend */
            if (entry.hash == 0) entry.hash = memory_hash_content(entry.content);
            if (entry.created_at == 0) entry.created_at = time(NULL);

            /* Check dedup */
            if (entry.hash != 0) {
                json_t *existing = json_obj_get(fd->cache, entry.key);
                if (existing) {
                    uint64_t ehash = (uint64_t)json_get_num(existing, "hash", 0);
                    if (ehash == entry.hash) continue; /* skip duplicate */
                }
            }

            json_t *obj = memory_entry_to_json(&entry);
            if (obj) {
                json_set(fd->cache, entry.key, obj);
                imported++;
                fd->dirty = true;
            }
        }
    }
    return imported;
}

json_t *vtable_file_export_json(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache) return json_new_array();

    json_t *arr = json_new_array();
    if (!arr) return NULL;

    size_t n = json_len(fd->cache);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (obj) {
            json_append(arr, json_copy(obj));
        }
    }
    return arr;
}

bool vtable_file_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !entry || hash == 0) return false;

    size_t n = json_len(fd->cache);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        uint64_t ehash = (uint64_t)json_get_num(obj, "hash", 0);
        if (ehash == hash) {
            return memory_entry_from_json(entry, obj);
        }
    }
    return false;
}

bool vtable_file_persist(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !fd->dirty) return true;

    char *json_str = json_serialize(fd->cache);
    if (!json_str) return false;

    FILE *f = fopen(fd->path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);

    fd->dirty = false;
    return true;
}

bool vtable_file_load(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return false;

    if (fd->cache) json_free(fd->cache);

    char *err = NULL;
    fd->cache = json_parse_file(fd->path, &err);
    if (err) free(err);

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return false;
    }

    fd->dirty = false;
    return true;
}

int vtable_file_compress_old(memory_storage_t *st, time_t before,
                              char *(*compress_cb)(const char *content)) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !compress_cb) return 0;

    size_t n = json_len(fd->cache);
    int compressed = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;

        time_t created = (time_t)json_get_num(obj, "created_at", 0);
        bool already_compressed = json_get_bool(obj, "compressed", false);
        if (already_compressed) continue;
        if (created >= before) continue;

        const char *content = json_get_str(obj, "content", "");
        if (!content || !*content) continue;

        char *compressed_content = compress_cb(content);
        if (compressed_content) {
            const char *key = fd->cache->c.keys[i];
            json_set(obj, "content", json_string(compressed_content));
            json_set(obj, "compressed", json_bool(true));
            json_set(obj, "updated_at", json_number((double)time(NULL)));
            free(compressed_content);

            /* Re-set in cache so it writes back */
            if (key) json_set(fd->cache, key, obj);
            fd->dirty = true;
            compressed++;
        }
    }
    return compressed;
}

memory_entry_t *vtable_file_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !count) { if (count) *count = 0; return NULL; }

    size_t n = json_len(fd->cache);
    if (n == 0) { *count = 0; return NULL; }

    /* Collect all entries */
    memory_entry_t *all = (memory_entry_t *)calloc(n, sizeof(memory_entry_t));
    if (!all) { *count = 0; return NULL; }

    size_t active = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        memory_entry_t entry;
        if (memory_entry_from_json(&entry, obj)) {
            if (!memory_entry_expired(&entry)) {
                memcpy(&all[active], &entry, sizeof(memory_entry_t));
                active++;
            }
        }
    }

    if (active == 0) { free(all); *count = 0; return NULL; }

    qsort(all, active, sizeof(memory_entry_t), entry_cmp_priority_desc);

    if (limit > 0 && active > limit) active = limit;

    memory_entry_t *result = (memory_entry_t *)calloc(active, sizeof(memory_entry_t));
    if (!result) { free(all); *count = 0; return NULL; }
    memcpy(result, all, active * sizeof(memory_entry_t));
    free(all);

    *count = active;
    return result;
}

/* ================================================================
 *  Storage backend constructors
 * ================================================================ */
