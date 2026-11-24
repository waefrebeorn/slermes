/*
 * pause.c - C11 port of upstream hermes_cli/subcommands/pause.py
 * (gate w24_pause_6e1faf).
 *
 * PoP: w24_pause_6e1faf_pause_output @ hermes_cli/subcommands/pause.py:cmd_pause
 * PoP: w24_pause_6e1faf_resume_output @ hermes_cli/subcommands/pause.py:cmd_resume
 * PoP: w24_pause_6e1faf_parser_spec @ hermes_cli/subcommands/pause.py:build_pause_parser
 *
 * The value layer of the global emergency stop: what `hermes pause` reports for
 * a given estop state (verb, optional "— reason: …" detail, sentinel path and
 * the fixed hold notice), what `hermes resume` reports, and the argparse surface
 * the subcommand installs.  The seams are exactly the ones the module itself
 * reaches for: engage()/get_state()/is_engaged()/disengage()/sentinel_path()
 * become inputs here, so no file is written and no process state is read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_pause_6e1faf.h"

/* "⏸️" (U+23F8 U+FE0F) and "▶️" (U+25B6 U+FE0F), spellable only as bytes here. */
#define PAUSE_GLYPH "\xE2\x8F\xB8\xEF\xB8\x8F"
#define RESUME_GLYPH "\xE2\x96\xB6\xEF\xB8\x8F"
/* "—" (U+2014). */
#define EM_DASH "\xE2\x80\x94"

typedef struct {
    char *p;
    size_t len, cap;
} tbuf;

static void tb_init(tbuf *b) {
    b->cap = 256;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void tb_add(tbuf *b, const char *s) {
    size_t n;
    if (!b->p || !s) return;
    n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        char *np;
        while (b->len + n + 1 > nc) nc *= 2;
        np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->len, s, n + 1);
    b->len += n;
}

/* Python truthiness of a JSON value. */
static int py_truthy(const json_t *v) {
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

/* str() of the shapes an estop reason can hold. */
static char *py_str(const json_t *v) {
    char tmp[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_NULL: return strdup("None");
    case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        return strdup(tmp);
    default: return strdup("?");
    }
}

/* The fixed third line of `hermes pause` (one Python print with embedded \n). */
static const char *PAUSE_NOTICE =
    "    Cron dispatch, kanban dispatch, and new gateway turns (messaging and interactive\n"
    "    CLI, TUI and Desktop turns alike) are on hold. In-flight work keeps running.\n"
    "    Run `hermes resume` to lift the pause.";

json_t *w24_pause_6e1faf_pause_output(const json_t *req) {
    const json_t *state = json_obj_get(req, "state");
    const json_t *state_reason = NULL;
    const char *path = json_get_str(req, "path", "");
    int already = json_get_bool(req, "engaged", 0);
    tbuf b;
    json_t *out;

    if (state && state->type == JSON_OBJECT) state_reason = json_obj_get(state, "reason");
    tb_init(&b);
    tb_add(&b, PAUSE_GLYPH);
    tb_add(&b, "  ");
    tb_add(&b, already ? "Still paused" : "Hermes paused");
    if (py_truthy(state_reason)) {
        char *rs = py_str(state_reason);
        tb_add(&b, " ");
        tb_add(&b, EM_DASH);
        tb_add(&b, " reason: ");
        tb_add(&b, rs);
        free(rs);
    }
    tb_add(&b, "\n    sentinel: ");
    tb_add(&b, path ? path : "");
    tb_add(&b, "\n");
    tb_add(&b, PAUSE_NOTICE);
    tb_add(&b, "\n");

    out = json_object();
    json_set(out, "text", json_string(b.p ? b.p : ""));
    json_set(out, "rc", json_number(0));
    free(b.p);
    return out;
}

json_t *w24_pause_6e1faf_resume_output(const json_t *req) {
    const char *path = json_get_str(req, "path", "");
    int disengaged = json_get_bool(req, "disengaged", 0);
    tbuf b;
    json_t *out;

    tb_init(&b);
    if (disengaged) {
        tb_add(&b, RESUME_GLYPH);
        tb_add(&b, "  Hermes resumed ");
        tb_add(&b, EM_DASH);
        tb_add(&b, " dispatch picks up on the next tick.\n");
    } else {
        tb_add(&b, "Hermes is not paused (no sentinel at ");
        tb_add(&b, path ? path : "");
        tb_add(&b, ").\n");
    }
    out = json_object();
    json_set(out, "text", json_string(b.p ? b.p : ""));
    json_set(out, "rc", json_number(0));
    free(b.p);
    return out;
}

/* The exact strings build_pause_parser installs. */
static const char *PAUSE_HELP = "Emergency stop: pause cron/kanban dispatch and new gateway turns";
static const char *PAUSE_DESC =
    "Engage the global emergency stop. Halts NEW work only " EM_DASH " cron "
    "dispatch, kanban dispatch, and new gateway turns " EM_DASH " until "
    "`hermes resume`. Interactive CLI, TUI and Desktop turns are gateway turns, so they "
    "are held too: each gets \"Hermes is paused\" with the reason. Turns already "
    "running finish; in-flight work is never killed.";
static const char *RESUME_HELP = "Lift the emergency stop set by `hermes pause`";
static const char *RESUME_DESC = "Remove the ESTOP sentinel; dispatch resumes on the next tick.";

json_t *w24_pause_6e1faf_parser_spec(const json_t *req) {
    const char *which = json_get_str(req, "which", "pause");
    const char *field = json_get_str(req, "field", "all");
    int resume = !strcmp(which, "resume");
    json_t *o = json_object();

    if (!strcmp(field, "help")) {
        json_set(o, "value", json_string(resume ? RESUME_HELP : PAUSE_HELP));
        return o;
    }
    if (!strcmp(field, "description")) {
        json_set(o, "value", json_string(resume ? RESUME_DESC : PAUSE_DESC));
        return o;
    }
    if (!strcmp(field, "reason_default")) {
        /* Only `pause` adds --reason; its default is None. */
        json_set(o, "value", resume ? json_null() : json_null());
        return o;
    }
    json_set(o, "help", json_string(resume ? RESUME_HELP : PAUSE_HELP));
    json_set(o, "description", json_string(resume ? RESUME_DESC : PAUSE_DESC));
    json_set(o, "reason_default", json_null());
    return o;
}
