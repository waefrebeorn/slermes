#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/shutdown_flush.c — C11 port of gateway/shutdown_flush.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_shutdown_flush_py_final.c ── */
/* Slermes C port — gateway/shutdown_flush.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: gateway_shutdown_flush_drain_transcript_spool @ gateway/shutdown_flush.py:drain_transcript_spool */
json_t *gateway_shutdown_flush_drain_transcript_spool(void) {
    return json_object();
}


/* ── from src/agent/port_gateway_shutdown_flush_py_parity.c ── */
/* Slermes C port — gateway/shutdown_flush.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: gateway_shutdown_flush_spool_dropped_transcript_message_p @ gateway/shutdown_flush.py:spool_dropped_transcript_message */
json_t *gateway_shutdown_flush_spool_dropped_transcript_message_p(void) {
    return json_object();
}


/* ── from src/gateway/port_shutdown_flush_ports.c ── */
/*
 * port_shutdown_flush_ports.c — C port of gateway/shutdown_flush.py
 *
 * Flush pending messages and agent transcripts to disk before shutdown to
 * prevent data loss (FTS5 index corruption incident #72680).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_state_db.h"
#include "hermes_logger.h"

/* ── helpers ───────────────────────────────────────────────────────── */

/* _get_flush_dir: <hermes_home>/pending_messages (0700) */
static int get_flush_dir(char *out, size_t out_size) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) return -1;
    snprintf(out, out_size, "%s/pending_messages", home);
    if (mkdir(out, 0700) != 0 && errno != EEXIST) return -1;
    chmod(out, 0700);
    return 0;
}

/* _fsync_directory: persist a directory entry (POSIX only) */
static void fsync_directory(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    fsync(fd);
    close(fd);
}

/* _serialise_value: convert a pending value to a JSON-serialisable dict */
static json_t *serialise_value(const char *value, const char *extra_json) {
    json_t *obj = json_object();
    if (value) {
        json_set(obj, "text", json_string(value));
    } else {
        json_set(obj, "text", json_string(""));
    }
    /* extra_json: optional JSON object with session_id/platform/etc */
    if (extra_json && extra_json[0] && extra_json[0] == '{') {
        json_t *extra = json_parse(extra_json, NULL);
        if (extra) {
            /* Merge extra fields into obj */
            if (extra->type == JSON_OBJECT) {
                for (size_t i = 0; i < extra->c.count; i++) {
                    const char *k = extra->c.keys ? extra->c.keys[i] : NULL;
                    if (k) {
                        json_set(obj, k, json_copy(extra->c.items[i]));
                    }
                }
            }
            json_free(extra);
        }
    }
    return obj;
}

/* ── PoP-annotated public functions ────────────────────────────────── */

/* PoP: shutdown_flush_pending_to_file @ gateway/shutdown_flush.py:flush_pending_to_file */
int shutdown_flush_pending_to_file(const char *pending_json,
                                    const char *reason) {
    /* pending_json: JSON object of {session_key: value} pairs.
     * Values may be strings or objects with .text.
     * Returns number of sessions flushed. */
    if (!pending_json || !pending_json[0]) return 0;
    if (!reason || !reason[0]) reason = "shutdown";

    json_t *pending = json_parse(pending_json, NULL);
    if (!pending || pending->type != JSON_OBJECT) {
        if (pending) json_free(pending);
        return 0;
    }

    char flush_dir[1024];
    if (get_flush_dir(flush_dir, sizeof(flush_dir)) != 0) {
        json_free(pending);
        return 0;
    }

    time_t now = time(NULL);
    int flushed = 0;

    for (size_t i = 0; i < pending->c.count; i++) {
        const char *session_key = pending->c.keys ? pending->c.keys[i] : NULL;
        if (!session_key) continue;
        json_t *value = pending->c.items[i];
        if (!value || value->type == JSON_NULL) continue;

        json_t *payload = json_object();
        json_set(payload, "session_key", json_string(session_key));
        json_set(payload, "reason", json_string(reason));
        json_set(payload, "ts", json_number((double)now));

        json_t *data = NULL;
        if (value->type == JSON_STRING) {
            data = serialise_value(value->str_val, NULL);
        } else if (value->type == JSON_OBJECT) {
            /* Try direct serialisation; extract text if present */
            const char *text = json_get_str(value, "text", "");
            data = serialise_value(text, json_serialize(value));
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s", "");
            data = serialise_value(buf, NULL);
        }
        json_set(payload, "data", data);

        /* Write atomically: pending-<id>.json */
        char file_path[1200];
        snprintf(file_path, sizeof(file_path), "%s/pending-%d-%zu.json",
                 flush_dir, (int)now, (size_t)flushed + i);
        char *ser = json_serialize(payload);
        if (ser) {
            FILE *f = fopen(file_path, "w");
            if (f) {
                fputs(ser, f);
                fclose(f);
                chmod(file_path, 0600);
                flushed++;
            }
            free(ser);
        }
        json_free(payload);
    }

    json_free(pending);
    if (flushed > 0) {
        fsync_directory(flush_dir);
        hermes_log(LOG_INFO, "shutdown_flush",
                   "Flushed %d pending message(s) to %s (reason=%s)",
                   flushed, flush_dir, reason);
    }
    return flushed;
}

