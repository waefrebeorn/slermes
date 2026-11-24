/*
 * gateway_lifecycle_guards.c - C11 port of upstream
 * hermes_cli/gateway_lifecycle_guards.py (gate w26_gatewaylif_903316).
 *
 * PoP: w26_gatewaylif_903316_elapsed @ hermes_cli/update_cmd_drain_report.py:_fmt_elapsed
 * PoP: w26_gatewaylif_903316_describe_unit @ hermes_cli/update_cmd_drain_report.py:describe_active_work_unit
 * PoP: w26_gatewaylif_903316_work_lines @ hermes_cli/gateway_lifecycle_guards.py:_work_lines
 * PoP: w26_gatewaylif_903316_refusal_lines @ hermes_cli/gateway_lifecycle_guards.py:refusal_lines
 * PoP: w26_gatewaylif_903316_constants @ hermes_cli/gateway_lifecycle_guards.py:INTERRUPT_FLAG
 *
 * The gateway probe itself is a seam: the control socket answer (``live_work``)
 * and the cron job registry (``jobs.json``) are fixed inputs here.  What is
 * ported is the part that is a pure function of values: how one ``active_work``
 * entry reads, how a live-work answer becomes the refusal block, and which
 * candidate home's answer is the one that counts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "noc_w26_gatewaylif_903316.h"

#define W26_GL_INTERRUPT_FLAG "--yes"
#define W26_GL_DEFAULT_HINT   "hermes gateway {verb} --yes"
/* U+2022 BULLET, then U+2026 ELLIPSIS-free text; kept as bytes so the source
 * file needs no particular encoding. */
#define W26_GL_BULLET "\xE2\x80\xA2"

/* ------------------------------------------------------------------ buffers */
typedef struct { char *p; size_t n, cap; } gl_buf;

static void gl_buf_add(gl_buf *b, const char *s) {
    size_t l, c;
    char *np;
    if (!s) s = "";
    l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        c = b->cap ? b->cap : 64;
        while (c < b->n + l + 1) c *= 2;
        np = (char *)realloc(b->p, c);
        if (!np) return;
        b->p = np;
        b->cap = c;
    }
    memcpy(b->p + b->n, s, l + 1);
    b->n += l;
}

static void gl_buf_printf(gl_buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    gl_buf_add(b, tmp);
}

static char *gl_buf_take(gl_buf *b) {
    char *out;
    if (!b->p) return strdup("");
    out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

static void gl_buf_free(gl_buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ------------------------------------------------------------ python values */
static int gl_truthy(const json_t *v) {
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

/* str(v) for the scalars an f-string interpolates; containers degrade to JSON. */
static void gl_py_str(gl_buf *b, const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) { gl_buf_add(b, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:  gl_buf_add(b, v->bool_val ? "True" : "False"); return;
    case JSON_STRING: gl_buf_add(b, v->str_val ? v->str_val : ""); return;
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e16) snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        else snprintf(tmp, sizeof(tmp), "%.15g", d);
        gl_buf_add(b, tmp);
        return;
    }
    default: {
        char *s = json_dumps(v, 0);
        gl_buf_add(b, s ? s : "None");
        free(s);
        return;
    }
    }
}

/* Python str.strip(): ASCII whitespace plus the UTF-8 encodings of the common
 * Unicode spaces (NBSP, narrow NBSP) that str.strip() also removes. */
static int gl_space_at(const char *s, size_t *len) {
    unsigned char c = (unsigned char)s[0];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') { *len = 1; return 1; }
    if (c == 0xC2 && (unsigned char)s[1] == 0xA0) { *len = 2; return 1; }
    if (c == 0xE2 && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0xAF) { *len = 3; return 1; }
    return 0;
}

static char *gl_strip(const char *s) {
    const char *start = s ? s : "";
    const char *end;
    size_t l = 0;
    while (*start && gl_space_at(start, &l)) start += l;
    end = start + strlen(start);
    while (end > start) {
        /* step back to the start of the final UTF-8 sequence */
        const char *p = end - 1;
        while (p > start && ((unsigned char)*p & 0xC0) == 0x80) p--;
        if (gl_space_at(p, &l) && p + l == end) end = p;
        else break;
    }
    {
        size_t n = (size_t)(end - start);
        char *out = (char *)malloc(n + 1);
        if (!out) return strdup("");
        memcpy(out, start, n);
        out[n] = '\0';
        return out;
    }
}

/* int(float(x)); 0 = converted, 1 = TypeError/ValueError ("?" for the caller). */
static int gl_int_of(const json_t *v, long long *out) {
    double d;
    if (!v) return 1;
    if (v->type == JSON_NUMBER) d = v->num_val;
    else if (v->type == JSON_BOOL) d = v->bool_val ? 1.0 : 0.0;
    else if (v->type == JSON_STRING) {
        char *end = NULL;
        const char *s = v->str_val ? v->str_val : "";
        d = strtod(s, &end);
        if (end == s) return 1;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end) return 1;
    } else {
        return 1; /* None, dict, list */
    }
    if (isnan(d) || isinf(d)) return 1;
    *out = (long long)d;
    return 0;
}

