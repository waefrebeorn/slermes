#include "hermes_json.h"
/*
 * runner.c - C11 port of upstream agent/verify/runner.py
 * (gate w26_runner_fdd9a0).
 *
 * Ported value->value: what a verify run's results serialize to and decide.
 * Proven by the differential oracle gate w26_runner_fdd9a0.
 * PoP: w26_runner_fdd9a0_tail @ agent/verify/runner.py:_tail
 * PoP: w26_runner_fdd9a0_phase_result @ agent/verify/runner.py:PhaseResult.to_dict
 * PoP: w26_runner_fdd9a0_readiness_result @ agent/verify/runner.py:ReadinessResult.to_dict
 * PoP: w26_runner_fdd9a0_verify_ok @ agent/verify/runner.py:VerifyResult.ok
 * PoP: w26_runner_fdd9a0_compose_live_state_reason @ agent/verify/runner.py:_compose_live_state_reason
 * PoP: w26_runner_fdd9a0_verify_refusal @ agent/verify/runner.py:run_verify
 * PoP: w26_runner_fdd9a0_constants @ agent/verify/runner.py:<literals>
 *
 * Seams: subprocess (shell phases, the docker compose ps probe), urllib
 * readiness polls, time.monotonic.  What is ported is what those results
 * decide and how the result objects serialize.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "noc_w26_runner_fdd9a0.h"

#define RV_TAIL_CHARS 2000

/* ---------------------------------------------------------------- helpers */

/* Python int(value); None/unparsable -> 0 with *ok = 0. */
static int rv_py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1 : 0; return 1;
    case JSON_NUMBER:
        if (v->num_val != v->num_val) return 0;
        if (v->num_val >= 9.2233720368547758e18) { *out = LLONG_MAX; return 1; }
        if (v->num_val <= -9.2233720368547758e18) { *out = LLONG_MIN; return 1; }
        *out = (long long)v->num_val; return 1;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long n;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) return 0;
        n = strtoll(s, &end, 10);
        if (!end) return 0;
        while (*end == ' ' || *end == '\t') end++;
        if (*end != '\0') return 0;
        *out = n; return 1;
    }
    default: return 0;
    }
}

/* Python truthiness. */
static int rv_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python round(x, 3) over the durations the runner records. */
static double rv_round3(const json_t *v) {
    double x = 0.0;
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) x = v->num_val;
    else if (v->type == JSON_BOOL) x = v->bool_val ? 1.0 : 0.0;
    else if (v->type == JSON_STRING) x = strtod(v->str_val ? v->str_val : "", NULL);
    if (x != x || x > 1e15 || x < -1e15) return x;
    return nearbyint(x * 1000.0) / 1000.0;
}

static const char *rv_advance(const char *p, long long n) {
    while (n-- > 0 && *p) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
    }
    return p;
}

/* str[start:stop] over UTF-8 code points, as Python slices a str. */
static char *rv_slice_cp(const char *s, long long start, long long stop) {
    const char *base = s ? s : "";
    const char *b, *e;
    size_t n;
    char *out;
    if (start < 0) start = 0;
    if (stop < start) stop = start;
    b = rv_advance(base, start);
    e = rv_advance(b, stop - start);
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

static long long rv_count_cp(const char *s) {
    long long cp = 0;
    const char *p = s ? s : "";
    while (*p) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        cp++;
    }
    return cp;
}

