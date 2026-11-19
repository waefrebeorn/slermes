/* t_port_web_git_flow.c -- harness for the web_git FLOW port (wg_ prefix).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace") and the final
 * return ("ret"). The echoed "in" is the ORIGINAL input (the oracle re-drives
 * the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order (a queue; list-valued results must be wrapped one level deeper),
 * or {"__script__": true, "ret": <value>, "raise": {type, message}}. A raise
 * spec's message is str(exc) EXACTLY. A missing/exhausted script is a
 * fail-closed RuntimeError RETURNED as a spec (so the trace records the event
 * before the port propagates it) on both sides.
 *
 * NUL-record convention: porcelain -z payloads encode NUL as \x01 in the
 * fixture (libjson strings are NUL-terminated); the oracle decodes/encodes
 * the sentinel on its side. See src/cli/web_git_flow.c header.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Area-qualified include: -I src precedes -I src/cli, so a bare name could
 * resolve to a different file. Include the port .c directly (self-contained). */
#include "cli/web_git_flow.c"

typedef struct {
    json_t *io_map;
    json_t *cursors;
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *fail_closed(const char *fmt, const char *name) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "RuntimeError");
    char msg[256];
    snprintf(msg, sizeof msg, fmt, name);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *dispatch(wg_io_t *io, const char *name, json_t *args) {
    (void)args;   /* borrowed: the port frees its own args objects */
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) return fail_closed("no script for %s", name);
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return fail_closed("script exhausted for %s", name);
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*wg_fn_t)(wg_io_t *, json_t *);
typedef struct { const char *func; wg_fn_t fn; } wg_case_t;
static const wg_case_t CASES[] = {
    { "_git_line", wg__git_line },
    { "_is_dir", wg__is_dir },
    { "_status_z", wg__status_z },
    { "_origin_head", wg__origin_head },
    { "_origin_head_abbrev", wg__origin_head_abbrev },
    { "_ref_exists", wg__ref_exists },
    { "_entry_staged", wg__entry_staged },
    { "_classify", wg__classify },
    { "_status_letter", wg__status_letter },
    { "_review_result", wg__review_result },
    { "_all_add_diff", wg__all_add_diff },
    { "_single_file_diff", wg__single_file_diff },
    { "file_diff_vs_head", wg_file_diff_vs_head },
    { "review_stage", wg_review_stage },
    { "review_unstage", wg_review_unstage },
    { "review_revert", wg_review_revert },
    { "review_rev_parse", wg_review_rev_parse },
    { "_has_staged", wg__has_staged },
    { "_review_push", wg__review_push },
    { "review_push", wg_review_push },
    { "_gh_json", wg__gh_json },
    { "review_ship_info", wg_review_ship_info },
    { "_pr_query", wg__pr_query },
    { "_pr_payload", wg__pr_payload },
    { "_own_pr", wg__own_pr },
    { "review_create_pr", wg_review_create_pr },
    { "worktree_list", wg_worktree_list },
    { "_main_root", wg__main_root },
    { "_fetch_tracking_ref", wg__fetch_tracking_ref },
    { "_slugify", wg__slugify },
    { "_default_branch", wg__default_branch },
    { "_ensure_repo", wg__ensure_repo },
    { "_remote_of_ref", wg__remote_of_ref },
    { "worktree_remove", wg_worktree_remove },
    { "_ref_names", wg__ref_names },
    { "branch_list", wg_branch_list },
    { "branch_switch", wg_branch_switch },
    { "base_branch_list", wg_base_branch_list },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const wg_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            wg_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_free(pin);
            json_free(io.trace);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}