/* ---------------------------------------------------------------- formatters */
json_t *w26_gatewaylif_903316_elapsed(const json_t *seconds) {
    long long total;
    char tmp[64];
    if (gl_int_of(seconds, &total) != 0) return json_string("?");
    if (total >= 60) {
        long long m = total / 60, s = total % 60; /* int(float) keeps sign; // floors */
        if (s < 0) { m -= 1; s += 60; }
        snprintf(tmp, sizeof(tmp), "%lldm%02llds", m, s);
    } else {
        snprintf(tmp, sizeof(tmp), "%llds", total);
    }
    return json_string(tmp);
}

/* One ``active_work`` entry as the refusal block prints it.  ``home`` is omitted:
 * its only use is the cron job name, which is a jobs.json seam left absent. */
static char *gl_describe_unit(const json_t *unit) {
    gl_buf b = {0};
    const json_t *kind_v, *pid, *elapsed, *session;
    char *kind_s;

    b.p = NULL;
    kind_v = unit && unit->type == JSON_OBJECT ? json_obj_get(unit, "kind") : NULL;
    if (!gl_truthy(kind_v)) {
        kind_s = strdup("work");
    } else {
        gl_buf k = {0};
        gl_py_str(&k, kind_v);
        kind_s = gl_buf_take(&k);
    }
    pid = unit && unit->type == JSON_OBJECT ? json_obj_get(unit, "pid") : NULL;
    elapsed = unit && unit->type == JSON_OBJECT ? json_obj_get(unit, "elapsed_s") : NULL;

    if (!strcmp(kind_s, "cron")) {
        const json_t *job = json_obj_get(unit, "job_id");
        gl_buf_add(&b, "cron job ");
        if (gl_truthy(job)) gl_py_str(&b, job); else gl_buf_add(&b, "?");
        gl_buf_add(&b, gl_truthy(json_obj_get(unit, "external")) ? " in external worker" : " in-process");
        if (gl_truthy(pid)) { gl_buf_add(&b, " pid "); gl_py_str(&b, pid); }
        if (elapsed && elapsed->type != JSON_NULL) {
            json_t *e = w26_gatewaylif_903316_elapsed(elapsed);
            gl_buf_add(&b, ", running ");
            gl_buf_add(&b, e && e->str_val ? e->str_val : "?");
            json_free(e);
        }
        free(kind_s);
        return gl_buf_take(&b);
    }
    if (!strcmp(kind_s, "chat")) {
        const json_t *model = json_obj_get(unit, "model");
        const json_t *tool = json_obj_get(unit, "current_tool");
        gl_buf detail = {0};
        session = json_obj_get(unit, "session");
        gl_buf_add(&b, "chat turn ");
        if (gl_truthy(session)) gl_py_str(&b, session); else gl_buf_add(&b, "?");
        if (gl_truthy(pid)) { gl_buf_add(&b, " pid "); gl_py_str(&b, pid); }
        if (elapsed && elapsed->type != JSON_NULL) {
            json_t *e = w26_gatewaylif_903316_elapsed(elapsed);
            gl_buf_add(&b, ", running ");
            gl_buf_add(&b, e && e->str_val ? e->str_val : "?");
            json_free(e);
        }
        if (gl_truthy(model)) { gl_buf_add(&detail, "model "); gl_py_str(&detail, model); }
        if (gl_truthy(tool)) {
            if (detail.n) gl_buf_add(&detail, ", ");
            gl_buf_add(&detail, "tool ");
            gl_py_str(&detail, tool);
        }
        if (detail.n) {
            gl_buf_add(&b, " [");
            gl_buf_add(&b, detail.p ? detail.p : "");
            gl_buf_add(&b, "]");
        }
        gl_buf_free(&detail);
        free(kind_s);
        return gl_buf_take(&b);
    }
    if (!strcmp(kind_s, "delegation")) {
        const json_t *goal = json_obj_get(unit, "goal");
        const json_t *did = json_obj_get(unit, "delegation_id");
        char *goal_s = NULL;
        gl_buf_add(&b, "background delegation ");
        if (gl_truthy(did)) gl_py_str(&b, did); else gl_buf_add(&b, "?");
        if (gl_truthy(pid)) { gl_buf_add(&b, " pid "); gl_py_str(&b, pid); }
        if (elapsed && elapsed->type != JSON_NULL) {
            json_t *e = w26_gatewaylif_903316_elapsed(elapsed);
            gl_buf_add(&b, ", running ");
            gl_buf_add(&b, e && e->str_val ? e->str_val : "?");
            json_free(e);
        }
        if (gl_truthy(goal)) {
            gl_buf raw = {0};
            gl_py_str(&raw, goal);
            goal_s = gl_strip(raw.p ? raw.p : "");
            gl_buf_free(&raw);
            if (goal_s && goal_s[0]) {
                gl_buf_add(&b, " [");
                gl_buf_add(&b, goal_s);
                gl_buf_add(&b, "]");
            }
        }
        free(goal_s);
        free(kind_s);
        return gl_buf_take(&b);
    }
    /* unknown shapes degrade to "<kind> run< session>< pid>< elapsed>" */
    {
        gl_buf_add(&b, kind_s);
        gl_buf_add(&b, " run");
        session = json_obj_get(unit, "session");
        if (gl_truthy(session)) { gl_buf_add(&b, " "); gl_py_str(&b, session); }
        if (gl_truthy(pid)) { gl_buf_add(&b, " pid "); gl_py_str(&b, pid); }
        if (elapsed && elapsed->type != JSON_NULL) {
            json_t *e = w26_gatewaylif_903316_elapsed(elapsed);
            gl_buf_add(&b, ", running ");
            gl_buf_add(&b, e && e->str_val ? e->str_val : "?");
            json_free(e);
        }
    }
    free(kind_s);
    return gl_buf_take(&b);
}

