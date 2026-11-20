/* t_port_executions_worker_flow2.c — harness for the worker-execution-repository
 * FLOW2 port (src/persistence/executions_worker_flow2.c).
 *
 * Reads JSON LINES: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace"}}.
 * "out" captures only what the C decision core did: the ordered seam trace
 * ({"name","args","ret"} events in call order) and the final return. The echoed
 * "in" is the ORIGINAL input so the oracle can re-drive the live reference.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (a plain value on
 * every call, a LIST consumed one-per-call, or {"__raise__":cls,"message":m}).
 * A missing/exhausted script fail-closes with RuntimeError; no real I/O.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/executions_worker_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ewf2_fn_t)(ewf2_ctx_t *, const json_t *);
typedef struct { const char *func; ewf2_fn_t fn; } ewf2_case_t;

static const ewf2_case_t CASES[] = {
    { "renew_turn_lease", ewf2_renew_turn_lease },
    { "touch_session_activity", ewf2_touch_session_activity },
    { "clear_session_activity", ewf2_clear_session_activity },
    { "end_session", ewf2_end_session },
    { "set_session_system_prompt", ewf2_set_session_system_prompt },
    { "latest_conversation_role", ewf2_latest_conversation_role },
    { "message_role", ewf2_message_role },
    { "acquire_compression_lock", ewf2_acquire_compression_lock },
    { "renew_compression_lock", ewf2_renew_compression_lock },
    { "compression_lock_holder", ewf2_compression_lock_holder },
    { "cleanup_compression_lock", ewf2_cleanup_compression_lock },
    { "record_compression_cooldown", ewf2_record_compression_cooldown },
    { "restore_compression_cooldown", ewf2_restore_compression_cooldown },
    { "clear_compression_cooldown", ewf2_clear_compression_cooldown },
    { "set_compression_fallback_streak", ewf2_set_compression_fallback_streak },
    { "set_compression_ineffective_count", ewf2_set_compression_ineffective_count },
    { "set_compression_recovery_deadline", ewf2_set_compression_recovery_deadline },
    { "reopen_orphaned_compression", ewf2_reopen_orphaned_compression },
    { "compression_session_context", ewf2_compression_session_context },
    { "compression_lineage_view", ewf2_compression_lineage_view },
    { "compression_history", ewf2_compression_history },
    { "dispatch_delegation", ewf2_dispatch_delegation },
    { "complete_delegation", ewf2_complete_delegation },
    { "record_delegation_child", ewf2_record_delegation_child },
    { "read_delegation", ewf2_read_delegation },
    { "withdraw_delegation", ewf2_withdraw_delegation },
    { "claim_delegation_delivery", ewf2_claim_delegation_delivery },
    { "ack_delegation_delivery", ewf2_ack_delegation_delivery },
    { "drop_delegation_delivery", ewf2_drop_delegation_delivery },
    { "defer_delegation_delivery", ewf2_defer_delegation_delivery },
    { "release_delegation_delivery", ewf2_release_delegation_delivery },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const ewf2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            ewf2_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.trace = json_array();
            ctx.cursors = json_object();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
