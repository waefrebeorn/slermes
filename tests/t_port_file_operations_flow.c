/*
 * t_port_file_operations_flow.c — oracle harness for the unclaimed
 * tools/file_operations.py FLOW cores + leaves ported in
 * src/tools/file_operations_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/file_operations_flow.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_file_operations_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered seam trace + return (+ init self state).
 *
 * The internal "__too_large__" tag (the _TooLarge isinstance model) is stripped
 * from the emitted ret — live projects the plain ExecuteResult field dict.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/file_operations_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;     /* array of events */
    json_t *counters;
} script_ctx_t;

static json_t *make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

/* fh_read seam results are hex strings. LIVE's FHDouble.read() calls
 * bytes.fromhex() on them at the seam boundary: ASCII whitespace is allowed
 * BETWEEN bytes only, a non-hex digit raises at its raw index, and an odd
 * digit count raises the even-number message. On success the value is the
 * normalized lowercase hex that bytes.fromhex(...).hex() would produce.
 * Returns a ValueError raise spec on failure, else NULL (result replaced). */
static json_t *fh_read_hex_materialize(json_t **pres) {
    json_t *result = *pres;
    if (!result || result->type != JSON_STRING) return NULL;
    const char *s = result->str_val ? result->str_val : "";
    size_t n = strlen(s);
    char *digits = (char *)malloc(n + 1);
    if (!digits) return NULL;
    size_t nd = 0, i = 0;
    json_t *err = NULL;
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n'
                         || s[i] == '\r' || s[i] == '\v' || s[i] == '\f')) i++;
        if (i >= n) break;
        if (fo_hexval(s[i]) < 0) {
            char m[160];
            snprintf(m, sizeof m,
                     "non-hexadecimal number found in fromhex() arg at position %zu", i);
            err = make_raise("ValueError", m);
            goto done;
        }
        digits[nd++] = s[i++];
        if (i >= n || fo_hexval(s[i]) < 0) {
            if (i >= n) {
                err = make_raise("ValueError",
                                 "fromhex() arg must contain an even number of hexadecimal digits");
                goto done;
            }
            char m[160];
            snprintf(m, sizeof m,
                     "non-hexadecimal number found in fromhex() arg at position %zu", i);
            err = make_raise("ValueError", m);
            goto done;
        }
        digits[nd++] = s[i++];
    }
    {
        char *out = (char *)malloc(nd + 1);
        for (size_t j = 0; j < nd; j++)
            out[j] = (digits[j] >= 'A' && digits[j] <= 'F') ? (char)(digits[j] + 32) : digits[j];
        out[nd] = '\0';
        json_free(result);
        *pres = json_string(out);
        free(out);
    }
done:
    free(digits);
    return err;
}

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            char *m = fo_fmt("no script for %s", name);
            result = make_raise("RuntimeError", m);
            free(m);
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        char *m = fo_fmt("no script for %s", name);
        result = make_raise("RuntimeError", m);
        free(m);
    }
    /* A scripted JSON null is delivered as a NULL pointer, not a json_null node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    /* Materialize fh_read hex results at the harness boundary: LIVE's
     * FHDouble.read() applies bytes.fromhex() here, before the port sees data. */
    if (strcmp(name, "fh_read") == 0 && result && result->type == JSON_STRING) {
        json_t *hexerr = fh_read_hex_materialize(&result);
        if (hexerr) { json_free(result); result = hexerr; }
    }
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, d)

/* Strip the internal __too_large__ tag recursively (live projects the plain
 * ExecuteResult fields; the tag only models isinstance(failed, _TooLarge)). */