json_t *w26_gatewaylif_903316_describe_unit(const json_t *unit) {
    char *line = gl_describe_unit(unit);
    json_t *out = json_string(line ? line : "");
    free(line);
    return out;
}

/* f"{flag_hint}".format(verb=verb): only the {verb} field is ever named. */
static char *gl_format_hint(const char *hint, const char *verb) {
    gl_buf b = {0};
    const char *p = hint ? hint : "";
    while (*p) {
        if (p[0] == '{' && p[1] == '{') { gl_buf_add(&b, "{"); p += 2; continue; }
        if (p[0] == '}' && p[1] == '}') { gl_buf_add(&b, "}"); p += 2; continue; }
        if (!strncmp(p, "{verb}", 6)) { gl_buf_add(&b, verb ? verb : ""); p += 6; continue; }
        {
            char one[2];
            one[0] = p[0];
            one[1] = '\0';
            gl_buf_add(&b, one);
        }
        p++;
    }
    return gl_buf_take(&b);
}

static void gl_work_lines(const char *verb, const json_t *work, const char *hint,
                          json_t **out_lines, size_t *out_count) {
    const json_t *units_in, *agents_v;
    size_t nunits = 0, i, cap;
    long long agents = 0, count;
    json_t *lines = json_array();
    gl_buf head = {0};
    char *fmt;

    *out_lines = lines;
    *out_count = 0;
    if (!verb) verb = "";
    units_in = work && work->type == JSON_OBJECT ? json_obj_get(work, "active_work") : NULL;
    cap = units_in && units_in->type == JSON_ARRAY ? json_len(units_in) : 0;
    for (i = 0; i < cap; i++) {
        const json_t *u = json_get(units_in, i);
        if (u && u->type == JSON_OBJECT) nunits++;
    }
    agents_v = work && work->type == JSON_OBJECT ? json_obj_get(work, "active_agents") : NULL;
    if (gl_truthy(agents_v)) (void)gl_int_of(agents_v, &agents); /* int(); non-numeric raises upstream */
    if (nunits > (size_t)agents) count = (long long)nunits; else count = agents;
    if (count <= 0) return;

    gl_buf_printf(&head, "Refusing to %s the gateway: %lld live session(s) would be interrupted.",
                  verb, count);
    json_append(lines, json_string(head.p ? head.p : ""));
    gl_buf_free(&head);

    for (i = 0; i < cap; i++) {
        const json_t *u = json_get(units_in, i);
        char *line;
        gl_buf row = {0};
        if (!u || u->type != JSON_OBJECT) continue;
        line = gl_describe_unit(u);
        gl_buf_add(&row, "  " W26_GL_BULLET " ");
        gl_buf_add(&row, line ? line : "");
        json_append(lines, json_string(row.p ? row.p : ""));
        gl_buf_free(&row);
        free(line);
    }
    if (nunits == 0) {
        gl_buf row = {0};
        gl_buf_printf(&row, "  " W26_GL_BULLET " %lld active agent run(s) (this gateway does not name them)", count);
        json_append(lines, json_string(row.p ? row.p : ""));
        gl_buf_free(&row);
    }
    json_append(lines, json_string("  Let them finish, or interrupt them deliberately:"));
    fmt = gl_format_hint(hint ? hint : W26_GL_DEFAULT_HINT, verb);
    {
        gl_buf row = {0};
        gl_buf_add(&row, "    ");
        gl_buf_add(&row, fmt ? fmt : "");
        gl_buf_printf(&row, "   # interrupts %lld live session(s)", count);
        json_append(lines, json_string(row.p ? row.p : ""));
        gl_buf_free(&row);
    }
    free(fmt);
    *out_count = (size_t)count;
}

