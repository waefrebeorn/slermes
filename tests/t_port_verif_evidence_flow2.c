/*
 * t_port_verif_evidence_flow2.c -- fixture-driven oracle harness for the
 * agent/verification_evidence.py UNCLAIMED-gap FLOW port
 * (src/agent/verif_evidence_flow2.c).
 *
 * Reads JSON-lines cases from stdin: {"func": <name>, "in": {...}}.
 * Emits one JSON line per case: {"func":..,"in":..,"out":{"ret":..,"trace":..,
 * "state":{"unattributed":..}}}. The universal seam looks up in.io[<name>]
 * and records {"ev":name,"args":..,"ret":..} in call order; an ABSENT script
 * fail-closes with a RuntimeError raise spec. A scripted {"__raise__":..}
 * becomes a pending exception propagated by the port (caught only at the
 * documented except sites, using the FULL builtin exception hierarchy).
 *
 * tests/oracle_verif_evidence_flow2.py drives the LIVE Python with the same
 * scripts and diffs ret + ordered trace + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/verif_evidence_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seqpos;
} script_ctx_t;

static json_t *script_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = (script_ctx_t *)ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = script;
    if (script && script->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(script, "__seq__");
        if (seq && seq->type == JSON_ARRAY) {
            const json_t *pos = json_obj_get(c->seqpos, name);
            size_t idx = (pos && pos->type == JSON_NUMBER) ? (size_t)pos->num_val : 0;
            size_t n = json_len(seq);
            if (n == 0) {
                val = NULL;
            } else {
                if (idx >= n) idx = n - 1;
                val = json_get(seq, idx);
                json_set(c->seqpos, name, json_number((double)(idx + 1)));
            }
        }
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (!script) {
        json_t *rs = json_object();
        json_set(rs, "__raise__", json_string("RuntimeError"));
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        json_set(rs, "msg", json_string(m));
        json_set(e, "ret", json_copy(rs));
        json_append(c->trace, e);
        return rs;
    }
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const char *gstr(const json_t *in, const char *k, const char *def) {
    return json_get_str(in, k, def);
}
static long gnum(const json_t *in, const char *k, long def) {
    const json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_NUMBER) ? (long)v->num_val : def;
}
static bool gbool(const json_t *in, const char *k, bool def) {
    const json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}
static bool ghas(const json_t *in, const char *k) { return json_obj_get(in, k) != NULL; }

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t sc = { in, json_array(), json_object() };
    ve2_ctx c;
    memset(&c, 0, sizeof(c));
    c.in = in;
    c.trace = sc.trace;
    c.seqpos = sc.seqpos;
    c.seam = script_call;
    c.seam_ud = &sc;

    /* state: unattributed runs (echo input, _insert_evidence mutates it) */
    json_t *state = json_object();
    json_t *unattr = json_array();
    const json_t *ua = json_obj_get(in, "unattributed");
    if (ua && ua->type == JSON_ARRAY) unattr = json_copy(ua);
    c.unattr = unattr;

    json_t *r = NULL;
    if (strcmp(func, "ledger_enabled") == 0) {
        r = ve2_jb(ve2_ledger_enabled(&c));
    } else if (strcmp(func, "split_shell_segments") == 0) {
        r = ve2_split_shell_segments(&c, gstr(in, "command", ""), gbool(in, "posix", true));
    } else if (strcmp(func, "segment_status") == 0) {
        r = ve2_segment_status(json_obj_get(in, "segments"), gnum(in, "match_index", 0),
                               gnum(in, "exit_code", 0), json_obj_get(in, "pipeline_status"));
    } else if (strcmp(func, "wrapper_option_width") == 0) {
        json_t *fl = json_obj_get(in, "flags");
        json_t *vl = json_obj_get(in, "valued");
        r = ve2_jn(ve2_wrapper_option_width(gstr(in, "token", ""), fl, vl));
    } else if (strcmp(func, "skip_wrapper") == 0) {
        r = ve2_skip_wrapper(&c, json_obj_get(in, "tokens"), gnum(in, "i", 0));
    } else if (strcmp(func, "interpreter_family") == 0) {
        r = ve2_interpreter_family(gstr(in, "token", ""));
    } else if (strcmp(func, "is_harmless_interpreter_flag") == 0) {
        r = ve2_jb(ve2_is_harmless_interpreter_flag(gstr(in, "token", "")));
    } else if (strcmp(func, "normalize_tokens") == 0) {
        r = ve2_normalize_tokens(json_obj_get(in, "tokens"));
    } else if (strcmp(func, "is_under") == 0) {
        const json_t *b = json_obj_get(in, "base");
        r = ve2_jb(ve2_is_under(&c, gstr(in, "token", ""),
                                (b && b->type == JSON_STRING) ? b->str_val : NULL));
    } else if (strcmp(func, "is_temp_script_path") == 0) {
        const json_t *b = json_obj_get(in, "root");
        r = ve2_jb(ve2_is_temp_script_path(&c, gstr(in, "token", ""),
                                           (b && b->type == JSON_STRING) ? b->str_val : NULL));
    } else if (strcmp(func, "is_interpreter_token") == 0) {
        r = ve2_jb(ve2_is_interpreter_token(gstr(in, "token", "")));
    } else if (strcmp(func, "ad_hoc_script_args") == 0) {
        const json_t *b = json_obj_get(in, "root");
        r = ve2_ad_hoc_script_args(&c, json_obj_get(in, "tokens"),
                                   (b && b->type == JSON_STRING) ? b->str_val : NULL);
    } else if (strcmp(func, "root_for") == 0) {
        const json_t *cwd = json_obj_get(in, "cwd");
        char *s = ve2_root_for(&c, json_obj_get(in, "facts"),
                               (cwd && cwd->type == JSON_STRING) ? cwd->str_val : NULL);
        r = ve2_js(s); free(s);
    } else if (strcmp(func, "load_changed_paths") == 0) {
        const json_t *raw = json_obj_get(in, "raw");
        r = ve2_load_changed_paths(&c, (raw && raw->type == JSON_STRING) ? raw->str_val : NULL);
    } else if (strcmp(func, "classify_in_workspace") == 0) {
        const json_t *cwd = json_obj_get(in, "cwd");
        const json_t *sid = json_obj_get(in, "session_id");
        const json_t *outp = json_obj_get(in, "output");
        r = ve2_classify_in_workspace(&c, gstr(in, "command", ""), json_obj_get(in, "facts"),
                 (cwd && cwd->type == JSON_STRING) ? cwd->str_val : NULL,
                 (sid && sid->type == JSON_STRING) ? sid->str_val : NULL,
                 gnum(in, "exit_code", 0),
                 (outp && outp->type == JSON_STRING) ? outp->str_val : "",
                 json_obj_get(in, "pipeline_status"));
    } else if (strcmp(func, "last_unattributed_run") == 0) {
        const json_t *sid = json_obj_get(in, "session_id");
        const json_t *root = json_obj_get(in, "root");
        const json_t *since = json_obj_get(in, "since");
        r = ve2_last_unattributed_run(
            (sid && sid->type == JSON_STRING) ? sid->str_val : NULL,
            (root && root->type == JSON_STRING) ? root->str_val : NULL,
            (since && since->type == JSON_STRING) ? since->str_val : NULL, unattr);
    } else if (strcmp(func, "record_verify_run") == 0) {
        const json_t *root = json_obj_get(in, "root");
        const json_t *sid = json_obj_get(in, "session_id");
        const json_t *cmd = json_obj_get(in, "command");
        const json_t *scp = json_obj_get(in, "scope");
        const json_t *outp = json_obj_get(in, "output");
        r = ve2_record_verify_run(&c,
            (root && root->type == JSON_STRING) ? root->str_val : "",
            (sid && sid->type == JSON_STRING) ? sid->str_val : NULL,
            gbool(in, "ok", true),
            (cmd && cmd->type == JSON_STRING) ? cmd->str_val : "hermes verify",
            (scp && scp->type == JSON_STRING) ? scp->str_val : "full",
            (outp && outp->type == JSON_STRING) ? outp->str_val : "");
    } else if (strcmp(func, "insert_evidence") == 0) {
        json_t *ev = json_object();
        const char *ks[] = {"command", "canonical_command", "kind", "scope", "status",
                            "exit_code", "cwd", "root", "session_id", "output_summary"};
        for (size_t i = 0; i < sizeof(ks) / sizeof(ks[0]); i++) {
            const json_t *v = json_obj_get(in, ks[i]);
            json_set(ev, ks[i], v ? json_copy(v) : (i == 5 ? json_number(0) : json_string("")));
        }
        r = ve2_insert_evidence(&c, ev);
        json_free(ev);
    } else if (strcmp(func, "newest_workspace_verify") == 0) {
        const json_t *root = json_obj_get(in, "root");
        const json_t *since = json_obj_get(in, "since");
        r = ve2_newest_workspace_verify(&c,
            (root && root->type == JSON_STRING) ? root->str_val : "",
            (since && since->type == JSON_STRING) ? since->str_val : NULL);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (c.exc) { json_set(out, "ret", c.exc); c.exc = NULL; }
    else json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", sc.trace);
    json_set(state, "unattributed", unattr);
    json_set(out, "state", state);
    json_free(sc.seqpos);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        char *in_s = json_serialize(in ? in : empty);
        char *out_s = json_serialize(out);
        json_free(empty);
        json_t *fn = json_string(func);
        char *fn_s = json_serialize(fn);
        json_free(fn);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", fn_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(fn_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
