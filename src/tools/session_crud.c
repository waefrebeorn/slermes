/*
 * session_crud.c — Session CRUD operations tool for Hermes C.
 * P143: Create, read, update, delete, list sessions with metadata filtering.
 * P146: Tag operations (add/remove).
 * P148: Export sessions (JSON/Markdown).
 * P149: Branch sessions.
 * P150: Migration.
 */


/* PoP: session CRUD (C SQLite implementation) */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_db.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"operation\":{"
        "\"type\":\"string\","
        "\"enum\":[\"list\",\"create\",\"delete\",\"info\",\"add_tag\",\"remove_tag\","
                 "\"export_json\",\"export_markdown\",\"branch\",\"migrate\","
                 "\"set_title\",\"get_title\",\"find_by_title\",\"end\",\"reopen\",\"resolve\",\"export_all\",\"get_meta\",\"set_meta\","
                 "\"set_system_prompt\",\"get_system_prompt\",\"bind_telegram_topic\",\"get_telegram_topic_binding\"],"        "\"description\":\"Operation to perform\""
      "},"
      "\"session_id\":{\"type\":\"string\",\"description\":\"Session ID (required for create/delete/info/tag/export/branch)\"},"
      "\"tag\":{\"type\":\"string\",\"description\":\"Tag to add or remove\"},"
      "\"title\":{\"type\":\"string\",\"description\":\"Title for set_title or find_by_title operation\"},"
      "\"branch_point\":{\"type\":\"number\",\"description\":\"Message index to branch at (0-based)\",\"default\":-1},"
      "\"new_id\":{\"type\":\"string\",\"description\":\"New session ID for branch operation\"},"
      "\"tag_filter\":{\"type\":\"string\",\"description\":\"Filter list by tag\"},"
      "\"limit\":{\"type\":\"number\",\"description\":\"Max results for list\",\"default\":50},"
      "\"key\":{\"type\":\"string\",\"description\":\"Meta key (for get_meta/set_meta)\"},\n      \"\"value\":{\"type\":\"string\",\"description\":\"Meta value (for set_meta)\"},\n      \"\"retention_days\":{\"type\":\"number\",\"description\":\"Retention days for prune\",\"default\":0}"
    "},"
    "\"required\":[\"operation\"]"
"}";

/* Get session db directory from state */
static db_t *open_session_db(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    char db_dir[4096];
    snprintf(db_dir, sizeof(db_dir), "%s/.hermes/sessions", home);
    return db_open(db_dir, NULL);
}

