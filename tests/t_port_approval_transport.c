/*
 * t_port_approval_transport.c — parity harness for
 * src/hermes_cli/approval_transport.c (hermes_cli/approval_transport.py).
 *
 * Fixture ops:
 *  {"kind":"create", "command","description","pattern_key","pattern_keys":[],
 *   "session_key","surface","allow_session":b,"allow_permanent":b,
 *   "timeout_seconds":n}
 *    -> {"allowed_choices":[...], "digest_matches":bool, "request_id_len":32}
 *  {"kind":"respond", <create fields>, "choice":"once"}
 *    -> {"request_id_matches":true,"digest_matches":true,"choice":...}
 *  {"kind":"invoke", <create fields>, "present":"ok_once"|"ok_deny"|
 *   "wrong_type"|"raise"|"stale_id"|"stale_digest"|"bad_choice"|"sleep",
 *   "timeout_seconds":n, "poll_interval":n, "polls_before_interrupt":k}
 *    -> {"choice":...,"failure":..., "active_workers_after":0}
 * Mirrors tests/sta_oracle_approval_transport.py.
 */
#include "hermes_json.h"
#include "approval_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

typedef struct {
    const char *mode;
    const approval_request_t *req;   /* for stale-id forgery */
    int polls_left_before_interrupt;
} present_ctx_t;

static int fake_present(void *vctx, const approval_request_t *req,
                        approval_decision_t **out) {
    present_ctx_t *pc = (present_ctx_t *)vctx;
    if (strcmp(pc->mode, "sleep") == 0) { sleep(2); return 0; *out = NULL; }
    if (strcmp(pc->mode, "raise") == 0) return 2;
    if (strcmp(pc->mode, "wrong_type") == 0) return 1;
    const char *choice =
        strcmp(pc->mode, "ok_deny") == 0 ? "deny" :
        strcmp(pc->mode, "bad_choice") == 0 ? "maybe" : "once";
    approval_decision_t *d;
    if (strcmp(pc->mode, "stale_id") == 0) {
        d = approval_request_respond(req, choice);
        free(d->request_id);
        d->request_id = strdup("00000000000000000000000000000000");
    } else if (strcmp(pc->mode, "stale_digest") == 0) {
        d = approval_request_respond(req, choice);
        free(d->request_digest);
        d->request_digest = strdup("deadbeef");
    } else {
        d = approval_request_respond(req, choice);
    }
    *out = d;
    return 0;
}

static void on_poll_count(void *ctx) {
    present_ctx_t *pc = (present_ctx_t *)ctx;
    if (pc->polls_left_before_interrupt > 0) pc->polls_left_before_interrupt--;
}
static int is_interrupted_after_polls(void *ctx) {
    present_ctx_t *pc = (present_ctx_t *)ctx;
    return pc->polls_left_before_interrupt == 0 &&
           pc->polls_left_before_interrupt >= 0 && strcmp(pc->mode, "interrupt") == 0;
}

/* build the request from fixture op fields */
static approval_request_t *mk_req(json_t *c) {
    json_t *pk = json_object_get(c, "pattern_keys");
    const char *pks[16]; size_t n_pk = 0;
    if (pk && pk->type == JSON_ARRAY)
        for (; n_pk < json_len(pk) && n_pk < 16; n_pk++)
            pks[n_pk] = json_get_str(json_get(pk, n_pk), "", "");
    double to = json_get_num(c, "timeout_seconds", 300.0);
    return approval_request_create(
        json_get_str(c, "command", ""), json_get_str(c, "description", ""),
        json_get_str(c, "pattern_key", ""), pks, n_pk,
        json_get_str(c, "session_key", ""), json_get_str(c, "surface", ""),
        json_get_bool(c, "allow_session", 0) ? 1 : 0,
        json_get_bool(c, "allow_permanent", 0) ? 1 : 0, to);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *rootj = json_parse(raw, NULL);
    if (!rootj) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(rootj, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "create") == 0 || strcmp(kind, "respond") == 0 ||
                strcmp(kind, "invoke") == 0) {
                approval_request_t *req = mk_req(c);

                /* self-check: recompute the digest over the canonical form of
                 * the ACTUAL generated request_id and compare */
                int dm = approval_request_verify_digest(req);

                if (strcmp(kind, "create") == 0) {
                    json_t *arr = json_new_array();
                    for (size_t k = 0; k < req->n_allowed_choices; k++)
                        json_array_append(arr, json_string(req->allowed_choices[k]));
                    json_object_set(o, "allowed_choices", arr);
                    json_object_set(o, "digest_matches", json_bool(dm));
                    json_object_set(o, "request_id_len",
                                    json_int((double)strlen(req->request_id)));
                } else if (strcmp(kind, "respond") == 0) {
                    approval_decision_t *d = approval_request_respond(
                        req, json_get_str(c, "choice", ""));
                    json_object_set(o, "request_id_matches",
                        json_bool(strcmp(d->request_id, req->request_id) == 0));
                    json_object_set(o, "digest_matches",
                        json_bool(strcmp(d->request_digest, req->digest) == 0));
                    json_object_set(o, "choice", json_string(d->choice));
                    approval_decision_free(d);
                } else {
                    static present_ctx_t pc;
                    memset(&pc, 0, sizeof pc);
                    pc.mode = json_get_str(c, "present", "ok_once");
                    pc.polls_left_before_interrupt =
                        (int)json_get_num(c, "polls_before_interrupt", -1);
                    approval_transport_result_t r = invoke_approval_transport(
                        fake_present, &pc, req,
                        json_get_num(c, "timeout_seconds", 5.0),
                        json_get_num(c, "poll_interval", 0.02),
                        on_poll_count, &pc,
                        strcmp(pc.mode, "interrupt") == 0 ? is_interrupted_after_polls : NULL,
                        &pc);
                    /* give an abandoned sleeper a moment so the detached
                     * worker's slot release is observed deterministically */
                    if (strcmp(pc.mode, "sleep") == 0) sleep(2);
                    json_object_set(o, "choice", json_string(r.choice));
                    json_object_set(o, "failure", json_string(r.failure));
                    json_object_set(o, "active_workers_after",
                                    json_int(approval_transport_active_workers()));
                }

                json_object_set(o, "self_digest_ok", json_bool(dm));
                approval_request_free(req);
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(rootj);
    free(raw);
    return 0;
}
