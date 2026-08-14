/* hermes_state_lifecycle.c — session lifecycle methods of the SessionDB port:
 * create_session, end_session, set_session_archived, get_session.
 * Faithful SQL mirrors of hermes_state.py. Self-contained; depends only on
 * hermes_state_internal.h.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include "hermes_json.h"   /* json_t parse/patch/serialize */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: create_session @ hermes_state.py:create_session */
bool hermes_state_create_session(hermes_state_db_t *db, const char *session_id,
                                 const char *source) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, started_at) "
            "VALUES (?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source && *source ? source : "unknown", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, hermes_state_now_epoch());
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: end_session @ hermes_state.py:end_session */
/* PoP: hermes_state_end_session @ hermes_state.py:end_session */
bool hermes_state_end_session(hermes_state_db_t *db, const char *session_id,
                              const char *end_reason) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = ?, end_reason = ? "
            "WHERE id = ? AND ended_at IS NULL", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_double(st, 1, hermes_state_now_epoch());
    sqlite3_bind_text(st, 2, end_reason && *end_reason ? end_reason : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* set_session_archived lives in hermes_state_archive.c — the faithful
 * version flips the WHOLE compression lineage via a recursive CTE. */

/* PoP: (model_config setter) @ hermes_state.py compression-walk exclusion */
bool hermes_state_set_model_config(hermes_state_db_t *db, const char *session_id,
                                   const char *model_config_json) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET model_config = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, model_config_json ? model_config_json : "{}", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* ── model_config read/patch (proactive-prune-rearm runway + micro sync) ─ */

/* PoP: get_session_model_config_value @ hermes_state.py:get_session_model_config_value */
char *hermes_state_get_session_model_config_value(hermes_state_db_t *db,
                                                  const char *session_id,
                                                  const char *key,
                                                  const char *default_json) {
    if (!db || !session_id || !*session_id || !key || !*key) {
        return default_json ? strdup(default_json) : strdup("null");
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT model_config FROM sessions WHERE id = ?", -1, &st, NULL)
        != SQLITE_OK)
        return default_json ? strdup(default_json) : strdup("null");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *result = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *mc = sqlite3_column_text(st, 0);
        const char *src = mc ? (const char *)mc : "{}";
        json_t *obj = json_parse(src, NULL);
        if (obj && obj->type == JSON_OBJECT) {
            json_t *val = json_obj_get(obj, key);
            if (val) {
                result = json_serialize(val);
                if (!result) result = strdup("null");
            }
        }
        if (!result)
            result = default_json ? strdup(default_json) : strdup("null");
        json_free(obj);
    } else {
        result = default_json ? strdup(default_json) : strdup("null");
    }
    sqlite3_finalize(st);
    return result;
}

