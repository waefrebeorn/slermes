/*
 * _backend.c - C home for persistence/sqlite/_backend.py.
 * Ports the module's pure helpers: the SQLite-error -> contract-error classification, the
 * `translated` error family, the repository registration rules, open()'s refusal order and the
 * holder-usability rule. The holders, executor and connections stay Python (live I/O).
 * PoP: w19_backend_308e86_store_error @ persistence/sqlite/_backend.py:_store_error
 * PoP: w19_backend_308e86_translate @ persistence/sqlite/_backend.py:translated
 * PoP: w19_backend_308e86_register_repository @ persistence/sqlite/_backend.py:register_repository
 * PoP: w19_backend_308e86_open_plan @ persistence/sqlite/_backend.py:SQLiteBackend.open
 * PoP: w19_backend_308e86_usable @ persistence/sqlite/_backend.py:SQLiteBackend._usable
 */
#include "noc_w19_backend_308e86.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* The repository attributes a store handle can carry (CAPABILITY_ATTRIBUTES). */
static const char *const CAPABILITY_ATTRIBUTES[] = {
    "sessions", "messages", "lineage", "search", "usage", "usage_analytics", "admissions",
    "runtime", "bindings", "coordination", "worker_executions", "maintenance", "state_meta",
    "profile_identity", "routing", "hosted_rooms", "hosted_room_peers", "hosted_room_replicas",
    "hosted_room_driver", "hosted_room_policy", "hosted_room_attachments", "health", NULL
};

/* The capability fields a registration may override (_BASE_CAPABILITIES). */
static const char *const CAPABILITY_FIELDS[] = {
    "full_text_search", "substring_search", "transactional_claims", "cross_process", "remote",
    "reopen_durability", "authority_fences", "durable_admissions", "receipted_writes",
    "physical_admin", NULL
};

static const char *const DISK_IO_ERROR_MARKER = "disk i/o error";

static int in_list(const char *const *list, const char *value) {
    if (!value) return 0;
    for (int i = 0; list[i]; i++)
        if (!strcmp(list[i], value)) return 1;
    return 0;
}

/* ASCII-lowercased copy of `s`. */
static char *lower_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c < 0x80) ? (char)tolower(c) : (char)c;
    }
    out[n] = '\0';
    return out;
}

/* `needle in haystack.lower()` */
static int lower_contains(const char *haystack, const char *needle) {
    char *low = lower_dup(haystack ? haystack : "");
    int found = 0;
    if (!low) return 0;
    if (strstr(low, needle)) found = 1;
    free(low);
    return found;
}

