/*
 * t_port_hermes_state_db.c — DB-backed oracle harness for the model_config
 * read/patch methods of hermes_state.py, ported in hermes_state_lifecycle.c.
 *
 * Opens a seeded sqlite DB (argv[1], built by make_seed.py with the real
 * Hermes schema) and runs deterministic model_config ops, emitting one JSON
 * object per op. The Python oracle (sta_oracle_hermes_state_db.py) opens the
 * SAME DB and replays the SAME ops via SessionDB, then the runner exact-
 * compares. The C harness runs first and its mutations are visible to the
 * oracle, so both sides must run the identical op sequence in the identical
 * order.
 *
 * Tested C surface (include/hermes_state_db.h):
 *   get_session_model_config_value, patch_session_model_config
 *
 * Note: get_session / create_session / end_session are intentionally NOT here
 * — the C get_session returns a 15-column subset vs Python's full row dict
 * (not byte-comparable), and create/end write live now_epoch() timestamps
 * (nondeterministic across C and Python).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_state_db.h"
#include "hermes_json.h"
#include "libjson/json.h"

/* Serialize a value to a JSON string for embedding; null-safe. */
static void emit_value(const char *op, const char *extra_key, const char *extra_val) {
    printf("{\"op\":\"%s\",\"out\":%s", op, extra_val ? extra_val : "null");
    if (extra_key) printf(",\"%s\":true", extra_key);
    printf("}\n");
}

/* Print a value returned by get_session_model_config_value as a JSON literal.
 * Stored values come back JSON-serialized (e.g. "\"gpt-4o\"" or an object);
 * default strings come back raw (e.g. "MISSING"). Normalize: if it parses as
 * valid JSON, emit as-is; otherwise wrap as a JSON string. */
static void emit_mc_value(const char *v) {
    if (!v) { fputs("null", stdout); return; }
    json_t *parsed = json_parse(v, NULL);
    if (parsed) {
        char *ser = json_serialize(parsed);
        fputs(ser ? ser : "null", stdout);
        free(ser);
        json_free(parsed);
    } else {
        char *ser = json_serialize(json_string(v));
        fputs(ser ? ser : "null", stdout);
        free(ser);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <seed.db>\n", argv[0]); return 2; }
    const char *dbpath = argv[1];
    hermes_state_db_t *db = hermes_state_db_open(dbpath);
    if (!db) { fprintf(stderr, "cannot open %s\n", dbpath); return 2; }

    /* Read helpers: get_session_model_config_value(session, key, default). */

    /* 1. existing scalar key. */
    {
        char *v = hermes_state_get_session_model_config_value(db, "sess_root", "model", "null");
        printf("{\"op\":\"read_model\",\"out\":%s}\n", v ? v : "null");
        free(v);
    }
    /* 2. nested object key (browser_model_lock) — serialized JSON. */
    {
        char *v = hermes_state_get_session_model_config_value(db, "sess_root", "browser_model_lock", "null");
        printf("{\"op\":\"read_lock\",\"out\":%s}\n", v ? v : "null");
        free(v);
    }
    /* 3. missing key on existing session -> default. */
    {
        char *v = hermes_state_get_session_model_config_value(db, "sess_root", "absent", "42");
        printf("{\"op\":\"read_absent\",\"out\":%s}\n", v ? v : "null");
        free(v);
    }
    /* 4. empty-config session -> default. */
    {
        char *v = hermes_state_get_session_model_config_value(db, "sess_empty", "model", "null");
        printf("{\"op\":\"read_empty\",\"out\":%s}\n", v ? v : "null");
        free(v);
    }
    /* 5. nonexistent session -> default. */
    {
        char *v = hermes_state_get_session_model_config_value(db, "ghost", "model", "null");
        printf("{\"op\":\"read_ghost\",\"out\":%s}\n", v ? v : "null");
        free(v);
    }

    /* Patch: patch_session_model_config(session, patch_json). */

    /* 6. merge new top-level key + overwrite existing. */
    {
        bool ok = hermes_state_patch_session_model_config(db, "sess_root",
            "{\"provider\":\"anthropic\",\"newkey\":\"nv\"}");
        char *v = hermes_state_get_session_model_config_value(db, "sess_root", "provider", "null");
        printf("{\"op\":\"patch_merge\",\"ok\":%s,\"provider\":%s}\n",
               ok ? "true" : "false", v ? v : "null");
        free(v);
    }
    /* 7. patch with a None value removes the key (Python semantics). */
    {
        bool ok = hermes_state_patch_session_model_config(db, "sess_root", "{\"keep\":null}");
        char *v = hermes_state_get_session_model_config_value(db, "sess_root", "keep", "MISSING");
        printf("{\"op\":\"patch_none\",\"ok\":%s,\"keep\":", ok ? "true" : "false");
        emit_mc_value(v);
        printf("}\n");
        free(v);
    }
    /* 8. patch on nonexistent session is a no-op. */
    {
        bool ok = hermes_state_patch_session_model_config(db, "ghost", "{\"x\":1}");
        printf("{\"op\":\"patch_missing\",\"ok\":%s}\n", ok ? "true" : "false");
    }
    /* 9. patch with empty dict {} is a no-op. */
    {
        bool ok = hermes_state_patch_session_model_config(db, "sess_root", "{}");
        printf("{\"op\":\"patch_empty\",\"ok\":%s}\n", ok ? "true" : "false");
    }

    hermes_state_db_close(db);
    return 0;
}
