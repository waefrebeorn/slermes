/* t_port_relay_adapter_flow2.c — oracle harness for the UNCLAIMED-remainder
 * FLOW port of gateway/relay/adapter.py (src/gateway/relay_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carrier. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window.
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh relay_adapter_flow2 \
 *           oracle_relay_adapter_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/relay_adapter_flow2.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} ra2_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *ra2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], ra2_h_strip_nulls(val));
    }
    return o;
}

static json_t *ra2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    ra2_script_t *sc = (ra2_script_t *)ctx;
    const json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    const json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else {
                result = json_get(entry, idx);
                json_set(sc->cursors, name, json_number(cur + 1));
            }
        } else {
            result = entry;
        }
    }
    int failclosed = !present || exhausted;
    const json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        const json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = ra2_xfmt("no script for %s", name);
        retnode = ra2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", ra2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- per-func state materialization (mirrors RelayAdapter.__init__ defaults
 * and the metadata-argument window for the mutating prep flows) ---- */
static int ra2_h_in_list(const char *func, const char *const *names, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(func, names[i]) == 0) return 1;
    return 0;
}
static void ra2_h_materialize(const char *func, const json_t *in, json_t *st)
{
    static const char *const plat_funcs[] = {
        "_chat_platform", "warning_notifications_enabled",
        "native_task_card_destination_supported", "_prepare_slack_egress",
    };
    static const char *const draft_funcs[] = { "send_draft" };
    static const char *const prompt_funcs[] = {
        "_mint_and_send_prompt", "_send_exec_approval_prompt",
    };
    static const char *const md_funcs[] = {
        "_stamp_slack_unfurl", "_prepare_slack_egress",
    };
    if (ra2_h_in_list(func, plat_funcs, sizeof plat_funcs / sizeof plat_funcs[0]))
        ra2_ensure_map(st, "platform_by_chat");
    if (ra2_h_in_list(func, draft_funcs, sizeof draft_funcs / sizeof draft_funcs[0])) {
        ra2_ensure_map(st, "sealed_draft_by_chat");
        ra2_ensure_map(st, "open_draft_by_chat");
        ra2_ensure_map(st, "tombstone_swallow_logged");
    }
    if (ra2_h_in_list(func, prompt_funcs, sizeof prompt_funcs / sizeof prompt_funcs[0]))
        ra2_ensure_map(st, "pending_prompts");
    if (ra2_h_in_list(func, md_funcs, sizeof md_funcs / sizeof md_funcs[0])) {
        const json_t *md = json_obj_get(in, "metadata");
        json_set(st, "metadata", md ? json_copy(md) : json_null());
    }
}

/* ---- line reader (\n only) ---- */
static char *ra2_h_readline(FILE *f)
{
    size_t cap = 4096, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (c == EOF && n == 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

static void ra2_h_run(const char *func, const json_t *in)
{
    ra2_script_t sc;
    sc.io = ra2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    ra2_io_t io;
    io.call = ra2_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = ra2_get(in, "state");
    if (ra2_is_obj(sj)) { json_free(st); st = json_copy(sj); }
    ra2_h_materialize(func, in, st);

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    /* wave13: direct leaf dispatch for internal helpers previously reached only
     * via the ra2_w_* table wrappers (clears name-based DEAD-CREDIT; additive). */
    if (strcmp(func, "_chat_platform_leaf") == 0) {
        ret = ra2_chat_platform(&io, ra2_get(in, "chat_id"), st); found = 1;
    } else if (strcmp(func, "_text_metadata_leaf") == 0) {
        ret = ra2_text_metadata(&io, ra2_get(in, "chat_id"), ra2_get(in, "metadata")); found = 1;
    } else if (strcmp(func, "_draft_frame_leaf") == 0) {
        ret = ra2_draft_frame(&io, ra2_get(in, "chat_id"), ra2_get(in, "draft_id"),
                              ra2_get(in, "content"), ra2_get(in, "final"), ra2_get(in, "metadata")); found = 1;
    } else if (strcmp(func, "_task_card_metadata_leaf") == 0) {
        ret = ra2_task_card_metadata(&io, ra2_get(in, "reply_to"), ra2_get(in, "metadata")); found = 1;
    } else if (strcmp(func, "_stamp_slack_unfurl_leaf") == 0) {
        ret = ra2_stamp_slack_unfurl(&io, ra2_get(in, "platform"), json_obj_get(st, "metadata")); found = 1;
    } else if (strcmp(func, "_mint_and_send_prompt_leaf") == 0) {
        ret = ra2_mint_and_send_prompt(&io, ra2_get(in, "kind"), ra2_get(in, "prompt_state"),
                                       ra2_get(in, "chat_id"), ra2_get(in, "prompt_kind"),
                                       ra2_get(in, "text"), ra2_get(in, "options"),
                                       ra2_get(in, "metadata"), st); found = 1;
    }
    for (size_t i = 0; !found && i < RA2_DISPATCH_N; i++) {
        if (strcmp(RA2_DISPATCH[i].func, func) == 0) { ret = RA2_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = ra2_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret);
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st);
    char *s = json_serialize(out);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(sc.cursors);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = ra2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = ra2_str(ra2_get(rec, "func"));
        const json_t *in = ra2_get(rec, "in");
        if (func && in) ra2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}