/*
 * cli_cron_lines.c — C port of upstream Hermes hermes_cli/cli_cron_lines.py.
 *
 * PoP: w12_clicronli_42411f_normalize_skills @ hermes_cli/cli_cron_lines.py:_normalize_skills
 * PoP: w12_clicronli_42411f_parse_cron_flags @ hermes_cli/cli_cron_lines.py:_parse_cron_flags
 * PoP: w12_clicronli_42411f_add_lines @ hermes_cli/cli_cron_lines.py:_add_lines
 * PoP: w12_clicronli_42411f_job_action_lines @ hermes_cli/cli_cron_lines.py:_job_action_lines
 *
 * Translation (_t) is stubbed to the identity key and the _cron store result is
 * injected from the fixture; the control flow and data shaping are ported.
 */
#include "noc_w12_clicronli_42411f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEBUF 8192

static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static int jbool(const json_t *o, const char *k, int d) {
    return (o && o->type == JSON_OBJECT) ? (json_get_bool(o, k, d != 0) ? 1 : 0) : d;
}
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

/* Python str(value or "") followed by .strip() (ASCII whitespace here). */
static void py_str_strip(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL || (v->type == JSON_BOOL && !v->bool_val) ||
        (v->type == JSON_NUMBER && v->num_val == 0.0)) {
        out[0] = '\0';
        return;
    }
    if (v->type == JSON_STRING) snprintf(out, cap, "%s", v->str_val ? v->str_val : "");
    else if (v->type == JSON_BOOL) snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(out, cap, "%lld", (long long)d);
        else snprintf(out, cap, "%g", d);
    } else { out[0] = '\0'; return; }
    char *s = out;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    if (s != out) memmove(out, s, n + 1);
}

static int py_parse_int(const char *s, long long *out) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    if (end == s) return 0;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (s >= end) return 0;
    long long v = 0;
    int last_digit = 0, any = 0;
    for (const char *p = s; p < end; p++) {
        if (*p == '_') {
            if (!last_digit || p + 1 >= end || !isdigit((unsigned char)p[1])) return 0;
            last_digit = 0;
            continue;
        }
        if (!isdigit((unsigned char)*p)) return 0;
        v = v * 10 + (*p - '0');
        last_digit = 1;
        any = 1;
    }
    if (!any || !last_digit) return 0;
    *out = neg ? -v : v;
    return 1;
}

static void push_line(json_t *arr, const char *fmt, const char *a, const char *b) {
    char buf[LINEBUF];
    if (b) snprintf(buf, sizeof(buf), fmt, a, b);
    else if (a) snprintf(buf, sizeof(buf), fmt, a);
    else snprintf(buf, sizeof(buf), "%s", fmt);
    json_append(arr, json_string(buf));
}

struct json_t *w12_clicronli_42411f_normalize_skills(const struct json_t *values) {
    json_t *out = json_array();
    if (!values || values->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(values); i++) {
        char text[LINEBUF];
        py_str_strip(json_get(values, i), text, sizeof(text));
        if (!*text) continue;
        int seen = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            const char *prior = json_string_value(json_get(out, j));
            if (prior && !strcmp(prior, text)) { seen = 1; break; }
        }
        if (!seen) json_append(out, json_string(text));
    }
    return out;
}

struct json_t *w12_clicronli_42411f_parse_cron_flags(const struct json_t *tokens) {
    json_t *o = json_object();
    json_set(o, "name", json_null());
    json_set(o, "deliver", json_null());
    json_set(o, "repeat", json_null());
    json_set(o, "prompt", json_null());
    json_set(o, "schedule", json_null());
    json_set(o, "skills", json_array());
    json_set(o, "add_skills", json_array());
    json_set(o, "remove_skills", json_array());
    json_set(o, "clear_skills", json_bool(false));
    json_set(o, "all", json_bool(false));
    json_set(o, "positionals", json_array());
    if (!tokens || tokens->type != JSON_ARRAY) return o;
    size_t n = json_len(tokens);
    for (size_t i = 0; i < n; ) {
        json_t *tv = json_get(tokens, i);
        const char *tok = (tv && tv->type == JSON_STRING) ? tv->str_val : "";
        int has_value = (i + 1 < n);
        json_t *next = has_value ? json_get(tokens, i + 1) : NULL;
        const char *val = (next && next->type == JSON_STRING) ? next->str_val : "";
        if (!strcmp(tok, "--clear-skills") || !strcmp(tok, "--all")) {
            json_set(o, !strcmp(tok, "--all") ? "all" : "clear_skills", json_bool(true));
            i += 1;
        } else if (has_value && (!strcmp(tok, "--skill") || !strcmp(tok, "--add-skill") ||
                                 !strcmp(tok, "--remove-skill"))) {
            const char *key = !strcmp(tok, "--skill") ? "skills"
                            : !strcmp(tok, "--add-skill") ? "add_skills" : "remove_skills";
            json_append(jget(o, key), json_string(val));
            i += 2;
        } else if (has_value && !strcmp(tok, "--repeat")) {
            long long r = 0;
            if (!py_parse_int(val, &r)) { json_free(o); return NULL; }
            json_set(o, "repeat", json_number((double)r));
            i += 2;
        } else if (has_value && (!strcmp(tok, "--name") || !strcmp(tok, "--deliver") ||
                                 !strcmp(tok, "--prompt") || !strcmp(tok, "--schedule"))) {
            const char *key = !strcmp(tok, "--name") ? "name" : !strcmp(tok, "--deliver") ? "deliver"
                            : !strcmp(tok, "--prompt") ? "prompt" : "schedule";
            json_set(o, key, json_string(val));
            i += 2;
        } else {
            json_append(jget(o, "positionals"), json_string(tok));
            i += 1;
        }
    }
    return o;
}

