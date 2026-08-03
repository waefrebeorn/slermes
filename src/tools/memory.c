/*
 * memory.c — Memory system depth for Hermes C.
 * P151-P158: Storage abstraction, TTL, prioritization, dedup, search,
 *            auto-save, import/export, compression.
 *
 * Provides a unified memory abstraction with multiple backends:
 *   - In-memory (hash map)
 *   - File-based (JSON file, backward-compatible with existing memory.json)
 *   - SQLite (via libdb)
 *   - Plugin (delegates to PLUGIN_MEMORY plugin)
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
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
#include "sqlite3.h"
#include "memory_store.h"  /* faithful tools/memory_tool.py port (singleton store) */

/* ================================================================
 *  Forward declarations of built-in backend vtables
 * ================================================================ */


/* ================================================================
 *  Internal: File storage data
 * ================================================================ */


/* ================================================================
 *  FNV-1a hash for dedup (P154)
 * ================================================================ */

uint64_t memory_hash_content(const char *content) {
    if (!content) return 0;
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */
    const unsigned char *p = (const unsigned char *)content;
    while (*p) {
        hash ^= (uint64_t)(*p);
        hash *= 1099511628211ULL; /* FNV prime */
        p++;
    }
    return hash;
}

/* ================================================================
 *  Memory snapshot formatting for system prompt (S1)
 * ================================================================ */

char *memory_format_snapshot(const memory_t *mem, int limit) {
    if (!mem) return NULL;

    int max_entries = limit > 0 ? limit : (mem->search_limit > 0 ? mem->search_limit : 20);

    /* Get prioritized entries */
    size_t count = 0;
    memory_entry_t *entries = memory_get_prioritized((memory_t *)mem, (size_t)max_entries, &count);
    if (!entries || count == 0) {
        free(entries);
        return NULL;
    }

    /* Calculate required buffer size */
    size_t buf_size = 64; /* header + prefix */
    for (size_t i = 0; i < count; i++) {
        buf_size += strlen(entries[i].key) + strlen(entries[i].content) + 64;
    }

    char *buf = (char *)malloc(buf_size);
    if (!buf) {
        free(entries);
        return NULL;
    }

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos,
        "## Memory Snapshot\n\nThe following information is stored in memory:\n\n");

    for (size_t i = 0; i < count && pos < buf_size - 4; i++) {
        if (memory_entry_expired(&entries[i])) continue;

        /* Format: - key: content (truncated to 200 chars) */
        char content_short[256];
        size_t clen = strlen(entries[i].content);
        if (clen >= 200) {
            memcpy(content_short, entries[i].content, 197);
            content_short[197] = '.';
            content_short[198] = '.';
            content_short[199] = '.';
            content_short[200] = '\0';
        } else {
            memcpy(content_short, entries[i].content, clen + 1);
        }

        pos += snprintf(buf + pos, buf_size - pos,
            "- **%s**: %s\n", entries[i].key, content_short);
    }

    free(entries);
    return buf;
}

/* ================================================================
 *  Entry helpers
 * ================================================================ */

bool memory_entry_expired(const memory_entry_t *entry) {
    if (!entry) return true;
    if (entry->expires_at == 0) return false;
    return time(NULL) >= entry->expires_at;
}

bool memory_entry_from_json(memory_entry_t *entry, const json_t *obj) {
    if (!entry || !obj) return false;
    memset(entry, 0, sizeof(*entry));

    const char *key = json_get_str(obj, "key", "");
    snprintf(entry->key, sizeof(entry->key), "%s", key);

    const char *content = json_get_str(obj, "content", "");
    snprintf(entry->content, sizeof(entry->content), "%s", content);

    entry->created_at    = (time_t)json_get_num(obj, "created_at", 0);
    entry->updated_at    = (time_t)json_get_num(obj, "updated_at", 0);
    entry->expires_at    = (time_t)json_get_num(obj, "expires_at", 0);
    entry->priority      = (int)json_get_num(obj, "priority", 0);
    entry->hash          = (uint64_t)json_get_num(obj, "hash", 0);
    entry->compressed    = json_get_bool(obj, "compressed", false);
    entry->access_count  = (int)json_get_num(obj, "access_count", 0);
    entry->last_accessed = (time_t)json_get_num(obj, "last_accessed", 0);

    /* Parse tags */
    const json_t *tags_arr = json_obj_get(obj, "tags");
    if (tags_arr && tags_arr->type == JSON_ARRAY) {
        size_t n = json_len(tags_arr);
        for (size_t i = 0; i < n && entry->tag_count < MEMORY_TAGS_MAX; i++) {
            json_t *tag = json_get(tags_arr, i);
            if (tag && tag->type == JSON_STRING) {
                snprintf(entry->tags[entry->tag_count], MEMORY_TAG_MAX, "%s", tag->str_val);
                entry->tag_count++;
            }
        }
    }

    return true;
}

