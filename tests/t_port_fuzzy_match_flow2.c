/*
 * t_port_fuzzy_match_flow2.c -- fixture-driven oracle harness for the
 * tools/fuzzy_match.py UNCLAIMED-gap FLOW port (src/tools/fuzzy_match_flow2.c).
 *
 * Reads JSON-lines cases from stdin: {"func": <name>, "in": {...}}.
 * Emits one JSON line per case: {"func":..,"in":..,"out":{"ret":..,"trace":..,
 * "state":{"correction_notes":..}}}. The universal seam looks up in.io[<name>]
 * and records {"ev":name,"args":...,"ret":...} in call order; a scripted
 * {"__raise__":kind,"msg":text} becomes a pending exception (propagated by the
 * port except at _note_edit_match's except-Exception gate). ABSENT script
 * fail-closes with RuntimeError "no script for <name>".
 *
 * tests/oracle_fuzzy_match_flow2.py drives the LIVE Python with the same
 * scripts and diffs ret + ordered trace + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/fuzzy_match_flow2.c"

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
static bool ghas(const json_t *in, const char *k) {
    return json_obj_get(in, k) != NULL;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t sc = { in, json_array(), json_object() };
    fm2_ctx c;
    memset(&c, 0, sizeof(c));
    c.in = in;
    c.trace = sc.trace;
    c.seqpos = sc.seqpos;
    c.seam = script_call;
    c.seam_ud = &sc;

    /* state: correction_notes list mutation (None when absent/null) */
    json_t *state = json_object();
    json_t *notes = NULL;
    bool has_notes = false;
    const json_t *cn = json_obj_get(in, "correction_notes");
    if (cn && cn->type == JSON_ARRAY) {
        notes = json_copy(cn);
        json_set(state, "correction_notes", notes);
        has_notes = true;
    } else {
        json_set(state, "correction_notes", json_null());
    }

    json_t *r = NULL;
    const char *content = gstr(in, "content", "");
    const char *old_string = gstr(in, "old_string", "");
    const char *new_string = gstr(in, "new_string", "");

    if (strcmp(func, "window_spans") == 0) {
        fm2_strs lines = fm2_json_strs(json_obj_get(in, "content_lines"));
        r = fm2_window_spans(&c, content, &lines, gnum(in, "n", 1), fm2_accept_seam, NULL);
        fm2_strs_free(&lines);
    } else if (strcmp(func, "match_transformed_lines") == 0) {
        r = fm2_match_transformed_lines(&c, content, gstr(in, "pattern", ""));
    } else if (strcmp(func, "strip_boundary") == 0) {
        r = fm2_strip_boundary(&c, json_obj_get(in, "lines"));
    } else if (strcmp(func, "invert_norm_map") == 0) {
        r = fm2_invert_norm_map(json_obj_get(in, "orig_to_norm"));
    } else if (strcmp(func, "norm_end_to_orig") == 0) {
        long v = fm2_norm_end_to_orig(json_obj_get(in, "orig_to_norm"),
                                      gnum(in, "orig_start", 0), gnum(in, "norm_end", 0));
        r = json_number((double)v);
    } else if (strcmp(func, "boundary_note") == 0) {
        r = fm2_boundary_note(json_obj_get(in, "strategies"),
                              ghas(in, "target") ? gstr(in, "target", "") : "old_string's",
                              ghas(in, "where") ? gstr(in, "where", "") : "");
    } else if (strcmp(func, "matched_regions") == 0) {
        fm2_spans m = fm2_json_spans(json_obj_get(in, "matches"));
        char *s = fm2_matched_regions(content, &m);
        r = json_string(s); free(s); free(m.v);
    } else if (strcmp(func, "run_strategy_chain") == 0) {
        r = fm2_run_strategy_chain(&c, content, old_string, new_string,
                                   gbool(in, "replace_all", false), json_obj_get(in, "strategies"));
    } else if (strcmp(func, "strip_line_number_gutter") == 0) {
        long minn = ghas(in, "min_numbered") ? gnum(in, "min_numbered", 2) : 2;
        char *s = fm2_strip_line_number_gutter(gstr(in, "text", ""), minn);
        r = s ? json_string(s) : json_null();
        free(s);
    } else if (strcmp(func, "gutter_stripped_old") == 0) {
        r = fm2_gutter_stripped_old(content, old_string);
    } else if (strcmp(func, "gutter_recovery") == 0) {
        r = fm2_gutter_recovery(&c, content, gstr(in, "stripped_old", ""),
                                json_obj_get(in, "strategies"), new_string,
                                gbool(in, "replace_all", false), has_notes, &notes);
    } else if (strcmp(func, "note_edit_match") == 0) {
        const json_t *s = json_obj_get(in, "strategy");
        const json_t *m = json_obj_get(in, "miss");
        fm2_note_edit_match(&c,
            (s && s->type == JSON_STRING) ? s->str_val : NULL,
            (m && m->type == JSON_STRING) ? m->str_val : NULL);
        r = NULL;
    } else if (strcmp(func, "detect_newline_literal_drift") == 0) {
        fm2_spans m = fm2_json_spans(json_obj_get(in, "matches"));
        r = fm2_detect_newline_literal_drift(content, &m, old_string, new_string);
        free(m.v);
    } else if (strcmp(func, "backslash_runs") == 0) {
        r = fm2_backslash_runs(gstr(in, "s", ""));
    } else if (strcmp(func, "detect_backslash_doubling") == 0) {
        r = fm2_detect_backslash_doubling(gstr(in, "matched_regions", ""), old_string, new_string);
    } else if (strcmp(func, "match_starts_mid_line") == 0) {
        bool v = fm2_match_starts_mid_line(content, gnum(in, "start", 0), gnum(in, "end", 0));
        r = json_bool(v);
    } else if (strcmp(func, "unicode_edit_plan") == 0) {
        r = fm2_unicode_edit_plan(&c, old_string, new_string);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (c.exc) {
        json_set(out, "ret", c.exc);
        c.exc = NULL;
    } else {
        json_set(out, "ret", r ? r : json_null());
    }
    json_set(out, "trace", sc.trace);
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