struct json_t *w12_clicronli_42411f_add_lines(const char *subcommand, const struct json_t *opts,
                                              const struct json_t *result) {
    (void)subcommand;
    json_t *out = json_array();
    json_t *pos = jget(opts, "positionals");
    size_t np = (pos && pos->type == JSON_ARRAY) ? json_len(pos) : 0;
    if (!np) { push_line(out, "cron.usage_add", NULL, NULL); return out; }
    const char *first = np ? json_string_value(json_get(pos, 0)) : "";
    const char *sched_v = jstr(opts, "schedule", "");
    char schedule[LINEBUF];
    snprintf(schedule, sizeof(schedule), "%s", *sched_v ? sched_v : (first ? first : ""));
    const char *prompt_v = jstr(opts, "prompt", "");
    char joined[LINEBUF];
    joined[0] = '\0';
    for (size_t i = 1; i < np; i++) {
        const char *p = json_string_value(json_get(pos, i));
        if (i > 1) strncat(joined, " ", sizeof(joined) - strlen(joined) - 1);
        strncat(joined, p ? p : "", sizeof(joined) - strlen(joined) - 1);
    }
    char prompt[LINEBUF];
    snprintf(prompt, sizeof(prompt), "%s", *prompt_v ? prompt_v : joined);
    (void)schedule;  /* the injected translation stub renders only the key */
    json_t *norm = w12_clicronli_42411f_normalize_skills(jget(opts, "skills"));
    if (!*prompt && json_len(norm) == 0) { json_free(norm); push_line(out, "cron.need_prompt_or_skill", NULL, NULL); return out; }
    (void)norm;
    if (!jtruthy(jget(result, "success"))) { push_line(out, "cron.create_failed", NULL, NULL); return out; }
    push_line(out, "cron.created", NULL, NULL);
    push_line(out, "  cron.schedule", NULL, NULL);
    json_t *rskills = jget(result, "skills");
    if (jtruthy(rskills)) push_line(out, "  cron.skills", NULL, NULL);
    push_line(out, "  cron.next_run", NULL, NULL);
    return out;
}

struct json_t *w12_clicronli_42411f_job_action_lines(const char *subcommand, const struct json_t *opts,
                                                     const struct json_t *result) {
    json_t *out = json_array();
    json_t *pos = jget(opts, "positionals");
    size_t np = (pos && pos->type == JSON_ARRAY) ? json_len(pos) : 0;
    if (!np) { push_line(out, "cron.usage_action", NULL, NULL); return out; }
    const char *sub = subcommand ? subcommand : "";
    const char *action = (!strcmp(sub, "remove") || !strcmp(sub, "rm") || !strcmp(sub, "delete"))
                         ? "remove" : sub;
    if (!jtruthy(jget(result, "success"))) {
        char key[LINEBUF];
        snprintf(key, sizeof(key), "cron.%s_failed", action);
        push_line(out, key, NULL, NULL);
        return out;
    }
    if (!strcmp(action, "remove")) { push_line(out, "cron.removed", NULL, NULL); return out; }
    json_t *job = jget(result, "job");
    if (!strcmp(action, "run") && job && jtruthy(jget(job, "execution_skipped"))) {
        push_line(out, "cron.did_not_run", NULL, NULL);
        return out;
    }
    if (!strcmp(action, "pause")) push_line(out, "cron.paused", NULL, NULL);
    else if (!strcmp(action, "resume")) push_line(out, "cron.resumed", NULL, NULL);
    else push_line(out, "cron.triggered", NULL, NULL);
    if (!strcmp(action, "resume")) push_line(out, "  cron.next_run", NULL, NULL);
    return out;
}