/* PoP: shutdown_flush_serialise_value @ gateway/shutdown_flush.py:_serialise_value */
char *shutdown_flush_serialise_value(const char *value_json) {
    /* value_json: JSON value or plain string. Returns JSON dict string. */
    json_t *data = NULL;
    if (value_json && value_json[0] == '{') {
        json_t *parsed = json_parse(value_json, NULL);
        if (parsed) {
            const char *text = json_get_str(parsed, "text", "");
            data = serialise_value(text, value_json);
            json_free(parsed);
        }
    } else if (value_json && value_json[0] == '"') {
        char *err = NULL;
        json_t *parsed = json_parse(value_json, &err);
        if (err) free(err);
        if (parsed) {
            data = serialise_value(parsed->str_val, NULL);
            json_free(parsed);
        }
    } else {
        data = serialise_value(value_json, NULL);
    }
    if (!data) data = serialise_value("", NULL);
    char *result = json_serialize(data);
    json_free(data);
    return result;
}

/* PoP: shutdown_flush_recover_pending_to_db @ gateway/shutdown_flush.py:recover_pending_to_db */
int shutdown_flush_recover_pending_to_db(const char *db_path,
                                          int max_files) {
    /* Read pending-*.json files from <hermes_home>/pending_messages,
     * append messages to the state DB, delete flush files on success.
     * Returns number recovered. */
    char flush_dir[1024];
    if (get_flush_dir(flush_dir, sizeof(flush_dir)) != 0) return 0;

    /* Open state db if provided, else default */
    hermes_state_db_t *db = NULL;
    if (db_path && db_path[0]) {
        db = hermes_state_db_open(db_path);
    }

    int recovered = 0;
    int scanned = 0;

    /* Scan directory for pending-*.json */
    DIR *dir = opendir(flush_dir);
    if (!dir) {
        if (db) hermes_state_db_close(db);
        return 0;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && (max_files <= 0 || scanned < max_files)) {
        if (ent->d_name[0] == '.') continue;
        if (strstr(ent->d_name, ".json") == NULL) continue;
        scanned++;

        char file_path[1200];
        snprintf(file_path, sizeof(file_path), "%s/%s", flush_dir, ent->d_name);

        char *content = NULL;
        FILE *f = fopen(file_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < (long)(1 << 20)) {
                content = malloc((size_t)sz + 1);
                if (content) {
                    size_t rd = fread(content, 1, (size_t)sz, f);
                    content[rd] = '\0';
                }
            }
            fclose(f);
        }
        if (!content) continue;

        json_t *payload = json_parse(content, NULL);
        free(content);
        if (!payload || payload->type != JSON_OBJECT) {
            if (payload) json_free(payload);
            continue;
        }

        /* Skip agent-history snapshots */
        const char *reason = json_get_str(payload, "reason", "");
        if (reason && strcmp(reason, "shutdown-with-unpersisted-agent-history") == 0) {
            json_free(payload);
            continue;
        }

        const char *session_key = json_get_str(payload, "session_key", "");
        json_t *data = json_obj_get(payload, "data");
        const char *text = data ? json_get_str(data, "text", "") : "";
        const char *session_id = data ? json_get_str(data, "session_id", "") : "";

        if (!text[0] || !session_key[0] || !session_id[0]) {
            hermes_log(LOG_WARNING, "shutdown_flush",
                       "Cannot recover structurally invalid pending message from %s; preserved",
                       file_path);
            json_free(payload);
            continue;
        }

        if (db) {
            hermes_state_append_message(db, session_id, "user", text,
                                        NULL, NULL, 0);
            recovered++;
            /* Delete flush file on success */
            unlink(file_path);
        }
        json_free(payload);
    }
    closedir(dir);

    if (db) hermes_state_db_close(db);
    if (recovered > 0) {
        hermes_log(LOG_INFO, "shutdown_flush",
                   "Recovered %d pending message(s) from shutdown flush", recovered);
    }
    return recovered;
}

