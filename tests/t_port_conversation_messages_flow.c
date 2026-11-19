/* t_port_conversation_messages_flow.c — harness for the transcript-contract
 * FLOW port (src/persistence/conversation_messages_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state", stamp_live's messages). The
 * echoed "in" is the ORIGINAL input (the oracle re-drives the live reference
 * from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/conversation_messages_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*cmf_fn_t)(cm_ctx_t *, const json_t *);
typedef struct { const char *func; cmf_fn_t fn; } cmf_case_t;

static const cmf_case_t CASES[] = {
    { "_live_or_column", cm_live_or_column },
    { "_count", cm_count },
    { "_json_text", cm_json_text },
    { "_same", cm_same },
    { "NewMessage.__post_init__", cm_new_message_post_init },
    { "NewMessage.from_live", cm_new_message_from_live },
    { "RowOutcome.as_row_annotation", cm_row_outcome_as_row_annotation },
    { "BatchAppendResult.inserted", cm_batch_append_result_inserted },
    { "batch_progress", cm_batch_progress },
    { "HeldMessage.__post_init__", cm_held_message_post_init },
    { "HeldCoverage.__post_init__", cm_held_coverage_post_init },
    { "ReplaceResult.survivor_ids", cm_replace_result_survivor_ids },
    { "stamp_live", cm_stamp_live },
    { "CompactionResult.row_ids", cm_compaction_result_row_ids },
    { "CompactionResult.row_snapshots", cm_compaction_result_row_snapshots },
    { "CooldownSnapshot.__post_init__", cm_cooldown_snapshot_post_init },
    { "ConversationRevision.as_tuple", cm_conversation_revision_as_tuple },
    { "ResumeTooLarge.__init__", cm_resume_too_large_init },
    { "DelegationDelivery.__post_init__", cm_delegation_delivery_post_init },
    { "DelegationDelivery.identity", cm_delegation_delivery_identity },
    { "ConversationMessage.as_openai_message", cm_conversation_message_as_openai_message },
    { "ConversationMessage.from_openai_message", cm_conversation_message_from_openai_message },
    { "TranscriptRead.__post_init__", cm_transcript_read_post_init },
    { "TranscriptRow.as_wire_row", cm_transcript_row_as_wire_row },
    { "ReadCheck.__init__", cm_read_check_init },
    { "ReadCheck.__call__", cm_read_check_call },
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

        const cmf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            cm_ctx_t ctx;
            ctx.state = json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
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