json_t *w26_gatewaylif_903316_work_lines(const json_t *req) {
    const json_t *work = req && req->type == JSON_OBJECT ? json_obj_get(req, "work") : NULL;
    const char *verb = json_get_str(req, "verb", "");
    const char *hint = json_get_str(req, "flag_hint", NULL);
    json_t *lines = NULL;
    size_t count = 0;
    gl_work_lines(verb, work, hint ? hint : W26_GL_DEFAULT_HINT, &lines, &count);
    return lines ? lines : json_array();
}

json_t *w26_gatewaylif_903316_refusal_lines(const json_t *req) {
    const json_t *homes, *answers;
    const char *verb = json_get_str(req, "verb", "");
    const char *hint = json_get_str(req, "flag_hint", NULL);
    size_t nh, na, i;

    homes = req && req->type == JSON_OBJECT ? json_obj_get(req, "homes") : NULL;
    answers = req && req->type == JSON_OBJECT ? json_obj_get(req, "answers") : NULL;
    nh = homes && homes->type == JSON_ARRAY ? json_len(homes) : 0;
    na = answers && answers->type == JSON_ARRAY ? json_len(answers) : 0;
    for (i = 0; i < nh; i++) {
        const json_t *answer = i < na ? json_get(answers, i) : NULL;
        json_t *lines = NULL;
        size_t count = 0;
        if (!answer || answer->type != JSON_OBJECT) continue; /* live_work() returned None */
        gl_work_lines(verb, answer, hint ? hint : W26_GL_DEFAULT_HINT, &lines, &count);
        return lines ? lines : json_array();
    }
    return json_array();
}

json_t *w26_gatewaylif_903316_constants(void) {
    json_t *o = json_object();
    json_set(o, "interrupt_flag", json_string(W26_GL_INTERRUPT_FLAG));
    return o;
}