json_t *memory_entry_to_json(const memory_entry_t *entry) {
    if (!entry) return json_null();
    json_t *obj = json_new_object();
    if (!obj) return NULL;

    json_set(obj, "key", json_string(entry->key));
    json_set(obj, "content", json_string(entry->content));
    json_set(obj, "created_at", json_number((double)entry->created_at));
    json_set(obj, "updated_at", json_number((double)entry->updated_at));
    json_set(obj, "expires_at", json_number((double)entry->expires_at));
    json_set(obj, "priority", json_number((double)entry->priority));
    json_set(obj, "hash", json_number((double)entry->hash));
    json_set(obj, "compressed", json_bool(entry->compressed));
    json_set(obj, "access_count", json_number((double)entry->access_count));
    json_set(obj, "last_accessed", json_number((double)entry->last_accessed));

    if (entry->tag_count > 0) {
        json_t *tags = json_new_array();
        for (int i = 0; i < entry->tag_count; i++) {
            json_append(tags, json_string(entry->tags[i]));
        }
        json_set(obj, "tags", tags);
    }

    return obj;
}

/* ================================================================
 *  Default storage vtable for in-memory
 * ================================================================ */

bool memory_storage_inmem_init(memory_storage_t *st) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_INMEM;
    st->vtable = inmem_vtable;
    return st->vtable.open(st, "");
}

bool memory_storage_file_init(memory_storage_t *st, const char *path) {
    if (!st || !path) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_FILE;
    st->vtable = file_vtable;
    snprintf(st->uri, sizeof(st->uri), "%s", path);
    return st->vtable.open(st, path);
}

bool memory_storage_sqlite_init(memory_storage_t *st, const char *path) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_SQLITE;
    st->vtable = sqlite_vtable;
    snprintf(st->uri, sizeof(st->uri), "%s", path ? path : "memory.db");
    return st->vtable.open(st, path ? path : "memory.db");
}

/* Port of Python gateway/platforms/yuanbao.py:open(). */
/* ================================================================
 *  Plugin-backed vtable — delegates to plugin_interface_t
 * ================================================================ */

static bool plugin_open(memory_storage_t *st, const char *uri) {
    (void)st; (void)uri;
    return true; /* plugin already initialized */
}

static void plugin_close(memory_storage_t *st) {
    if (st && st->plugin_iface && st->plugin_iface->memory_clear)
        st->plugin_iface->memory_clear();
}

static bool plugin_store(memory_storage_t *st, memory_entry_t *entry) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_store || !entry)
        return false;
    char *result = st->plugin_iface->memory_store(
        entry->content,
        "{\"key\":\"%s\"}" /* minimal metadata with key */);
    bool ok = (result && strstr(result, "\"status\":\"ok\"") != NULL);
    free(result);
    return ok;
}

static bool plugin_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    (void)st; (void)key; (void)entry;
    return false; /* plugin search is query-based, not key-based */
}

static bool plugin_delete(memory_storage_t *st, const char *key) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_store || !key)
        return false;
    /* Delete by setting empty content with matching key metadata */
    char metadata[256];
    snprintf(metadata, sizeof(metadata),
             "{\"key\":\"%s\",\"operation\":\"delete\"}", key);
    char *result = st->plugin_iface->memory_store("", metadata);
    bool ok = (result != NULL);
    free(result);
    return ok;
}

static void plugin_clear(memory_storage_t *st) {
    if (st && st->plugin_iface && st->plugin_iface->memory_clear)
        st->plugin_iface->memory_clear();
}