/* PoP: patch_session_model_config @ hermes_state.py:patch_session_model_config */
bool hermes_state_patch_session_model_config(hermes_state_db_t *db,
                                             const char *session_id,
                                             const char *patch_json) {
    if (!db || !session_id || !*session_id) return false;

    /* Read current model_config, merge patch keys on top, write back. */
    sqlite3_stmt *gst = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT model_config FROM sessions WHERE id = ?", -1, &gst, NULL)
        != SQLITE_OK) return false;
    sqlite3_bind_text(gst, 1, session_id, -1, SQLITE_TRANSIENT);

    json_t *merged = json_object();
    if (sqlite3_step(gst) == SQLITE_ROW) {
        const unsigned char *mc = sqlite3_column_text(gst, 0);
        json_t *cur = json_parse(mc ? (const char*)mc : "{}", NULL);
        if (cur && cur->type == JSON_OBJECT) {
            /* Deep-copy existing entries into merged. */
            for (size_t i = 0; i < cur->c.count; i++)
                json_set(merged, cur->c.keys[i], json_copy(cur->c.items[i]));
        }
        json_free(cur);
    }
    sqlite3_finalize(gst);
    if (!merged) return false;

    /* Apply patch: top-level keys overwrite existing (matches Python update).
     * A JSON null patch value REMOVES that key (matches _merge_model_config_json
     * `config.pop(key, None)` for None values). */
    if (patch_json && *patch_json) {
        json_t *patch = json_parse(patch_json, NULL);
        if (patch && patch->type == JSON_OBJECT) {
            for (size_t i = 0; i < patch->c.count; i++) {
                json_t *pv = patch->c.items[i];
                if (pv && pv->type == JSON_NULL)
                    json_obj_del(merged, patch->c.keys[i]);
                else
                    json_set(merged, patch->c.keys[i], json_copy(pv));
            }
        }
        json_free(patch);
    }

    char *out = json_serialize(merged);
    json_free(merged);
    if (!out) return false;
    bool written = hermes_state_set_model_config(db, session_id, out);
    free(out);
    return written;
}
bool hermes_state_link_child(hermes_state_db_t *db, const char *child_id,
                             const char *parent_id, const char *end_reason) {
    if (!db || !child_id || !*child_id || !parent_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET parent_session_id = ?, end_reason = ?, "
            "ended_at = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, parent_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, end_reason && *end_reason ? end_reason : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, hermes_state_now_epoch());
    sqlite3_bind_text(st, 4, child_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: get_session @ hermes_state.py:get_session */
char *hermes_state_get_session(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id || !*session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, source, parent_session_id, title, archived, pinned, "
            "started_at, ended_at, end_reason, message_count, model, "
            "input_tokens, output_tokens, reasoning_tokens, estimated_cost_usd "
            "FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        const unsigned char *par = sqlite3_column_text(st, 2);
        const unsigned char *title = sqlite3_column_text(st, 3);
        int arch = sqlite3_column_int(st, 4);
        int pinned = sqlite3_column_int(st, 5);
        double started = sqlite3_column_double(st, 6);
        int ended_null = sqlite3_column_type(st, 7) == SQLITE_NULL;
        const unsigned char *ereason = sqlite3_column_text(st, 8);
        int mc = sqlite3_column_int(st, 9);
        const unsigned char *model = sqlite3_column_text(st, 10);
        long long in = sqlite3_column_int64(st, 11);
        long long out_tok = sqlite3_column_int64(st, 12);
        long long rzn = sqlite3_column_int64(st, 13);
        double cost = sqlite3_column_double(st, 14);
        /* quote-or-null helpers for nullable text columns */
        char parq[512], titleq[1024], ereasonq[256], modelq[512];
        if (par)     snprintf(parq, sizeof parq, "\"%s\"", (const char*)par);     else snprintf(parq, sizeof parq, "null");
        if (title)   snprintf(titleq, sizeof titleq, "\"%s\"", (const char*)title); else snprintf(titleq, sizeof titleq, "null");
        if (ereason) snprintf(ereasonq, sizeof ereasonq, "\"%s\"", (const char*)ereason); else snprintf(ereasonq, sizeof ereasonq, "null");
        if (model)   snprintf(modelq, sizeof modelq, "\"%s\"", (const char*)model); else snprintf(modelq, sizeof modelq, "null");
        int n = snprintf(NULL, 0,
            "{\"id\":\"%s\",\"source\":\"%s\",\"parent_session_id\":%s,"
            "\"title\":%s,\"archived\":%d,\"pinned\":%d,\"started_at\":%.6f,"
            "\"ended_at\":%s,\"end_reason\":%s,\"message_count\":%d,"
            "\"model\":%s,\"input_tokens\":%lld,\"output_tokens\":%lld,"
            "\"reasoning_tokens\":%lld,\"estimated_cost_usd\":%.6f}",
            id ? (const char*)id : "",
            src ? (const char*)src : "",
            parq,
            titleq,
            arch, pinned, started,
            ended_null ? "null" : "0.0",
            ereasonq,
            mc,
            modelq,
            in, out_tok, rzn, cost);
        out = malloc((size_t)n + 1);
        snprintf(out, (size_t)n + 1,
            "{\"id\":\"%s\",\"source\":\"%s\",\"parent_session_id\":%s,"
            "\"title\":%s,\"archived\":%d,\"pinned\":%d,\"started_at\":%.6f,"
            "\"ended_at\":%s,\"end_reason\":%s,\"message_count\":%d,"
            "\"model\":%s,\"input_tokens\":%lld,\"output_tokens\":%lld,"
            "\"reasoning_tokens\":%lld,\"estimated_cost_usd\":%.6f}",
            id ? (const char*)id : "",
            src ? (const char*)src : "",
            parq,
            titleq,
            arch, pinned, started,
            ended_null ? "null" : "0.0",
            ereasonq,
            mc,
            modelq,
            in, out_tok, rzn, cost);
    }
    sqlite3_finalize(st);
    return out;
}