static const char *rv_str_field(const json_t *o, const char *key) {
    const json_t *v = o ? json_obj_get(o, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* str.splitlines() over the ASCII terminators. */
static size_t rv_splitlines(const char *text, char ***out_lines) {
    const char *p = text ? text : "";
    char **lines = NULL;
    size_t n = 0, cap = 0;
    *out_lines = NULL;
    if (!*p) return 0;
    for (;;) {
        const char *q = p;
        size_t len;
        char *line;
        while (*q && *q != '\n' && *q != '\r' && *q != '\v' && *q != '\f') q++;
        len = (size_t)(q - p);
        line = (char *)malloc(len + 1);
        if (!line) break;
        memcpy(line, p, len);
        line[len] = '\0';
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            char **grown = (char **)realloc(lines, ncap * sizeof(char *));
            if (!grown) { free(line); break; }
            lines = grown;
            cap = ncap;
        }
        lines[n++] = line;
        if (*q == '\0') break;
        if (*q == '\r' && q[1] == '\n') q++;
        q++;
        p = q;
        if (*p == '\0') break;
    }
    *out_lines = lines;
    return n;
}

static int rv_blank(const char *s) {
    for (; s && *s; s++)
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' && *s != '\v' && *s != '\f')
            return 0;
    return 1;
}

static int rv_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* ---------------------------------------------------------------- entries */

json_t *w26_runner_fdd9a0_tail(const json_t *in) {
    const json_t *t = json_obj_get(in, "text");
    const json_t *lv = json_obj_get(in, "limit");
    const char *text;
    long long limit = RV_TAIL_CHARS;
    long long len, start;
    char *sliced;
    json_t *out;

    text = (t && t->type == JSON_STRING && t->str_val) ? t->str_val : "";
    if (lv && lv->type != JSON_NULL && !rv_py_int(lv, &limit)) return json_null();
    len = rv_count_cp(text);
    /* text[-limit:] -- and text[-0:] is text[0:], i.e. the whole string. */
    if (limit == 0 || (limit > 0 && len <= limit)) return json_string(text);
    start = (limit > 0) ? len - limit : -limit;
    if (start > len) start = len;
    if (start < 0) start = 0;
    sliced = rv_slice_cp(text, start, len);
    out = json_string(sliced ? sliced : "");
    free(sliced);
    return out;
}

json_t *w26_runner_fdd9a0_phase_result(const json_t *in) {
    const json_t *ecv = json_obj_get(in, "exit_code");
    int timed_out = rv_truthy(json_obj_get(in, "timed_out"));
    long long ec = 0;
    int has_ec = 0;
    json_t *out = json_object();

    if (ecv && ecv->type != JSON_NULL && rv_py_int(ecv, &ec)) has_ec = 1;
    json_set(out, "phase", json_string(rv_str_field(in, "phase")));
    json_set(out, "command", json_string(rv_str_field(in, "command")));
    json_set(out, "exitCode", has_ec ? json_number((double)ec) : json_null());
    json_set(out, "duration", json_number(rv_round3(json_obj_get(in, "duration"))));
    json_set(out, "ok", json_bool(has_ec && ec == 0 && !timed_out));
    json_set(out, "timedOut", json_bool(timed_out));
    json_set(out, "outputTail", json_string(rv_str_field(in, "output_tail")));
    return out;
}

json_t *w26_runner_fdd9a0_readiness_result(const json_t *in) {
    const json_t *scv = json_obj_get(in, "status_code");
    const json_t *ev = json_obj_get(in, "error");
    long long sc = 0;
    int has_sc = 0;
    json_t *out = json_object();

    if (scv && scv->type != JSON_NULL && rv_py_int(scv, &sc)) has_sc = 1;
    json_set(out, "url", json_string(rv_str_field(in, "url")));
    json_set(out, "ready", json_bool(rv_truthy(json_obj_get(in, "ready"))));
    json_set(out, "statusCode", has_sc ? json_number((double)sc) : json_null());
    json_set(out, "duration", json_number(rv_round3(json_obj_get(in, "duration"))));
    if (ev && ev->type == JSON_STRING) json_set(out, "error", json_string(ev->str_val));
    else json_set(out, "error", json_null());
    json_set(out, "outputTail", json_string(rv_str_field(in, "output_tail")));
    return out;
}

/* PhaseResult.ok from a serialized phase dict (or a raw one). */
static int rv_phase_ok(const json_t *p) {
    const json_t *ok = p ? json_obj_get(p, "ok") : NULL;
    const json_t *ec = p ? json_obj_get(p, "exitCode") : NULL;
    const json_t *to = p ? json_obj_get(p, "timedOut") : NULL;
    long long n = 0;
    if (ok) return rv_truthy(ok);
    if (!ec) ec = p ? json_obj_get(p, "exit_code") : NULL;
    if (!to) to = p ? json_obj_get(p, "timed_out") : NULL;
    if (!ec || ec->type == JSON_NULL || !rv_py_int(ec, &n)) return 0;
    return n == 0 && !rv_truthy(to);
}

json_t *w26_runner_fdd9a0_verify_ok(const json_t *in) {
    const json_t *phases = json_obj_get(in, "phases");
    const json_t *readiness = json_obj_get(in, "readiness");
    size_t i, n = (phases && phases->type == JSON_ARRAY) ? json_len(phases) : 0;
    int all_ok = 1;

    for (i = 0; i < n; i++)
        if (!rv_phase_ok(json_get(phases, i))) { all_ok = 0; break; }
    if (readiness && readiness->type == JSON_OBJECT && !rv_truthy(json_obj_get(readiness, "ready")))
        all_ok = 0;
    return json_bool(all_ok);
}

json_t *w26_runner_fdd9a0_compose_live_state_reason(const json_t *in) {
    const char *mode = json_get_str(in, "mode", "ok");
    const json_t *rcv = json_obj_get(in, "returncode");
    const char *stdout_s = rv_str_field(in, "stdout");
    const char *stderr_s = rv_str_field(in, "stderr");
    long long rc = 0;
    char **names = NULL, **detail = NULL;
    size_t n_names, n_detail, i;
    char buf[8192];
    size_t used;

    if (mode && !strcmp(mode, "missing")) return json_null();
    if (mode && !strcmp(mode, "timeout"))
        return json_string("docker compose ps timed out after 15s; live containers cannot be ruled out");
    if (!rcv || rcv->type == JSON_NULL || !rv_py_int(rcv, &rc)) return json_null();
    if (rc != 0) {
        const char *src = stderr_s[0] ? stderr_s : stdout_s;
        size_t start = 0, end = strlen(src);
        char *trimmed;
        while (src[start] && rv_is_space(src[start])) start++;
        while (end > start && rv_is_space(src[end - 1])) end--;
        trimmed = (char *)malloc(end - start + 1);
        if (!trimmed) return json_null();
        memcpy(trimmed, src + start, end - start);
        trimmed[end - start] = '\0';
        n_detail = rv_splitlines(trimmed, &detail);
        used = (size_t)snprintf(buf, sizeof(buf), "docker compose ps failed (exit %lld): ", rc);
        if (n_detail && used < sizeof(buf))
            snprintf(buf + used, sizeof(buf) - used, "%s", detail[n_detail - 1]);
        else if (used < sizeof(buf))
            snprintf(buf + used, sizeof(buf) - used, "no output");
        for (i = 0; i < n_detail; i++) free(detail[i]);
        free(detail);
        free(trimmed);
        return json_string(buf);
    }
    n_names = rv_splitlines(stdout_s, &names);
    { size_t kept = 0;
      for (i = 0; i < n_names; i++) {
          if (rv_blank(names[i])) free(names[i]);
          else names[kept++] = names[i];
      }
      n_names = kept; }
    if (!n_names) { free(names); return json_null(); }
    used = (size_t)snprintf(buf, sizeof(buf),
                            "this compose project already has running container(s): ");
    for (i = 0; i < n_names && used < sizeof(buf); i++)
        used += (size_t)snprintf(buf + used, sizeof(buf) - used, "%s%s", i ? ", " : "", names[i]);
    for (i = 0; i < n_names; i++) free(names[i]);
    free(names);
    return json_string(buf);
}

json_t *w26_runner_fdd9a0_verify_refusal(const json_t *in) {
    static const char *const REST =
        ". `docker compose build` + `up` would replace live containers on an image-hash "
        "change, destroying any container-local state they carry. If you intend to rebuild "
        "this live deployment, run `docker compose build`/`up` yourself.";
    const json_t *build = json_obj_get(in, "build");
    const char *reason = json_get_str(in, "reason", "");
    const char *command = "docker compose build";
    char *tail;
    size_t need;
    json_t *out = json_object();
    json_t *phases = json_array();
    json_t *phase = json_object();

    if (build && build->type == JSON_ARRAY && json_len(build) > 0) {
        const json_t *first = json_get(build, 0);
        if (first && first->type == JSON_STRING && first->str_val) command = first->str_val;
    }
    need = strlen("Refusing to run: ") + strlen(reason) + strlen(REST) + 1;
    tail = (char *)malloc(need);
    if (!tail) { json_free(phases); json_free(out); return json_null(); }
    snprintf(tail, need, "Refusing to run: %s%s", reason, REST);
    json_set(phase, "phase", json_string("build"));
    json_set(phase, "command", json_string(command));
    json_set(phase, "exitCode", json_number(1));
    json_set(phase, "duration", json_number(0.0));
    json_set(phase, "ok", json_bool(0));
    json_set(phase, "timedOut", json_bool(0));
    json_set(phase, "outputTail", json_string(tail));
    free(tail);
    json_append(phases, phase);
    json_set(out, "recipe", json_string(json_get_str(in, "recipe", "verify")));
    json_set(out, "ok", json_bool(0));
    json_set(out, "phases", phases);
    json_set(out, "readiness", json_null());
    return out;
}

json_t *w26_runner_fdd9a0_constants(void) {
    json_t *o = json_object();
    json_t *order = json_array();
    json_append(order, json_string("bootstrap"));
    json_append(order, json_string("build"));
    json_append(order, json_string("test"));
    json_set(o, "phase_timeout", json_number(W26_RUNNER_FDD9A0_PHASE_TIMEOUT));
    json_set(o, "ready_timeout", json_number(W26_RUNNER_FDD9A0_READY_TIMEOUT));
    json_set(o, "tail_chars", json_number(W26_RUNNER_FDD9A0_TAIL_CHARS));
    json_set(o, "phase_order", order);
    return o;
}

/* PoP: agent_verify_runner_u_ok @ agent/verify/runner.py:PhaseResult.ok */
/* exit_code == 0 and not timed_out. Input JSON {"exit_code":N|null,"timed_out":b}. */
int agent_verify_runner_u_ok(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    if (!o || !json_is_object(o)) { if (o) json_free(o); return 0; }
    json_t *ec = json_obj_get(o, "exit_code");
    int zero = (ec && json_is_number(ec) && ec->num_val == 0.0) ? 1 : 0;
    json_t *to = json_obj_get(o, "timed_out");
    int timed = 0;
    if (to) {
        if (json_is_bool(to)) timed = to->bool_val ? 1 : 0;
        else if (json_is_number(to)) timed = (to->num_val != 0.0);
    }
    json_free(o);
    return (zero && !timed) ? 1 : 0;
}

/* PoP: agent_verify_runner_u_tail @ agent/verify/runner.py:_tail */
/* text[-limit:] if len(text) > limit else text -- CODEPOINT semantics, and
 * limit == 0 answers the WHOLE text (python -0 == 0). */
static int _tl_cps(const char *s, size_t n, int *out, int max) {
    int k = 0;
    for (size_t i = 0; i < n; ) {
        unsigned char b = (unsigned char)s[i]; int len = 1;
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        if (k < max) out[k] = (int)i;
        k++; i += (size_t)len;
    }
    return k;
}
char *agent_verify_runner_u_tail(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    if (!o || !json_is_object(o)) { if (o) json_free(o); return strdup(""); }
    json_t *tv = json_obj_get(o, "text"), *lv = json_obj_get(o, "limit");
    const char *text = (tv && json_is_string(tv)) ? tv->str_val : "";
    if (!text) text = "";
    long limit = (lv && json_is_number(lv)) ? (long)lv->num_val : 2000;
    size_t n = strlen(text);
    int idx[4096]; int cps = _tl_cps(text, n, idx, 4096);
    char *r;
    if (limit == 0 || cps <= limit) {
        r = strdup(text);
    } else {
        long start;
        if (limit > 0) start = cps - limit; else start = -limit;
        if (start < 0) start = 0;
        if (start > cps) start = cps;
        size_t byte = (start < cps) ? (size_t)idx[start] : n;
        r = strdup(text + byte);
    }
    json_free(o);
    return r;
}