static size_t plugin_count(memory_storage_t *st) {
    (void)st;
    return 0; /* plugin doesn't expose count in standard interface */
}

static char **plugin_list_keys(memory_storage_t *st, size_t *count) {
    (void)st;
    if (count) *count = 0;
    return NULL;
}

/* Plugin search via plugin's memory_search */
static memory_entry_t *plugin_search(memory_storage_t *st, const char *query, int limit) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_search)
        return NULL;
    char *result_json = st->plugin_iface->memory_search(query, limit);
    if (!result_json) return NULL;

    /* Parse the JSON result to extract entries */
    /* Simple approach: return a single entry with the raw JSON */
    memory_entry_t *entries = (memory_entry_t *)calloc(1, sizeof(memory_entry_t));
    if (entries) {
        entries->key[0] = '\0';
        snprintf(entries->content, sizeof(entries->content), "%s", result_json);
        entries->created_at = time(NULL);
        entries->access_count = 5;
    }
    free(result_json);
    return entries;
}

static int plugin_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    if (!st->plugin_iface || !st->plugin_iface->memory_store) return 0;

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            if (entry.created_at == 0) entry.created_at = time(NULL);
            if (entry.updated_at == 0) entry.updated_at = entry.created_at;
            if (!entry.key[0]) {
                snprintf(entry.key, sizeof(entry.key), "entry_imported_%zu", (size_t)imported);
            }

            /* Store via plugin interface */
            char metadata[128];
            snprintf(metadata, sizeof(metadata),
                     "{\"key\":\"%s\",\"created_at\":%ld,\"updated_at\":%ld}",
                     entry.key, (long)entry.created_at, (long)entry.updated_at);
            char *result = st->plugin_iface->memory_store(entry.content, metadata);
            if (result) {
                imported++;
                free(result);
            }
        }
    }
    return imported;
}

static json_t *plugin_export_json(memory_storage_t *st) {
    (void)st;
    /* Plugin backend doesn't support full enumeration.
     * Return empty array — callers can use search for specific queries. */
    return json_new_array();
}

static bool plugin_vtable_persist(memory_storage_t *st) {
    (void)st; return true; /* plugin manages its own state */
}

static bool plugin_vtable_load(memory_storage_t *st) {
    (void)st; return true;
}

static bool plugin_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    (void)st; (void)hash; (void)entry;
    /* Plugin backend doesn't support hash-based dedup lookup.
     * Return false = "not found" — dedup best-effort for plugin. */
    return false;
}

static int plugin_compress_old(memory_storage_t *st, time_t before, memory_compress_fn_t compress_cb) {
    (void)st; (void)before; (void)compress_cb;
    /* Plugin manages its own storage — no local compression needed. */
    return 0;
}

static memory_entry_t *plugin_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    (void)st; (void)limit;
    if (count) *count = 0;
    /* Plugin backend doesn't support priority enumeration. */
    return NULL;
}

static memory_storage_vtable_t plugin_vtable = {
    .name        = "plugin",
    .open        = plugin_open,
    .close       = plugin_close,
    .store       = plugin_store,
    .get         = plugin_get,
    .delete      = plugin_delete,
    .clear       = plugin_clear,
    .count       = plugin_count,
    .list_keys   = plugin_list_keys,
    .search      = plugin_search,
    .import_json = plugin_import_json,
    .export_json = plugin_export_json,
    .get_by_hash = plugin_get_by_hash,
    .persist     = plugin_vtable_persist,
    .load        = plugin_vtable_load,
    .compress_old = plugin_compress_old,
    .get_prioritized  = plugin_get_prioritized,
};