static json_t *error_answer(const char *kind, const char *message) {
    json_t *out = json_object();
    json_set(out, "kind", json_string(kind));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

json_t *w19_backend_308e86_store_error(const char *message, int is_disk_full, int is_lock,
                                      int no_more_rows, int fts_corrupt, int malformed,
                                      const char *classify) {
    const char *msg = message ? message : "";
    if (is_disk_full) return error_answer("DiskFull", msg);
    if (is_lock || no_more_rows) return error_answer("StoreContention", msg);
    if (lower_contains(msg, DISK_IO_ERROR_MARKER)) return error_answer("TransientStoreError", msg);
    if (fts_corrupt) return error_answer("SearchIndexCorrupt", msg);
    if (malformed) return error_answer("MalformedSchema", msg);
    if (classify && !strcmp(classify, "corrupt")) return error_answer("CorruptStoreError", msg);
    if (lower_contains(msg, "unable to open database file"))
        return error_answer("StoreUnavailable", msg);
    return error_answer("PermanentStoreError", msg);
}

json_t *w19_backend_308e86_translate(int exc_kind, const char *exc_name, const char *message,
                                    int is_disk_full, int is_lock, int no_more_rows,
                                    int fts_corrupt, int malformed, const char *classify) {
    switch (exc_kind) {
        case 0:
            return json_null();
        case 1:
            return error_answer("DeletedWalGeneration", message);
        case 2:
            return error_answer("StoreReplaced", message);
        case 3:
            return error_answer("StoreQuarantined", message);
        case 4:
            return w19_backend_308e86_store_error(message, is_disk_full, is_lock, no_more_rows,
                                                 fts_corrupt, malformed, classify);
        default: {
            /* Not one of the mapped families: the exception propagates unchanged. */
            json_t *out = error_answer(exc_name ? exc_name : "Exception", message);
            json_set(out, "propagated", json_bool(1));
            return out;
        }
    }
}

/* repr() of a plain string, as the ValueError messages embed it. */
static char *repr_text(const char *value) {
    size_t n;
    char *out;
    if (!value) return NULL;
    n = strlen(value);
    out = (char *)malloc(n + 3);
    if (!out) return NULL;
    out[0] = '\'';
    memcpy(out + 1, value, n);
    out[n + 1] = '\'';
    out[n + 2] = '\0';
    return out;
}

json_t *w19_backend_308e86_register_repository(json_t *already_registered, const char *attribute,
                                              json_t *capabilities) {
    json_t *registered = json_array();
    json_t *overrides = json_object();
    const char *attr = attribute ? attribute : "";
    char buf[512];

    if (already_registered && already_registered->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(already_registered); i++) {
            json_t *v = json_get(already_registered, i);
            if (v && v->type == JSON_STRING) json_append(registered, json_string(v->str_val));
        }

    json_t *out = json_object();
    if (!in_list(CAPABILITY_ATTRIBUTES, attr)) {
        char *shown = repr_text(attr);
        snprintf(buf, sizeof buf, "unknown capability attribute %s", shown ? shown : "");
        free(shown);
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(buf));
        json_set(out, "registered", registered);
        json_set(out, "overrides", overrides);
        return out;
    }
    if (already_registered && already_registered->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(already_registered); i++) {
            json_t *v = json_get(already_registered, i);
            if (v && v->type == JSON_STRING && !strcmp(v->str_val, attr)) {
                char *shown = repr_text(attr);
                snprintf(buf, sizeof buf, "sqlite repository %s already registered", shown ? shown : "");
                free(shown);
                json_set(out, "ok", json_bool(0));
                json_set(out, "error", json_string(buf));
                json_set(out, "registered", registered);
                json_set(out, "overrides", overrides);
                return out;
            }
        }
    }
    /* _REPOSITORY_FACTORIES[attribute] = factory happens before the capability checks. */
    json_append(registered, json_string(attr));
    if (capabilities && capabilities->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(capabilities); i++) {
            const char *name = json_object_get_key_at(capabilities, i);
            if (!name) continue;
            if (!in_list(CAPABILITY_FIELDS, name)) {
                char *shown = repr_text(name);
                snprintf(buf, sizeof buf, "unknown capability field %s", shown ? shown : "");
                free(shown);
                json_set(out, "ok", json_bool(0));
                json_set(out, "error", json_string(buf));
                json_set(out, "registered", registered);
                json_set(out, "overrides", overrides);
                return out;
            }
            json_set(overrides, name, json_copy(json_object_get_at(capabilities, i)));
        }
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "registered", registered);
    json_set(out, "overrides", overrides);
    return out;
}

json_t *w19_backend_308e86_open_plan(json_t *store_keys, const char *scope_key,
                                    const char *profile_dir, int closed) {
    json_t *out = json_object();
    int held = 0;
    if (store_keys && store_keys->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(store_keys); i++) {
            json_t *v = json_get(store_keys, i);
            if (v && v->type == JSON_STRING && scope_key && !strcmp(v->str_val, scope_key)) {
                held = 1;
                break;
            }
        }
    if (!held) {
        char buf[1024];
        snprintf(buf, sizeof buf, "this SQLite backend holds no store for %s",
                 profile_dir ? profile_dir : "");
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("ValueError"));
        json_set(out, "message", json_string(buf));
        return out;
    }
    if (closed) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("RuntimeError"));
        json_set(out, "message", json_string("SQLite backend used after close"));
        return out;
    }
    json_set(out, "ok", json_bool(1));
    return out;
}

int w19_backend_308e86_usable(int has_holder, int read_only, int registry_owned) {
    return (has_holder && (read_only || registry_owned)) ? 1 : 0;
}
