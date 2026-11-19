/* t_port_state_errors.c — oracle harness for persistence/sqlite/_state/errors.py
 * pure predicates + message formatting. Emits one JSON line per case. */
#include "port_state_errors.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void exc_in(const char *kind, const char *msg, int has_code, int code)
{
    printf("{\"kind\":\"%s\",\"message\":", kind);
    jstr_escape(msg ? msg : "");
    printf(",\"code\":");
    if (has_code)
        printf("%d", code);
    else
        printf("null");
    putchar('}');
}

static void pred_case(int cs, const char *fn, bool (*f)(const se_exc_t *),
                      se_kind_t kind, const char *kind_name, const char *msg,
                      int has_code, int code)
{
    se_exc_t e = { kind, msg, has_code, code };
    bool out = f(&e);
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":", cs, fn);
    exc_in(kind_name, msg, has_code, code);
    printf(",\"out\":%s}\n", out ? "true" : "false");
    fflush(stdout);
}

static void classify_case(int cs, se_kind_t kind, const char *kind_name,
                          const char *msg, int has_code, int code)
{
    se_exc_t e = { kind, msg, has_code, code };
    const char *out = se_classify_persistence_error(&e);
    printf("{\"case\":%d,\"fn\":\"classify_persistence_error\",\"in\":", cs);
    exc_in(kind_name, msg, has_code, code);
    printf(",\"out\":{\"str\":");
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

static void closed_msg_case(int cs, const char *in_json, const char *session_id)
{
    char *out = se_compression_session_closed_message(session_id);
    printf("{\"case\":%d,\"fn\":\"compression_session_closed_message\",\"in\":%s,"
           "\"out\":{\"str\":", cs, in_json);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
    free(out);
}

static void const_msg_case(int cs, const char *fn, char *(*f)(void))
{
    char *out = f();
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":{},\"out\":{\"str\":", cs, fn);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
    free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _is_no_more_rows: message-scoped, case-insensitive, no type gate. */
    pred_case(1, "_is_no_more_rows", se_is_no_more_rows, SE_OPERATIONAL,
              "operational", "No more rows available.", 0, 0);
    pred_case(2, "_is_no_more_rows", se_is_no_more_rows, SE_OPERATIONAL,
              "operational", "database is locked", 0, 0);
    pred_case(3, "_is_no_more_rows", se_is_no_more_rows, SE_DATABASE,
              "database", "NO MORE ROWS AVAILABLE", 0, 0);
    pred_case(4, "_is_no_more_rows", se_is_no_more_rows, SE_OTHER_EXC,
              "other", "no more rows available", 0, 0);

    /* is_transient_sqlite_error: OperationalError gate; code decides the
     * lock half, markers still apply to the text. */
    pred_case(5, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_OPERATIONAL, "operational", "database is locked", 0, 0);
    pred_case(6, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_OPERATIONAL, "operational", "disk i/o error", 1, 10);
    pred_case(7, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_OPERATIONAL, "operational",
              "vtable constructor failed: messages_fts", 1, 5);
    pred_case(8, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_OPERATIONAL, "operational",
              "database disk image is malformed", 0, 0);
    pred_case(9, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_DATABASE, "database", "database is locked", 0, 0);
    pred_case(10, "is_transient_sqlite_error", se_is_transient_sqlite_error,
              SE_OPERATIONAL, "operational", "something odd", 1, 517);

    /* is_malformed_schema_error: DatabaseError family + explicit schema text. */
    pred_case(11, "is_malformed_schema_error", se_is_malformed_schema_error,
              SE_DATABASE, "database",
              "malformed database schema (messages_fts)", 0, 0);
    pred_case(12, "is_malformed_schema_error", se_is_malformed_schema_error,
              SE_OPERATIONAL, "operational", "Malformed Database Schema", 0, 0);
    pred_case(13, "is_malformed_schema_error", se_is_malformed_schema_error,
              SE_DATABASE, "database",
              "database disk image is malformed", 0, 0);
    pred_case(14, "is_malformed_schema_error", se_is_malformed_schema_error,
              SE_OTHER_EXC, "other", "malformed database schema", 0, 0);
    pred_case(15, "is_malformed_schema_error", se_is_malformed_schema_error,
              SE_CORRUPT, "corrupt", "malformed database schema", 0, 0);

    /* classify_persistence_error: backend types first (order matters), then
     * the shared storage classifier (provenance before prose). */
    classify_case(16, SE_NONE, "none", NULL, 0, 0);
    classify_case(17, SE_COMPRESSION_CLOSED, "compression_closed", "s1", 0, 0);
    classify_case(18, SE_COMPRESSION_IN_PROGRESS, "compression_in_progress",
                  "x", 0, 0);
    classify_case(19, SE_COMPRESSION_BUSY, "compression_busy", "x", 0, 0);
    classify_case(20, SE_DELETED_WAL, "deleted_wal", "x", 0, 0);
    classify_case(21, SE_REPLACED, "replaced", "x", 0, 0);
    classify_case(22, SE_CORRUPT, "corrupt", "x", 0, 0);
    classify_case(23, SE_TURN_LEASE_LOST, "turn_lease", "x", 0, 0);
    classify_case(24, SE_STRING, "string", "database is locked", 0, 0);
    classify_case(25, SE_STRING, "string",
                  "fts5: corrupt database disk image (messages_fts_data)", 0, 0);
    classify_case(26, SE_STRING, "string", "xmessages_fts malformed", 0, 0);
    classify_case(27, SE_STRING, "string",
                  "state.db was replaced underneath the gateway", 0, 0);
    classify_case(28, SE_STRING, "string", "database or disk is full", 0, 0);
    classify_case(29, SE_STRING, "string", "something else", 0, 0);
    classify_case(30, SE_DATABASE, "database",
                  "FOREIGN KEY constraint failed", 1, 787);
    classify_case(31, SE_DATABASE, "database",
                  "database disk image is malformed", 1, 267);
    classify_case(32, SE_DATABASE, "database",
                  "fts5: corrupt messages_fts_data", 1, 11);
    classify_case(33, SE_STRING, "string",
                  "Session 's1' is closed by compression; adopt its live "
                  "continuation before appending messages", 0, 0);

    /* CompressionSessionClosedError.__init__ message (Python repr edges). */
    closed_msg_case(34, "{\"session_id\":\"s1\"}", "s1");
    closed_msg_case(35, "{\"session_id\":\"it's\"}", "it's");
    closed_msg_case(36, "{\"session_id\":\"both'and\\\"\"}", "both'and\"");
    closed_msg_case(37, "{\"session_id\":\"a\\u0001b\\u007fc\"}", "a\x01" "b\x7f" "c");
    closed_msg_case(38, "{\"session_id\":\"café\"}", "café");
    closed_msg_case(39, "{\"session_id\":null}", NULL);

    /* The three FATAL operator messages (exact module constants). */
    const_msg_case(40, "state_db_replaced_message", se_state_db_replaced_message);
    const_msg_case(41, "deleted_wal_generation_message",
                   se_deleted_wal_generation_message);
    const_msg_case(42, "state_db_corrupt_message", se_state_db_corrupt_message);
    return 0;
}