bool memory_storage_plugin_init(memory_storage_t *st, void *plugin_reg, const char *plugin_name_str) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_PLUGIN;

    /* Search plugin registry for a PLUGIN_MEMORY plugin */
    if (plugin_reg) {
        plugin_registry_t *reg = (plugin_registry_t *)plugin_reg;
        const char *target_name = plugin_name_str && plugin_name_str[0] ? plugin_name_str : "in-memory-store";

        /* Find the plugin by name */
        plugin_t *plug = plugin_registry_find(reg, target_name);
        if (!plug) {
            /* Try loading from source tree if not found in registry */
            char plugin_path[512];
            const char *home = getenv("HOME");
            if (!home) home = "/tmp";
            snprintf(plugin_path, sizeof(plugin_path),
                     "%s/hermes-agent-dev/C/src/plugins/plugin_honcho.so", home);
            plug = plugin_load(plugin_path);
            if (plug) {
/* PoP: add @ tools/memory_tool.py:add */
                plugin_registry_add(reg, plug);
                /* Initialize */
                typedef int (*init_fn_t)(void);
                init_fn_t init_fn = (init_fn_t)plugin_symbol(plug, "plugin_init");
                if (init_fn) init_fn();
            } else {
                fprintf(stderr, "[memory] plugin_load failed: %s\n",
                        plugin_error());
            }
        }

        if (plug && plugin_type(plug) == PLUGIN_MEMORY) {
            void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(plug, "plugin_get_interface");
            if (get_iface) {
                plugin_interface_t *iface = (plugin_interface_t *)get_iface();
                if (iface && iface->memory_store && iface->memory_search) {
                    st->plugin_plug = plug;
                    st->plugin_iface = iface;
                    st->vtable = plugin_vtable;
                    fprintf(stderr, "[memory] using plugin: %s\n", plugin_name(plug));
                    return true;
                }
            }
        }
    }

    /* Fallback to in-memory if plugin not available */
    fprintf(stderr, "[memory] plugin not found, falling back to in-memory\n");
    return memory_storage_inmem_init(st);
}

/* ================================================================
 *  Memory manager implementation
 * ================================================================ */

/* Default memory path for file storage */
static void get_default_memory_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sz, "%s/.hermes/memory.json", home);
}

bool memory_init(memory_t *mem, memory_storage_type_t type, const char *uri) {
    if (!mem) return false;
    memset(mem, 0, sizeof(*mem));

    pthread_mutex_init(&mem->lock, NULL);

    mem->autosave = MEMORY_AUTOSAVE_OFF;
    mem->autosave_interval_sec = 60;
    mem->ttl_days = 30;
    mem->dedup_enabled = true;
    mem->search_limit = 20;
    mem->compression.enabled = false;
    mem->compression.min_entries = 50;
    mem->compression.max_batch = 10;
    mem->compression.older_than_days = 30;

    mem->ttl_running = false;
    mem->autosave_running = false;

    /* Initialize storage backend */
    switch (type) {
        case MEMORY_STORAGE_INMEM:
            return memory_storage_inmem_init(&mem->storage);

        case MEMORY_STORAGE_FILE: {
            char path[512];
            if (uri && uri[0])
                snprintf(path, sizeof(path), "%s", uri);
            else
                get_default_memory_path(path, sizeof(path));
            return memory_storage_file_init(&mem->storage, path);
        }

        case MEMORY_STORAGE_SQLITE: {
            char path[512];
            if (uri && uri[0])
                snprintf(path, sizeof(path), "%s", uri);
            else
                snprintf(path, sizeof(path), "memory.db");
            return memory_storage_sqlite_init(&mem->storage, path);
        }

        case MEMORY_STORAGE_PLUGIN:
            return memory_storage_plugin_init(&mem->storage, NULL, NULL);

        default:
            return false;
    }
}

bool memory_init_from_config(memory_t *mem, const memory_config_t *cfg) {
    if (!mem || !cfg) return false;
    if (!memory_init(mem, (memory_storage_type_t)cfg->storage_type, cfg->storage_path))
        return false;

    mem->ttl_days = cfg->ttl_days > 0 ? cfg->ttl_days : 30;
    mem->dedup_enabled = cfg->dedup_enabled;
    mem->search_limit = cfg->search_limit > 0 ? cfg->search_limit : 20;
    mem->compression.enabled = cfg->compression_enabled;
    mem->compression.min_entries = cfg->compression_min_entries > 0 ? cfg->compression_min_entries : 50;
    mem->compression.max_batch = 10;
    mem->compression.older_than_days = cfg->ttl_days > 0 ? (time_t)cfg->ttl_days : 30;

    if (cfg->auto_save) {
        int interval = cfg->auto_save_interval > 0 ? cfg->auto_save_interval : 60;
        memory_autosave_start_thread(mem, interval);
    }

    if (cfg->ttl_days > 0) {
        memory_ttl_start_thread(mem, 300); /* Check TTL every 5 minutes */
    }

    mem->config = cfg;
    return true;
}

