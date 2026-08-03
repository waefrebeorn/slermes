/*
 * mirror.c — Session mirroring for cross-platform message delivery.
 *
 * Port of Python gateway/mirror.py.
 *
 * When a message is sent to a platform (via send_message or cron delivery),
 * this module appends a "delivery-mirror" record to the target session's
 * transcript so the receiving-side agent has context about what was sent.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_mirror.h"
#include "hermes_json.h"
#include "hermes_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include <ctype.h>

/* ================================================================
 *  Internal: get sessions index path
 *  Port of Python _SESSIONS_DIR / _SESSIONS_INDEX
 * ================================================================ */

static const char *mirror_sessions_dir(void) {
    static char dir[1024] = {0};
    if (dir[0] == '\0') {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            snprintf(dir, sizeof(dir), "%s/sessions", home);
        }
    }
    return dir[0] ? dir : NULL;
}

static const char *mirror_sessions_index(void) {
    static char path[1100] = {0};
    if (path[0] == '\0') {
        const char *dir = mirror_sessions_dir();
        if (dir) {
            snprintf(path, sizeof(path), "%s/sessions.json", dir);
        }
    }
    return path[0] ? path : NULL;
}

/* ================================================================
 *  Internal: find session id for platform + chat_id
 *  Port of Python _find_session_id()
 * ================================================================ */

/* PoP: _find_session_id @ gateway/mirror.py:_find_session_id */
/* Port of Python gateway/mirror.py:_find_session_id(). */
static char *mirror_find_session_id(const char *platform,
                                     const char *chat_id,
                                     const char *thread_id,
                                     const char *user_id) {
    const char *index_path = mirror_sessions_index();
    if (!index_path) return NULL;

    struct stat st;
    if (stat(index_path, &st) != 0) return NULL;

    char *err = NULL;
    json_node_t *data = json_parse_file(index_path, &err);
    if (!data) {
        free(err);
        return NULL;
    }

    char *result = NULL;
    char plat_lower[64];
    {
        size_t i;
        for (i = 0; platform[i] && i < sizeof(plat_lower) - 1; i++)
            plat_lower[i] = (char)tolower((unsigned char)platform[i]);
        plat_lower[i] = '\0';
    }

    /* Iterate object entries */
    json_t *jdata = (json_t *)data;
    json_node_t *candidates[64];
    int candidate_count = 0;
    memset(candidates, 0, sizeof(candidates));

    for (size_t i = 0; i < jdata->c.count && candidate_count < 64; i++) {
        json_node_t *entry = jdata->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;

        json_node_t *origin = json_object_get(entry, "origin");
        if (!origin || origin->type != JSON_OBJECT) {
            origin = entry;
        }

        const char *entry_plat = json_object_get_string(origin, "platform", "");
        if (!*entry_plat) {
            entry_plat = json_object_get_string(entry, "platform", "");
        }
        if (strcasecmp(entry_plat, plat_lower) != 0) continue;

        const char *origin_chat = json_object_get_string(origin, "chat_id", "");
        if (!origin_chat || strcmp(origin_chat, chat_id) != 0) continue;

        if (thread_id && *thread_id) {
            const char *origin_thread = json_object_get_string(origin, "thread_id", "");
            if (strcmp(origin_thread ? origin_thread : "", thread_id) != 0) continue;
        }

        candidates[candidate_count++] = entry;
    }

    if (candidate_count == 0) {
        json_free(data);
        return NULL;
    }

    if (user_id && *user_id) {
        json_node_t *exact_matches[64];
        int exact_count = 0;
        for (int ci = 0; ci < candidate_count; ci++) {
            json_node_t *origin = json_object_get(candidates[ci], "origin");
            const char *entry_uid = origin
                ? json_object_get_string(origin, "user_id", "")
                : "";
            if (strcmp(entry_uid, user_id) == 0) {
                exact_matches[exact_count++] = candidates[ci];
            }
        }
        if (exact_count > 0) {
            candidate_count = exact_count;
            memcpy(candidates, exact_matches, sizeof(exact_matches[0]) * exact_count);
        } else if (candidate_count > 1) {
            json_free(data);
            return NULL;
        }
    } else if (candidate_count > 1) {
        const char *first_uid = NULL;
        bool all_same = true;
        for (int ci = 0; ci < candidate_count; ci++) {
            json_node_t *origin = json_object_get(candidates[ci], "origin");
            const char *uid = origin
                ? json_object_get_string(origin, "user_id", "")
                : "";
            if (!first_uid) {
                first_uid = uid;
            } else if (strcmp(uid, first_uid) != 0) {
                all_same = false;
                break;
            }
        }
        if (!all_same) {
            json_free(data);
            return NULL;
        }
    }

    /* Pick the entry with the latest updated_at */
    json_node_t *best = candidates[0];
    const char *best_time = json_object_get_string(best, "updated_at", "");

    for (int ci = 1; ci < candidate_count; ci++) {
        const char *cur_time = json_object_get_string(candidates[ci], "updated_at", "");
        if (strcmp(cur_time, best_time) > 0) {
            best = candidates[ci];
            best_time = cur_time;
        }
    }

    const char *session_id = json_object_get_string(best, "session_id", "");
    if (*session_id) {
        result = strdup(session_id);
    }

    json_free(data);
    return result;
}