static void strip_tags(json_t *v) {
    if (!v) return;
    if (v->type == JSON_OBJECT) {
        json_obj_del(v, "__too_large__");
        for (size_t i = 0; i < v->c.count; i++) strip_tags(v->c.items[i]);
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) strip_tags(v->c.items[i]);
    }
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    fo_io_t io = {script_call, &ctx, J("env"), J("self"), J("amb")};
    json_t *state = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "init")) {
        state = fo_init(&io, J("env"), J("cwd"));
    } else if (!strcmp(func, "_cat")) {
        r = fo_cat(&io, SD("path", ""));
    } else if (!strcmp(func, "_head")) {
        r = fo_head(&io, SD("path", ""), (long long)N("nbytes", 0));
    } else if (!strcmp(func, "_bounded")) {
        char *s = fo_bounded(SD("encoder", ""), SD("arg", ""), J("max_bytes"));
        r = json_string(s);
        free(s);
    } else if (!strcmp(func, "_failed_read")) {
        r = fo_failed_read(&io, SD("path", ""), SD("payload", ""), (long long)N("read_rc", 0));
    } else if (!strcmp(func, "_matches_size")) {
        r = fo_matches_size(&io, SD("data", ""), SD("size_segment", ""));
    } else if (!strcmp(func, "_fenced_read")) {
        r = fo_fenced_read(&io, SD("body", ""), J("more"));
    } else if (!strcmp(func, "_read_exact_bytes_hex")) {
        r = fo_read_exact_bytes_hex(&io, SD("path", ""), J("max_bytes"));
    } else if (!strcmp(func, "_read_exact_bytes")) {
        r = fo_read_exact_bytes(&io, SD("path", ""), J("max_bytes"));
    } else if (!strcmp(func, "_probe_regular_file")) {
        r = fo_probe_regular_file(&io, SD("path", ""));
    } else if (!strcmp(func, "_env_unavailable_error")) {
        r = fo_env_unavailable_error(SD("path", ""), SD("status", "env_unavailable"));
    } else if (!strcmp(func, "_detect_binary")) {
        r = fo_detect_binary(&io, SD("path", ""));
    } else if (!strcmp(func, "_native_path")) {
        r = fo_native_path(&io, SD("path", ""));
    } else if (!strcmp(func, "_python_interpreter_cmd")) {
        r = fo_python_interpreter_cmd(&io);
    } else if (!strcmp(func, "_exec_python_snippet")) {
        r = fo_exec_python_snippet(&io, SD("snippet", ""), J("py"));
    } else if (!strcmp(func, "read_file_bytes")) {
        r = fo_read_file_bytes(&io, SD("path", ""), J("max_bytes"));
    } else if (!strcmp(func, "_reject_unencodable")) {
        r = fo_reject_unencodable(SD("path", ""), J("content"));
    } else if (!strcmp(func, "_json_nonstandard_constant")) {
        r = fo_json_nonstandard_constant(&io, SD("text", ""));
    } else if (!strcmp(func, "_refuse_introduced_json_constant")) {
        r = fo_refuse_introduced_json_constant(&io, SD("path", ""), SD("content", ""), J("pre_content"));
    } else if (!strcmp(func, "_fail_closed_syntax_error")) {
        r = fo_fail_closed_syntax_error(&io, SD("path", ""), SD("ext", ""), SD("content", ""));
    } else if (!strcmp(func, "_verify_written_hash")) {
        r = fo_verify_written_hash(&io, SD("path", ""), SD("content_bytes", ""));
    } else if (!strcmp(func, "write_file")) {
        r = fo_write_file(&io, SD("path", ""), SD("content", ""), J("pre_content"));
    } else if (!strcmp(func, "_no_match_result")) {
        r = fo_no_match_result(&io, SD("path", ""), SD("content", ""), SD("old_string", ""),
                               SD("new_string", ""), (long long)N("match_count", 0), J("error"));
    } else if (!strcmp(func, "_verify_patch_persisted")) {
        r = fo_verify_patch_persisted(&io, SD("path", ""), SD("new_content", ""));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    strip_tags(r);
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) { strip_tags(state); json_set(out, "state", state); }
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}