char *session_crud_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *op = json_object_get_string(args, "operation", "");
    const char *session_id = json_object_get_string(args, "session_id", NULL);
    const char *tag = json_object_get_string(args, "tag", NULL);
    const char *title_val = json_object_get_string(args, "title", NULL);
    int branch_point = (int)json_object_get_number(args, "branch_point", -1);
    const char *new_id = json_object_get_string(args, "new_id", "");
    const char *tag_filter = json_object_get_string(args, "tag_filter", NULL);
    const char *meta_key = json_object_get_string(args, "key", NULL);
    const char *meta_value = json_object_get_string(args, "value", NULL);
    const char *holder = json_object_get_string(args, "holder", NULL);
    int limit = (int)json_object_get_number(args, "limit", 50);
    int lock_ttl = (int)json_object_get_number(args, "lock_ttl", 60);
    int retention_days = (int)json_object_get_number(args, "retention_days", 0);

    json_node_t *result = json_new_object();

    db_t *db = open_session_db();
    if (!db) {
        json_object_set(result, "error", json_new_string("Cannot open session DB"));
        char *out = json_serialize(result);
        json_free(result);
        json_free(args);
        return out;
    }

    if (strcmp(op, "list") == 0) {
        /* List sessions with optional tag filter */
        size_t count = 0;
        db_session_entry_t *entries = db_list_with_meta(db, &count);
        json_node_t *sessions = json_new_array();
        int listed = 0;

        for (size_t i = 0; i < count && listed < limit; i++) {
            bool include = true;
            if (tag_filter && *tag_filter) {
                include = false;
                for (int t = 0; t < entries[i].meta.tag_count; t++) {
                    if (strstr(entries[i].meta.tags[t], tag_filter)) {
                        include = true;
                        break;
                    }
                }
            }
            if (include) {
                json_node_t *s = json_new_object();
                json_object_set(s, "session_id", json_new_string(entries[i].id));
                json_object_set(s, "title", json_new_string(entries[i].meta.title[0] ? entries[i].meta.title : entries[i].id));
                json_object_set(s, "model", json_new_string(entries[i].meta.model));
                json_object_set(s, "message_count", json_new_number((double)entries[i].meta.message_count));
                json_object_set(s, "token_count", json_new_number((double)entries[i].meta.token_count));
                json_object_set(s, "created_at", json_new_number((double)entries[i].meta.created_at));

                /* Add tags array */
                json_node_t *tags = json_new_array();
                for (int t = 0; t < entries[i].meta.tag_count; t++)
                    json_array_append(tags, json_new_string(entries[i].meta.tags[t]));
                json_object_set(s, "tags", tags);

                json_array_append(sessions, s);
                listed++;
            }
        }
        free(entries);

        json_object_set(result, "operation", json_new_string("list"));
        json_object_set(result, "count", json_new_number((double)listed));
        json_object_set(result, "total", json_new_number((double)count));
        json_object_set(result, "sessions", sessions);

    } else if (strcmp(op, "create") == 0) {
        /* Create a new empty session */
        char new_id_buf[64];
        if (!session_id || !*session_id) {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            snprintf(new_id_buf, sizeof(new_id_buf), "%04d%02d%02d_%02d%02d%02d",
                     tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec);
            session_id = new_id_buf;
        }

        if (db_save(db, session_id, "[]") && db_save_meta(db, session_id, &(session_meta_t){
                .schema_version = 1,
                .created_at = time(NULL),
                .updated_at = time(NULL),
                .branch_point = -1
            })) {
            json_object_set(result, "operation", json_new_string("create"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "success", json_new_bool(true));
        } else {
            json_object_set(result, "error", json_new_string("Failed to create session"));
        }

    } else if (strcmp(op, "delete") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            bool ok = db_delete(db, session_id);
            json_object_set(result, "operation", json_new_string("delete"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "success", json_new_bool(ok));
            if (!ok)
                json_object_set(result, "warning", json_new_string("Session not found or already deleted"));
        }

    } else if (strcmp(op, "info") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            json_object_set(result, "operation", json_new_string("info"));
            json_object_set(result, "session_id", json_new_string(session_id));

            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                json_object_set(result, "title", json_new_string(meta.title));
                json_object_set(result, "model", json_new_string(meta.model));
                json_object_set(result, "message_count", json_new_number((double)meta.message_count));
                json_object_set(result, "token_count", json_new_number((double)meta.token_count));
                json_object_set(result, "created_at", json_new_number((double)meta.created_at));
                json_object_set(result, "updated_at", json_new_number((double)meta.updated_at));
                json_object_set(result, "schema_version", json_new_number((double)meta.schema_version));

                json_node_t *tags = json_new_array();
                for (int t = 0; t < meta.tag_count; t++)
                    json_array_append(tags, json_new_string(meta.tags[t]));
                json_object_set(result, "tags", tags);

                if (meta.parent_id[0])
                    json_object_set(result, "parent_id", json_new_string(meta.parent_id));
                if (meta.branch_point >= 0)
                    json_object_set(result, "branch_point", json_new_number((double)meta.branch_point));
            } else {
                json_object_set(result, "found", json_new_bool(false));
                json_object_set(result, "warning", json_new_string("No metadata found"));
            }

            /* Check if session data exists */
            char *data = db_load(db, session_id, NULL);
            json_object_set(result, "has_data", json_new_bool(data != NULL));
            if (data) {
                /* Count messages roughly */
                int msg_count = 0;
                const char *p = data;
                int depth = 0;
                while (*p) {
                    if (*p == '{') { if (depth == 0) msg_count++; depth++; }
                    else if (*p == '}') depth--;
                    p++;
                }
                json_object_set(result, "data_message_count", json_new_number((double)(msg_count < 0 ? 0 : msg_count)));
                free(data);
            }
        }

    } else if (strcmp(op, "add_tag") == 0) {
        if (!session_id || !*session_id || !tag || !*tag) {
            json_object_set(result, "error", json_new_string("session_id and tag required"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);

            bool found = false;
            for (int i = 0; i < meta.tag_count; i++) {
                if (strcmp(meta.tags[i], tag) == 0) { found = true; break; }
            }

            if (!found && meta.tag_count < 32) {
                snprintf(meta.tags[meta.tag_count], sizeof(meta.tags[0]), "%s", tag);
                meta.tag_count++;
                meta.updated_at = time(NULL);
                db_save_meta(db, session_id, &meta);
            }

            json_object_set(result, "operation", json_new_string("add_tag"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "tag", json_new_string(tag));
            json_object_set(result, "success", json_new_bool(true));
        }

    } else if (strcmp(op, "remove_tag") == 0) {
        if (!session_id || !*session_id || !tag || !*tag) {
            json_object_set(result, "error", json_new_string("session_id and tag required"));
        } else {
            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                int found = -1;
                for (int i = 0; i < meta.tag_count; i++) {
                    if (strcmp(meta.tags[i], tag) == 0) { found = i; break; }
                }
                if (found >= 0) {
                    for (int i = found; i < meta.tag_count - 1; i++)
                        snprintf(meta.tags[i], sizeof(meta.tags[0]), "%s", meta.tags[i + 1]);
                    meta.tag_count--;
                    meta.updated_at = time(NULL);
                    db_save_meta(db, session_id, &meta);
                    json_object_set(result, "success", json_new_bool(true));
                } else {
                    json_object_set(result, "success", json_new_bool(false));
                    json_object_set(result, "warning", json_new_string("Tag not found"));
                }
            } else {
                json_object_set(result, "success", json_new_bool(false));
                json_object_set(result, "warning", json_new_string("Session not found"));
            }
            json_object_set(result, "operation", json_new_string("remove_tag"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "tag", json_new_string(tag));
        }

    } else if (strcmp(op, "set_title") == 0) {
        if (!session_id || !*session_id || !title_val || !*title_val) {
            json_object_set(result, "error", json_new_string("session_id and title required"));
        } else {
            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                snprintf(meta.title, sizeof(meta.title), "%s", title_val);
                meta.updated_at = time(NULL);
                db_save_meta(db, session_id, &meta);
                json_object_set(result, "operation", json_new_string("set_title"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "title", json_new_string(meta.title));
                json_object_set(result, "success", json_new_bool(true));
            } else {
                json_object_set(result, "error", json_new_string("Session not found"));
            }
        }

    } else if (strcmp(op, "get_title") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                json_object_set(result, "operation", json_new_string("get_title"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "title", json_new_string(meta.title[0] ? meta.title : ""));
                json_object_set(result, "success", json_new_bool(true));
            } else {
                json_object_set(result, "error", json_new_string("Session not found"));
            }
        }

    } else if (strcmp(op, "find_by_title") == 0) {
        /* Find sessions whose title contains the search string (case-insensitive) */
        if (!title_val || !*title_val) {
            json_object_set(result, "error", json_new_string("title required for find_by_title"));
        } else {
            size_t count = 0;
            db_session_entry_t *entries = db_list_with_meta(db, &count);
            json_node_t *matches = json_new_array();
            int found = 0;

            for (size_t i = 0; i < count && found < limit; i++) {
                if (entries[i].meta.title[0]) {
                    /* Case-insensitive substring match */
                    bool match = false;
                    const char *p = entries[i].meta.title;
                    while (*p) {
                        const char *tp = title_val;
                        const char *sp = p;
                        while (*sp && *tp && tolower((unsigned char)*sp) == tolower((unsigned char)*tp)) {
                            sp++; tp++;
                        }
                        if (!*tp) { match = true; break; }
                        p++;
                    }
                    if (match) {
                        json_node_t *s = json_new_object();
                        json_object_set(s, "session_id", json_new_string(entries[i].id));
                        json_object_set(s, "title", json_new_string(entries[i].meta.title));
                        json_object_set(s, "model", json_new_string(entries[i].meta.model));
                        json_object_set(s, "message_count", json_new_number((double)entries[i].meta.message_count));
                        json_object_set(s, "token_count", json_new_number((double)entries[i].meta.token_count));
                        json_object_set(s, "created_at", json_new_number((double)entries[i].meta.created_at));
                        json_object_set(s, "source", json_new_string(entries[i].meta.source));

                        json_node_t *tags = json_new_array();
                        for (int t = 0; t < entries[i].meta.tag_count; t++)
                            json_array_append(tags, json_new_string(entries[i].meta.tags[t]));
                        json_object_set(s, "tags", tags);

                        json_array_append(matches, s);
                        found++;
                    }
                }
            }
            free(entries);

            json_object_set(result, "operation", json_new_string("find_by_title"));
            json_object_set(result, "query", json_new_string(title_val));
            json_object_set(result, "count", json_new_number((double)found));
            json_object_set(result, "sessions", matches);
        }

    } else if (strcmp(op, "resolve") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            /* First try exact match */
            if (db_exists(db, session_id)) {
                json_object_set(result, "operation", json_new_string("resolve"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "resolved", json_new_string(session_id));
            } else {
                /* Prefix match: list all sessions, find unique prefix */
                size_t count = 0;
                char **ids = db_list(db, &count);
                const char *match = NULL;
                size_t prefix_len = strlen(session_id);

                for (size_t i = 0; i < count; i++) {
                    if (strncmp(ids[i], session_id, prefix_len) == 0) {
                        if (match) {
                            /* Ambiguous — more than one match */
                            match = NULL;
                            break;
                        }
                        match = ids[i];
                    }
                }

                if (match) {
                    json_object_set(result, "operation", json_new_string("resolve"));
                    json_object_set(result, "session_id", json_new_string(session_id));
                    json_object_set(result, "resolved", json_new_string(match));
                } else {
                    json_object_set(result, "operation", json_new_string("resolve"));
                    json_object_set(result, "session_id", json_new_string(session_id));
                    json_object_set(result, "resolved", json_new_null());
                }

                for (size_t i = 0; i < count; i++)
                    free(ids[i]);
                free(ids);
            }
        }

    } else if (strcmp(op, "export_json") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            char *exported = db_export_json(db, session_id);
            if (exported) {
                json_object_set(result, "operation", json_new_string("export_json"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "data", json_new_string(exported));
                free(exported);
            } else {
                json_object_set(result, "error", json_new_string("Failed to export session"));
            }
        }

    } else if (strcmp(op, "export_markdown") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            char *exported = db_export_markdown(db, session_id);
            if (exported) {
                json_object_set(result, "operation", json_new_string("export_markdown"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "data", json_new_string(exported));
                free(exported);
            } else {
                json_object_set(result, "error", json_new_string("Failed to export session"));
            }
        }

    } else if (strcmp(op, "branch") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else if (branch_point < 0) {
            json_object_set(result, "error", json_new_string("branch_point (>= 0) required"));
        } else {
            char new_id_buf[64];
            if (!new_id || !*new_id) {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                snprintf(new_id_buf, sizeof(new_id_buf), "%04d%02d%02d_%02d%02d%02d_br",
                         tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                         tm->tm_hour, tm->tm_min, tm->tm_sec);
                new_id = new_id_buf;
            }

            char *branched = db_branch(db, session_id, new_id, branch_point);
            if (branched) {
                json_object_set(result, "operation", json_new_string("branch"));
                json_object_set(result, "source_id", json_new_string(session_id));
                json_object_set(result, "new_id", json_new_string(new_id));
                json_object_set(result, "branch_point", json_new_number((double)branch_point));
                json_object_set(result, "success", json_new_bool(true));
                free(branched);
            } else {
                json_object_set(result, "error", json_new_string("Failed to branch session"));
            }
        }

    } else if (strcmp(op, "migrate") == 0) {
        int migrated = migrate(db);
        json_object_set(result, "operation", json_new_string("migrate"));
        json_object_set(result, "migrated_count", json_new_number((double)migrated));
        json_object_set(result, "success", json_new_bool(true));

        if (retention_days > 0) {
            int pruned = db_prune_by_age(db, retention_days);
            json_object_set(result, "pruned_count", json_new_number((double)pruned));
        }

    } else if (strcmp(op, "end") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            session_meta_t meta;
            db_meta_init(&meta);
            bool has_meta = db_load_meta(db, session_id, &meta);
            if (has_meta && meta.ended_at != 0) {
                json_object_set(result, "error", json_new_string("Session already ended"));
            } else if (!has_meta) {
                json_object_set(result, "error", json_new_string("Session not found"));
            } else {
                meta.ended_at = time(NULL);
                if (title_val && *title_val)
                    snprintf(meta.end_reason, sizeof(meta.end_reason), "%s", title_val);
                else
                    snprintf(meta.end_reason, sizeof(meta.end_reason), "user_exit");
                db_save_meta(db, session_id, &meta);
                json_object_set(result, "operation", json_new_string("end"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "success", json_new_bool(true));
            }
        }

    } else if (strcmp(op, "reopen") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            session_meta_t meta;
            db_meta_init(&meta);
            bool has_meta = db_load_meta(db, session_id, &meta);
            if (!has_meta) {
                json_object_set(result, "error", json_new_string("Session not found"));
            } else {
                meta.ended_at = 0;
                meta.end_reason[0] = '\0';
                db_save_meta(db, session_id, &meta);
                json_object_set(result, "operation", json_new_string("reopen"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "success", json_new_bool(true));
            }
        }

    } else if (strcmp(op, "set_meta") == 0) {
        if (!session_id || !*session_id || !meta_key || !*meta_key) {
            json_object_set(result, "error", json_new_string("session_id and key required"));
        } else {
            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                /* Parse existing meta_json, set key, serialize back */
                char *new_json = NULL;
                if (meta.meta_json[0]) {
                    /* Simple JSON key set: parse, modify, serialize */
                    json_node_t *obj = json_parse(meta.meta_json, NULL);
                    if (obj) {
                        json_object_set(obj, meta_key, json_new_string(meta_value ? meta_value : ""));
                        new_json = json_serialize(obj);
                        json_free(obj);
                    }
                }
                if (!new_json) {
                    /* Create new JSON object with just this key */
                    json_node_t *obj = json_new_object();
                    json_object_set(obj, meta_key, json_new_string(meta_value ? meta_value : ""));
                    new_json = json_serialize(obj);
                    json_free(obj);
                }
                if (new_json) {
                    snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", new_json);
                    meta.updated_at = time(NULL);
                    db_save_meta(db, session_id, &meta);
                    free(new_json);
                }
                json_object_set(result, "operation", json_new_string("set_meta"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "key", json_new_string(meta_key));
                json_object_set(result, "success", json_new_bool(true));
            } else {
                json_object_set(result, "error", json_new_string("Session not found"));
            }
        }

    } else if (strcmp(op, "get_meta") == 0) {
        if (!session_id || !*session_id || !meta_key || !*meta_key) {
            json_object_set(result, "error", json_new_string("session_id and key required"));
        } else {
            session_meta_t meta;
            if (db_load_meta(db, session_id, &meta)) {
                const char *found_value = NULL;
                if (meta.meta_json[0]) {
                    json_node_t *obj = json_parse(meta.meta_json, NULL);
                    if (obj) {
                        found_value = json_object_get_string(obj, meta_key, NULL);
                        json_free(obj);
                    }
                }
                json_object_set(result, "operation", json_new_string("get_meta"));
                json_object_set(result, "session_id", json_new_string(session_id));
                json_object_set(result, "key", json_new_string(meta_key));
                json_object_set(result, "value", json_new_string(found_value ? found_value : ""));
                json_object_set(result, "found", json_new_bool(found_value != NULL));
            } else {
                json_object_set(result, "error", json_new_string("Session not found"));
            }
        }

    } else if (strcmp(op, "try_acquire_compression_lock") == 0) {
        if (!session_id || !*session_id || !holder || !*holder) {
            json_object_set(result, "error", json_new_string("session_id and holder required"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);
            /* Use meta_json to store compression lock: {"holder": "X", "expires_at": N} */
            json_node_t *lock_obj = json_new_object();
            json_object_set(lock_obj, "holder", json_new_string(holder));
            time_t expires = time(NULL) + lock_ttl;
            json_object_set(lock_obj, "expires_at", json_new_number((double)expires));
            char *lock_json = json_serialize(lock_obj);
            json_free(lock_obj);
            if (lock_json) {
                /* Store lock in a sidecar file */
                char lock_path[8192];
                snprintf(lock_path, sizeof(lock_path),
                         "%s/.hermes/sessions/.compression_%s.json",
                         getenv("HERMES_HOME") ? getenv("HERMES_HOME") : getenv("HOME") ? getenv("HOME") : "/tmp",
                         session_id);
                FILE *f = fopen(lock_path, "w");
                if (f) {
                    fputs(lock_json, f);
                    fclose(f);
                    json_object_set(result, "operation", json_new_string("try_acquire_compression_lock"));
                    json_object_set(result, "acquired", json_new_bool(true));
                    json_object_set(result, "session_id", json_new_string(session_id));
                    json_object_set(result, "holder", json_new_string(holder));
                } else {
                    json_object_set(result, "acquired", json_new_bool(false));
                    json_object_set(result, "error", json_new_string("Cannot write lock file"));
                }
                free(lock_json);
            } else {
                json_object_set(result, "acquired", json_new_bool(false));
                json_object_set(result, "error", json_new_string("JSON serialization failed"));
            }
        }

    } else if (strcmp(op, "release_compression_lock") == 0) {
        if (!session_id || !*session_id || !holder || !*holder) {
            json_object_set(result, "error", json_new_string("session_id and holder required"));
        } else {
            char lock_path[8192];
            snprintf(lock_path, sizeof(lock_path),
                     "%s/.hermes/sessions/.compression_%s.json",
                     getenv("HERMES_HOME") ? getenv("HERMES_HOME") : getenv("HOME") ? getenv("HOME") : "/tmp",
                     session_id);
            /* Read lock file, verify holder matches */
            FILE *f = fopen(lock_path, "r");
            if (!f) {
                json_object_set(result, "released", json_new_bool(false));
                json_object_set(result, "error", json_new_string("No lock exists"));
            } else {
                char buf[4096];
                size_t n = fread(buf, 1, sizeof(buf)-1, f);
                buf[n] = '\0';
                fclose(f);
                char *err = NULL;
                json_node_t *lock = json_parse(buf, &err);
                if (lock) {
                    const char *lock_holder = json_object_get_string(lock, "holder", NULL);
                    if (lock_holder && strcmp(lock_holder, holder) == 0) {
                        remove(lock_path);
                        json_object_set(result, "released", json_new_bool(true));
                    } else {
                        json_object_set(result, "released", json_new_bool(false));
                        json_object_set(result, "error", json_new_string("Holder mismatch"));
                    }
                    json_free(lock);
                } else {
                    json_object_set(result, "released", json_new_bool(false));
                    json_object_set(result, "error", json_new_string("Corrupt lock file"));
                    free(err);
                }
            }
            json_object_set(result, "operation", json_new_string("release_compression_lock"));
            json_object_set(result, "session_id", json_new_string(session_id));
        }

    } else if (strcmp(op, "get_compression_lock_holder") == 0) {
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            char lock_path[8192];
            snprintf(lock_path, sizeof(lock_path),
                     "%s/.hermes/sessions/.compression_%s.json",
                     getenv("HERMES_HOME") ? getenv("HERMES_HOME") : getenv("HOME") ? getenv("HOME") : "/tmp",
                     session_id);
            FILE *f = fopen(lock_path, "r");
            if (!f) {
                json_object_set(result, "holder", json_new_string(""));
                json_object_set(result, "locked", json_new_bool(false));
            } else {
                char buf[4096];
                size_t n = fread(buf, 1, sizeof(buf)-1, f);
                buf[n] = '\0';
                fclose(f);
                char *err = NULL;
                json_node_t *lock = json_parse(buf, &err);
                if (lock) {
                    const char *lock_holder = json_object_get_string(lock, "holder", NULL);
                    double expires = json_object_get_number(lock, "expires_at", 0);
                    time_t now = time(NULL);
                    if (expires > 0 && (double)now > expires) {
                        /* Lock expired */
                        remove(lock_path);
                        json_object_set(result, "holder", json_new_string(""));
                        json_object_set(result, "locked", json_new_bool(false));
                        json_object_set(result, "expired", json_new_bool(true));
                    } else {
                        json_object_set(result, "holder", json_new_string(lock_holder ? lock_holder : ""));
                        json_object_set(result, "locked", json_new_bool(lock_holder != NULL));
                    }
                    json_free(lock);
                } else {
                    json_object_set(result, "holder", json_new_string(""));
                    json_object_set(result, "locked", json_new_bool(false));
                    free(err);
                }
            }
            json_object_set(result, "operation", json_new_string("get_compression_lock_holder"));
            json_object_set(result, "session_id", json_new_string(session_id));
        }

    } else if (strcmp(op, "export_all") == 0) {
        /* Export all sessions as JSON array with full message data */
        size_t count = 0;
        db_session_entry_t *entries = db_list_with_meta(db, &count);
        json_node_t *exports = json_new_array();
        int exported = 0;

        for (size_t i = 0; i < count; i++) {
            char *json_str = db_export_json(db, entries[i].id);
            if (json_str) {
                json_node_t *session_export = json_parse(json_str, NULL);
                if (session_export) {
                    json_array_append(exports, session_export);
                    exported++;
                }
                free(json_str);
            }
        }
        free(entries);

        json_object_set(result, "operation", json_new_string("export_all"));
        json_object_set(result, "count", json_new_number((double)exported));
        json_object_set(result, "exports", exports);

    } else if (strcmp(op, "set_system_prompt") == 0) {
        /* SE04: Set per-session system prompt */
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else if (!meta_key || !*meta_key) {
            json_object_set(result, "error", json_new_string("prompt value required in 'key' field"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);
            {
                json_node_t *root = json_parse(meta.meta_json[0] ? meta.meta_json : "{}", NULL);
                if (!root) root = json_new_object();
                json_object_set(root, "system_prompt", json_new_string(meta_key));
                char *new_json = json_serialize(root);
                json_free(root);
                if (new_json) {
                    snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", new_json);
                    free(new_json);
                }
            }
            meta.updated_at = time(NULL);
            db_save_meta(db, session_id, &meta);
            json_object_set(result, "operation", json_new_string("set_system_prompt"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "success", json_new_bool(true));
        }

    } else if (strcmp(op, "get_system_prompt") == 0) {
        /* SE04: Get per-session system prompt */
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);
            const char *prompt_val = "";
            {
                json_node_t *root = json_parse(meta.meta_json[0] ? meta.meta_json : "{}", NULL);
                if (root) {
                    prompt_val = json_get_str(root, "system_prompt", "");
                    json_free(root);
                }
            }
            json_object_set(result, "operation", json_new_string("get_system_prompt"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "value", json_new_string(prompt_val));
        }

    } else if (strcmp(op, "bind_telegram_topic") == 0) {
        /* SE07: Bind Telegram topic to session */
        if (!session_id || !*session_id || !meta_key || !*meta_key) {
            json_object_set(result, "error", json_new_string("session_id and topic_id (in 'key') required"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);
            {
                json_node_t *root = json_parse(meta.meta_json[0] ? meta.meta_json : "{}", NULL);
                if (!root) root = json_new_object();
                json_object_set(root, "telegram_topic_id", json_new_string(meta_key));
                char *new_json = json_serialize(root);
                json_free(root);
                if (new_json) {
                    snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", new_json);
                    free(new_json);
                }
            }
            meta.updated_at = time(NULL);
            db_save_meta(db, session_id, &meta);
            json_object_set(result, "operation", json_new_string("bind_telegram_topic"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "topic_id", json_new_string(meta_key));
            json_object_set(result, "success", json_new_bool(true));
        }

    } else if (strcmp(op, "get_telegram_topic_binding") == 0) {
        /* SE07: Get Telegram topic binding for session */
        if (!session_id || !*session_id) {
            json_object_set(result, "error", json_new_string("session_id required"));
        } else {
            session_meta_t meta;
            db_load_meta(db, session_id, &meta);
            const char *topic_id = "";
            {
                json_node_t *root = json_parse(meta.meta_json[0] ? meta.meta_json : "{}", NULL);
                if (root) {
                    topic_id = json_get_str(root, "telegram_topic_id", "");
                    json_free(root);
                }
            }
            json_object_set(result, "operation", json_new_string("get_telegram_topic_binding"));
            json_object_set(result, "session_id", json_new_string(session_id));
            json_object_set(result, "topic_id", json_new_string(topic_id));
        }

    } else {
        json_object_set(result, "error", json_new_string("Unknown operation"));
    }

    db_close(db);

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_session_crud(void) {
    registry_register("session_crud",
        "Session CRUD operations: list, create, delete, info, add_tag, remove_tag, "
        "export_json, export_markdown, branch, migrate, set_title, get_title, find_by_title, export_all, get_meta, set_meta, "
        "set_system_prompt, get_system_prompt, bind_telegram_topic, get_telegram_topic_binding, compression_locks. "
        "Manage conversation sessions with full metadata support.",
        SCHEMA, session_crud_handler);
}