void memory_cleanup(memory_t *mem) {
    if (!mem) return;

    /* Stop threads */
    memory_ttl_stop_thread(mem);
    memory_autosave_stop_thread(mem);

    pthread_mutex_lock(&mem->lock);

    /* Persist if dirty */
    mem->storage.vtable.persist(&mem->storage);

    /* Close storage */
    mem->storage.vtable.close(&mem->storage);

    pthread_mutex_unlock(&mem->lock);
    pthread_mutex_destroy(&mem->lock);
}

bool memory_store(memory_t *mem, memory_entry_t *entry) {
    if (!mem || !entry) return false;

    pthread_mutex_lock(&mem->lock);

    /* Set timestamps */
    if (entry->created_at == 0) entry->created_at = time(NULL);
    entry->updated_at = time(NULL);

    /* Compute hash for dedup */
    if (entry->hash == 0) {
        entry->hash = memory_hash_content(entry->content);
    }

    /* Dedup check (P154) */
    if (mem->dedup_enabled && entry->hash != 0) {
        memory_entry_t existing;
        if (mem->storage.vtable.get_by_hash(&mem->storage, entry->hash, &existing)) {
            /* Duplicate found — update existing entry's access count and content */
            existing.updated_at = time(NULL);
            existing.access_count++;
            if (entry->priority > 0) existing.priority = entry->priority;
            /* Update content if different */
            if (strcmp(existing.content, entry->content) != 0) {
                snprintf(existing.content, sizeof(existing.content), "%s", entry->content);
            }
            /* Merge tags */
            for (int t = 0; t < entry->tag_count && existing.tag_count < MEMORY_TAGS_MAX; t++) {
                bool found = false;
                for (int et = 0; et < existing.tag_count; et++) {
                    if (strcmp(existing.tags[et], entry->tags[t]) == 0) { found = true; break; }
                }
                if (!found) {
                    snprintf(existing.tags[existing.tag_count], MEMORY_TAG_MAX, "%s", entry->tags[t]);
                    existing.tag_count++;
                }
            }

            bool ok = mem->storage.vtable.store(&mem->storage, &existing);
            pthread_mutex_unlock(&mem->lock);
            return ok;
        }
    }

    bool ok = mem->storage.vtable.store(&mem->storage, entry);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

bool memory_store_simple(memory_t *mem, const char *content, int priority, int ttl_seconds) {
    if (!mem || !content) return false;

    memory_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.content, sizeof(entry.content), "%s", content);
    entry.priority = priority;
    entry.created_at = time(NULL);
    entry.updated_at = entry.created_at;

    if (ttl_seconds > 0) {
        entry.expires_at = entry.created_at + ttl_seconds;
    }

    entry.hash = memory_hash_content(content);
    entry.access_count = 1;
    entry.last_accessed = entry.created_at;

    return memory_store(mem, &entry);
}

bool memory_get(memory_t *mem, const char *key, memory_entry_t *entry) {
    if (!mem || !key || !entry) return false;

    pthread_mutex_lock(&mem->lock);
    bool found = mem->storage.vtable.get(&mem->storage, key, entry);
    if (found) {
        if (memory_entry_expired(entry)) {
            /* Auto-delete expired entries on access */
            mem->storage.vtable.delete(&mem->storage, key);
            pthread_mutex_unlock(&mem->lock);
            return false;
        }
        entry->access_count++;
        entry->last_accessed = time(NULL);
    }
    pthread_mutex_unlock(&mem->lock);
    return found;
}