/* ================================================================
 *  Internal: append message to session database
 *  Port of Python _append_to_sqlite()
 *
 *  Uses the file-based JSON session store. Loads existing session
 *  data, appends the mirror message to the messages array, saves back.
 * ================================================================ */

static bool mirror_append_to_db(const char *session_id, json_node_t *message) {
    if (!session_id || !*session_id || !message) return false;

    const char *sessions_dir = mirror_sessions_dir();
    if (!sessions_dir) return false;

    db_t *db = db_open(sessions_dir, NULL);
    if (!db) return false;

    /* Load existing session data */
    char *err = NULL;
    char *json_data = db_load(db, session_id, &err);
    json_node_t *session = NULL;

    if (json_data) {
        session = json_parse(json_data, &err);
        free(json_data);
        free(err);
    }

    if (!session) {
        /* Create new session structure */
        session = json_new_object();
        json_object_set(session, "session_id", json_new_string(session_id));
        json_object_set(session, "messages", json_new_array());
    }

    /* Add mirror message to messages array */
    json_node_t *messages = json_object_get(session, "messages");
    if (!messages || messages->type != JSON_ARRAY) {
        messages = json_new_array();
        json_object_set(session, "messages", messages);
    }

    json_array_append(messages, json_copy(message));

    /* Serialize and save */
    char *serialized = json_serialize(session);
    bool ok = false;
    if (serialized) {
        ok = db_save(db, session_id, serialized);
        free(serialized);
    }

    json_free(session);
    db_close(db);
    return ok;
}

/* ================================================================
 *  Mirror a delivery to the target session's transcript
 *  Port of Python mirror_to_session()
 *  AG26: Port of Python gateway/mirror.py:mirror_to_session().
 *
 *  Finds the gateway session that matches the given platform + chat_id,
 *  then writes a mirror entry to the session store.
 *
 *  Returns true if mirrored successfully, false if no matching session
 *  or error occurred. All errors are caught — this is never fatal.
 * ================================================================ */

/* PoP: mirror_to_session @ gateway/mirror.py:mirror_to_session */
bool mirror_to_session(const char *platform,
                        const char *chat_id,
                        const char *message_text,
                        const char *source_label,
                        const char *thread_id,
                        const char *user_id) {
    if (!platform || !chat_id || !message_text) return false;

    char *session_id = mirror_find_session_id(platform, chat_id, thread_id, user_id);
    if (!session_id) {
        return false;
    }

    /* Build mirror message */
    json_node_t *mirror_msg = json_new_object();
    json_object_set(mirror_msg, "role", json_new_string("assistant"));
    json_object_set(mirror_msg, "content", json_new_string(message_text));
    json_object_set(mirror_msg, "mirror", json_new_bool(true));
    json_object_set(mirror_msg, "mirror_source",
                    json_new_string(source_label ? source_label : "cli"));

    bool ok = mirror_append_to_db(session_id, mirror_msg);

    json_free(mirror_msg);
    free(session_id);
    return ok;
}
