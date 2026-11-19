/* t_port_hosted_rooms_flow.c — harness for the gateway/hosted_rooms.py
 * room-lifecycle FLOW port (src/gateway/hosted_rooms_flow.c).
 *
 * Reads JSON LINES from stdin (argv[1] optional): {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace") and the
 * final return / raised exception ("ret"). The echoed "in" is the ORIGINAL
 * input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order (conn.execute scripts are queues of ROWS lists), or
 * {"__script__": true, "ret": <value>, "raise": {"type","message"}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "hosted_rooms_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*hrf_fn_t)(hrf_ctx_t *, const json_t *);
typedef struct { const char *func; hrf_fn_t fn; } hrf_case_t;

static const hrf_case_t CASES[] = {
    { "create_room", hrf_create_room },
    { "list_rooms", hrf_list_rooms },
    { "room_has_peer_obligations", hrf_room_has_peer_obligations },
    { "retire_room_link_records", hrf_retire_room_link_records },
    { "due_room_grant_revokes", hrf_due_room_grant_revokes },
    { "settle_room_grant_revoke", hrf_settle_room_grant_revoke },
    { "drop_remote_run_receipt", hrf_drop_remote_run_receipt },
    { "_typed", hrf_typed },
    { "_grant_claims", hrf_grant_claims },
    { "_reservation_claims", hrf_reservation_claims },
    { "_reservation_grant", hrf_reservation_grant },
    { "default_db_path", hrf_default_db_path },
    { "_room_row", hrf_room_row },
    { "_is_retired", hrf_is_retired },
    { "_require_authority", hrf_require_authority },
    { "_migrate_legacy_columns", hrf_migrate_legacy_columns },
    { "_prepare_event", hrf_prepare_event },
    { "_insert_event", hrf_insert_event },
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

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(f)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const hrf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            hrf_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
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
    if (f != stdin) fclose(f);
    return 0;
}