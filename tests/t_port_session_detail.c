/*
 * t_port_session_detail.c — oracle harness for the web_server session-detail
 * stack (port_web_server_session_detail.c). Operates on a seeded sqlite DB
 * (seed.db) and emits one JSON object per op against the shared fixture so the
 * Python oracle (sta_oracle_session_detail.py) can exact-compare.
 *
 * Usage: t_port_session_detail <seed.db>
 * Ops are hardcoded here (no fixture stdin) because they all target the one
 * shared DB path passed as argv[1].
 */

#include "web_server_session_detail.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* emit a result object: {"op":..., "out": <raw json string or null>} */
static void emit_raw(const char *op, const char *raw) {
    printf("{\"op\":\"%s\",\"out\":%s}\n", op, raw ? raw : "null");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <seed.db>\n", argv[0]); return 2; }
    const char *db = argv[1];

    /* 1. sanitize_title: control chars stripped, collapse, too-long reject. */
    {
        char *err = NULL;
        char *r = ws_sess_sanitize_title("  Hello\x01World\nFoo\tBar  ", &err);
        char *d = json_dumps(json_string(r ? r : ""), 0);
        char *ed = json_dumps(err ? json_string(err) : json_null(), 0);
        printf("{\"op\":\"sanitize\",\"out\":%s,\"err\":%s}\n", d ? d : "null", ed ? ed : "null");
        free(r); free(err); free(d); free(ed);
    }
    {
        char *err = NULL;
        char big[256];
        for (int i = 0; i < 120; i++) big[i] = 'x';
        big[120] = '\0';
        char *r = ws_sess_sanitize_title(big, &err);
        char *ed = json_dumps(err ? json_string(err) : json_null(), 0);
        printf("{\"op\":\"sanitize_too_long\",\"out\":%s,\"err\":%s}\n",
               r ? "\"nonnull\"" : "null", ed ? ed : "null");
        free(r); free(err); free(ed);
    }

    /* 2. get_session (exact id). */
    {
        json_t *s = ws_sess_get_session(db, "sessA");
        char *d = s ? json_dumps(s, 0) : NULL;
        emit_raw("get_session", d);
        free(d); if (s) json_free(s);
    }
    {
        json_t *s = ws_sess_get_session(db, "nope");
        emit_raw("get_session_missing", s ? json_dumps(s, 0) : NULL);
        if (s) json_free(s);
    }

    /* 3. get_messages (active only, no limit). */
    {
        json_t *m = ws_sess_get_messages(db, "sessA", false, false, 0, 0);
        char *d = m ? json_dumps(m, 0) : NULL;
        emit_raw("get_messages", d);
        free(d); if (m) json_free(m);
    }

    /* 4. compression_tip (sessA -> sessA_child). */
    {
        char *t = ws_sess_compression_tip(db, "sessA");
        char *d = json_dumps(json_string(t ? t : ""), 0);
        emit_raw("compression_tip", d);
        free(t); free(d);
    }

    /* 5. resolve_resume_id (sessA -> its continuation with messages). */
    {
        char *t = ws_sess_resolve_resume_id(db, "sessA");
        char *d = json_dumps(json_string(t ? t : ""), 0);
        emit_raw("resolve_resume", d);
        free(t); free(d);
    }

    /* 6. set_title + get_title roundtrip. */
    {
        char *err = NULL;
        bool ok = ws_sess_set_title(db, "sessB", "New Beta Title", &err);
        char *got = ws_sess_get_title(db, "sessB");
        char *d = json_dumps(json_bool(ok), 0);
        char *g = json_dumps(got ? json_string(got) : json_null(), 0);
        char *ed = json_dumps(err ? json_string(err) : json_null(), 0);
        printf("{\"op\":\"set_title\",\"out\":%s,\"title\":%s,\"err\":%s}\n", d, g, ed ? ed : "null");
        free(got); free(err); free(d); free(g); free(ed);
    }

    /* 7. set_archived (lineage flip on sessA). */
    {
        bool ok = ws_sess_set_archived(db, "sessA", true);
        char *d = json_dumps(json_bool(ok), 0);
        emit_raw("set_archived", d);
        free(d);
    }

    /* 8. export_session (sessA -> {**session, messages}). */
    {
        json_t *e = ws_sess_export_session(db, "sessA");
        char *d = e ? json_dumps(e, 0) : NULL;
        emit_raw("export", d);
        free(d); if (e) json_free(e);
    }

    /* 9. latest_descendant_endpoint (sessA -> sessA_child). */
    {
        json_t *e = ws_sess_latest_descendant_endpoint(db, "sessA");
        char *d = e ? json_dumps(e, 0) : NULL;
        emit_raw("latest_descendant", d);
        free(d); if (e) json_free(e);
    }

    /* 10. delete_session (sessB then re-query). */
    {
        bool ok = ws_sess_delete_session(db, "sessB");
        json_t *after = ws_sess_get_session(db, "sessB");
        char *d = json_dumps(json_bool(ok), 0);
        char *a = after ? json_dumps(after, 0) : NULL;
        printf("{\"op\":\"delete\",\"out\":%s,\"after\":%s}\n", d, a ? a : "null");
        free(d); free(a); if (after) json_free(after);
    }

    return 0;
}