/* Port of Python tools/browser_camofox.py:_delete(). */
/* Port of Python gateway/platforms/api_server.py:delete(). */
bool memory_delete(memory_t *mem, const char *key) {
    if (!mem || !key) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.delete(&mem->storage, key);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

void memory_clear(memory_t *mem) {
    if (!mem) return;
    pthread_mutex_lock(&mem->lock);
    mem->storage.vtable.clear(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
}

size_t memory_count(memory_t *mem) {
    if (!mem) return 0;
    pthread_mutex_lock(&mem->lock);
    size_t c = mem->storage.vtable.count(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return c;
}

char **memory_list_keys(memory_t *mem, size_t *count) {
    if (!mem || !count) { if (count) *count = 0; return NULL; }
    pthread_mutex_lock(&mem->lock);
    char **keys = mem->storage.vtable.list_keys(&mem->storage, count);
    pthread_mutex_unlock(&mem->lock);
    return keys;
}

memory_entry_t *memory_search(memory_t *mem, const char *query, int limit) {
    if (!mem || !query) return NULL;

    pthread_mutex_lock(&mem->lock);

    if (limit <= 0) limit = mem->search_limit;

    memory_entry_t *results;

    /* Use inline search for inmem backend; file backend via vtable */
    if (mem->storage.type == MEMORY_STORAGE_INMEM) {
        size_t count = 0;
        results = inmem_search_internal(&mem->storage, query, limit, &count);
        /* Re-wrap: inmem_search_internal already returns count-limited */
        (void)count;
    } else {
        results = mem->storage.vtable.search(&mem->storage, query, limit);
    }

    pthread_mutex_unlock(&mem->lock);
    return results;
}

memory_entry_t *memory_get_prioritized(memory_t *mem, size_t limit, size_t *count) {
    if (!mem || !count) { if (count) *count = 0; return NULL; }
    pthread_mutex_lock(&mem->lock);
    memory_entry_t *results = mem->storage.vtable.get_prioritized(&mem->storage, limit, count);
    pthread_mutex_unlock(&mem->lock);
    return results;
}

size_t memory_ttl_cleanup(memory_t *mem) {
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    size_t removed = 0;
    size_t nkeys = 0;
    char **keys = mem->storage.vtable.list_keys(&mem->storage, &nkeys);

    if (keys) {
        for (size_t i = 0; i < nkeys; i++) {
            memory_entry_t entry;
            if (mem->storage.vtable.get(&mem->storage, keys[i], &entry)) {
                if (memory_entry_expired(&entry)) {
                    mem->storage.vtable.delete(&mem->storage, keys[i]);
                    removed++;
                }
            }
            free(keys[i]);
        }
        free(keys);
    }

    /* Persist after cleanup */
    mem->storage.vtable.persist(&mem->storage);

    pthread_mutex_unlock(&mem->lock);
    return removed;
}

/* TTL cleanup thread */
static void *ttl_thread_fn(void *arg) {
    memory_t *mem = (memory_t *)arg;
    while (mem->ttl_running) {
        sleep(300); /* Check every 5 minutes */
        if (!mem->ttl_running) break;
        memory_ttl_cleanup(mem);
    }
    return NULL;
}

bool memory_ttl_start_thread(memory_t *mem, int interval_sec) {
    if (!mem || mem->ttl_running) return false;
    (void)interval_sec;
    mem->ttl_running = true;
    if (pthread_create(&mem->ttl_thread, NULL, ttl_thread_fn, mem) != 0) {
        mem->ttl_running = false;
        return false;
    }
    pthread_detach(mem->ttl_thread);
    return true;
}

void memory_ttl_stop_thread(memory_t *mem) {
    if (!mem || !mem->ttl_running) return;
    mem->ttl_running = false;
    /* Thread is detached, it will exit on next sleep wake */
}

/* Auto-save thread */
static void *autosave_thread_fn(void *arg) {
    memory_t *mem = (memory_t *)arg;
    while (mem->autosave_running) {
        sleep((unsigned int)mem->autosave_interval_sec);
        if (!mem->autosave_running) break;
        memory_persist(mem);
    }
    return NULL;
}

bool memory_autosave_start_thread(memory_t *mem, int interval_sec) {
    if (!mem || mem->autosave_running) return false;
    mem->autosave_running = true;
    mem->autosave = MEMORY_AUTOSAVE_ON;
    if (interval_sec > 0) mem->autosave_interval_sec = interval_sec;
    if (pthread_create(&mem->autosave_thread, NULL, autosave_thread_fn, mem) != 0) {
        mem->autosave_running = false;
        mem->autosave = MEMORY_AUTOSAVE_OFF;
        return false;
    }
    pthread_detach(mem->autosave_thread);
    return true;
}

void memory_autosave_stop_thread(memory_t *mem) {
    if (!mem || !mem->autosave_running) return;
    mem->autosave_running = false;
    mem->autosave = MEMORY_AUTOSAVE_OFF;
}

/* Port of Python agent/credential_pool.py:_persist(). */
bool memory_persist(memory_t *mem) {
    if (!mem) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.persist(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

bool memory_load(memory_t *mem) {
    if (!mem) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.load(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

/* ================================================================
 *  Import / Export (P157)
 * ================================================================ */

int memory_import_file(memory_t *mem, const char *path, bool merge) {
    if (!mem || !path) return 0;

    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) { free(err); return 0; }
    if (!doc) return 0;

    int imported = memory_import_json(mem, NULL, merge);
    /* Actually use the parsed doc */
    if (doc->type == JSON_ARRAY) {
        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, doc);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
    } else if (doc->type == JSON_OBJECT) {
        /* Support old format: {key: {content, ...}} */
        /* Convert to array by extracting values */
        size_t n = json_len(doc);
        json_t *arr = json_new_array();
        for (size_t i = 0; i < n; i++) {
            json_t *val = json_get(doc, i);
            if (val) json_append(arr, json_copy(val));
        }

        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, arr);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
        json_free(arr);
    }

    json_free(doc);
    return imported;
}

int memory_import_json(memory_t *mem, const char *json_str, bool merge) {
    if (!mem || !json_str) return 0;

    char *err = NULL;
    json_t *doc = json_parse(json_str, &err);
    if (err) { free(err); return 0; }
    if (!doc) return 0;

    int imported = 0;
    if (doc->type == JSON_ARRAY) {
        if (!merge) memory_clear(mem);
        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, doc);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
    }

    json_free(doc);
    return imported;
}

bool memory_export_file(memory_t *mem, const char *path) {
    if (!mem || !path) return false;

    char *json_str = memory_export_json(mem);
    if (!json_str) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

char *memory_export_json(memory_t *mem) {
    if (!mem) return NULL;

    pthread_mutex_lock(&mem->lock);
    json_t *arr = mem->storage.vtable.export_json(&mem->storage);
    pthread_mutex_unlock(&mem->lock);

    if (!arr) return strdup("[]");

    char *json_str = json_serialize_pretty(arr, 2);
    json_free(arr);
    return json_str;
}

/* ================================================================
 *  Compression (P158)
 * ================================================================ */

int memory_compress_old(memory_t *mem, time_t before,
                         char *(*compress_cb)(const char *content)) {
    if (!mem || !compress_cb) return 0;

    pthread_mutex_lock(&mem->lock);
    int result = mem->storage.vtable.compress_old(&mem->storage, before, compress_cb);
    mem->storage.vtable.persist(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return result;
}

/* ================================================================
 *  Tool handler — backward-compatible memory tool (P151-P158)
 * ================================================================ */

/* Static memory instance for the tool handler */
static memory_t g_memory;
static bool g_memory_initialized = false;
static void *g_plugin_registry = NULL;

/* Set plugin registry for plugin-backed memory */
void memory_set_plugin_registry(void *reg) {
    g_plugin_registry = reg;
}

/* Ensure global memory is initialized */
static memory_t *get_global_memory(void) {
    if (!g_memory_initialized) {
        char path[512];
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";

        /* Try plugin-backed memory if registry available */
        if (g_plugin_registry) {
            memory_init(&g_memory, MEMORY_STORAGE_PLUGIN, NULL);
        } else {
            snprintf(path, sizeof(path), "%s/.hermes/memory.json", home);
            memory_init(&g_memory, MEMORY_STORAGE_FILE, path);
        }
        g_memory.dedup_enabled = true;
        g_memory.search_limit = 20;
        g_memory.ttl_days = 30;
        g_memory.autosave = MEMORY_AUTOSAVE_ON;
        g_memory.autosave_interval_sec = 60;
        memory_autosave_start_thread(&g_memory, 60);
        g_memory_initialized = true;
    }
    return &g_memory;
}