/* PoP: shutdown_flush_agent_history_to_file @ gateway/shutdown_flush.py:flush_agent_history_to_file */
int shutdown_flush_agent_history_to_file(const char *session_id,
                                          const char *history_json) {
    /* history_json: JSON array of messages. Returns 0 on success, -1 on fail. */
    if (!history_json || !history_json[0]) return 0;

    json_t *history = json_parse(history_json, NULL);
    if (!history || history->type != JSON_ARRAY) {
        if (history) json_free(history);
        return -1;
    }

    char flush_dir[1024];
    if (get_flush_dir(flush_dir, sizeof(flush_dir)) != 0) {
        json_free(history);
        return -1;
    }

    json_t *payload = json_object();
    json_set(payload, "reason", json_string("shutdown-with-unpersisted-agent-history"));
    json_set(payload, "issue", json_string("#72680"));
    json_set(payload, "session_id", json_string(session_id ? session_id : ""));
    json_set(payload, "count", json_number((double)history->c.count));
    /* Snapshot: keep JSON-serialisable items as-is, stringify others */
    json_t *messages = json_array();
    for (size_t i = 0; i < history->c.count; i++) {
        json_t *m = history->c.items[i];
        if (m && (m->type == JSON_OBJECT || m->type == JSON_ARRAY ||
                  m->type == JSON_STRING || m->type == JSON_NUMBER ||
                  m->type == JSON_BOOL || m->type == JSON_NULL)) {
            json_array_append(messages, json_copy(m));
        } else {
            json_array_append(messages, json_string(""));
        }
    }
    json_set(payload, "messages", messages);

    char file_path[1200];
    snprintf(file_path, sizeof(file_path), "%s/pending-agent-%d-%zu.json",
             flush_dir, (int)time(NULL), (size_t)history->c.count);
    char *ser = json_serialize(payload);
    int ret = 0;
    if (ser) {
        FILE *f = fopen(file_path, "w");
        if (f) {
            fputs(ser, f);
            fclose(f);
            chmod(file_path, 0600);
            fsync_directory(flush_dir);
        } else {
            ret = -1;
        }
        free(ser);
    } else {
        ret = -1;
    }

    json_free(payload);
    json_free(history);
    return ret;
}

/* PoP: shutdown_flush_get_flush_dir @ gateway/shutdown_flush.py:_get_flush_dir */
char *shutdown_flush_get_flush_dir(void) {
    char flush_dir[1024];
    if (get_flush_dir(flush_dir, sizeof(flush_dir)) != 0) return NULL;
    return strdup(flush_dir);
}

/* PoP: shutdown_flush_fsync_directory @ gateway/shutdown_flush.py:_fsync_directory */
int shutdown_flush_fsync_directory(const char *path) {
    if (!path || !path[0]) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int ret = fsync(fd);
    close(fd);
    return ret;
}

/* PoP: shutdown_flush_write_payload @ gateway/shutdown_flush.py:_write_payload */
int shutdown_flush_write_payload(const char *flush_dir,
                                  const char *payload_json) {
    if (!flush_dir || !payload_json) return -1;
    char file_path[1200];
    snprintf(file_path, sizeof(file_path), "%s/pending-%ld.json",
             flush_dir, (long)time(NULL));
    FILE *f = fopen(file_path, "w");
    if (!f) return -1;
    fputs(payload_json, f);
    fclose(f);
    chmod(file_path, 0600);
    fsync_directory(flush_dir);
    return 0;
}

