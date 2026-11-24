/*
 * cli_status_bar_mixin.c - real C11 port of the pure rendering helpers in upstream Hermes
 *   hermes_cli/cli_status_bar_mixin.py.
 * The live mixin methods that touch prompt_toolkit, the pet renderer or state.db stay in Python;
 * every pure predicate/formatter below is mirrored byte-for-byte, with time and the i18n catalog
 * injected as explicit seams.
 *
 * PoP: w19_clistatusb_91df97_threshold_style @ hermes_cli/cli_status_bar_mixin.py:_threshold_style
 * PoP: w19_clistatusb_91df97_finite @ hermes_cli/cli_status_bar_mixin.py:_finite
 * PoP: w19_clistatusb_91df97_context_style @ hermes_cli/cli_status_bar_mixin.py:_status_bar_context_style
 * PoP: w19_clistatusb_91df97_cache_style @ hermes_cli/cli_status_bar_mixin.py:_cache_hit_rate_style
 * PoP: w19_clistatusb_91df97_battery_style @ hermes_cli/cli_status_bar_mixin.py:_battery_status_style
 * PoP: w19_clistatusb_91df97_compression_style @ hermes_cli/cli_status_bar_mixin.py:_compression_count_style
 * PoP: w19_clistatusb_91df97_context_bar @ hermes_cli/cli_status_bar_mixin.py:_build_context_bar
 * PoP: w19_clistatusb_91df97_prompt_elapsed @ hermes_cli/cli_status_bar_mixin.py:_format_prompt_elapsed
 * PoP: w19_clistatusb_91df97_trim_text @ hermes_cli/cli_status_bar_mixin.py:_trim_status_bar_text
 * PoP: w19_clistatusb_91df97_title_badge @ hermes_cli/cli_status_bar_mixin.py:_status_title_badge
 * PoP: w19_clistatusb_91df97_right_align @ hermes_cli/cli_status_bar_mixin.py:_right_align_status_title
 * PoP: w19_clistatusb_91df97_minimal_chrome @ hermes_cli/cli_status_bar_mixin.py:_use_minimal_tui_chrome
 * PoP: w19_clistatusb_91df97_scrollback_width @ hermes_cli/cli_status_bar_mixin.py:_scrollback_box_width
 * PoP: w19_clistatusb_91df97_cache_hit_rate @ hermes_cli/cli_status_bar_mixin.py:_cache_hit_rate
 * PoP: w19_clistatusb_91df97_goal_segment @ hermes_cli/cli_status_bar_mixin.py:_status_bar_goal_segment
 * PoP: w19_clistatusb_91df97_stash_age @ hermes_cli/cli_status_bar_mixin.py:_fmt_stash_age
 * PoP: w19_clistatusb_91df97_field_set @ hermes_cli/cli_status_bar_mixin.py:_get_status_bar_field_set
 */
#include "hermes_json.h"
#include "noc_w19_clistatusb_91df97.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIM    "class:status-bar-dim"
#define STRONG "class:status-bar-strong"
/* UTF-8 glyphs kept as escapes so this source stays ASCII. */
#define GLYPH_TIMER_OFF "\xE2\x8F\xB2"
#define GLYPH_TIMER_ON  "\xE2\x8F\xB1"
#define GLYPH_CACHE     "\xE2\x97\x8E"
#define GLYPH_GOAL      "\xE2\x8A\x99"
#define GLYPH_FULL      "\xE2\x96\x88"
#define GLYPH_EMPTY     "\xE2\x96\x91"
#define GLYPH_RULE      "\xE2\x94\x80"

static char *dupstr(const char *s) {
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s) + 1;
    o = (char *)malloc(n);
    if (!o) return NULL;
    memcpy(o, s, n);
    return o;
}

static char *format_dup(const char *fmt, ...) {
    va_list ap;
    int n;
    char *buf;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return dupstr("");
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) return dupstr("");
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip_dup(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *o;
    while (*b && py_isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && py_isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, b, n);
    o[n] = '\0';
    return o;
}

static char *py_rstrip_dup(const char *s) {
    const char *b = s ? s : "";
    const char *e = b + strlen(b);
    size_t n;
    char *o;
    while (e > b && py_isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, b, n);
    o[n] = '\0';
    return o;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default: return 0;
    }
}

/* Display cells for the status bar: one column per Unicode codepoint (ASCII text plus the
 * single-cell box/gear glyphs above - never a raw byte count). */
static long long cw(const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    long long n = 0;
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

const char *w19_clistatusb_91df97_threshold_style(double value, const json_t *ladder,
                                                  const char *fallback) {
    static char buf[128];
    size_t i;
    if (ladder && ladder->type == JSON_ARRAY) {
        for (i = 0; i < json_len(ladder); i++) {
            json_t *pair = json_get(ladder, i);
            json_t *bound, *name;
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) continue;
            bound = json_get(pair, 0);
            name = json_get(pair, 1);
            if (!bound || bound->type != JSON_NUMBER) continue;
            if (value >= bound->num_val) {
                const char *nm = (name && name->type == JSON_STRING) ? name->str_val : "";
                snprintf(buf, sizeof buf, "class:status-bar-%s", nm);
                return buf;
            }
        }
    }
    snprintf(buf, sizeof buf, "class:status-bar-%s", fallback ? fallback : "");
    return buf;
}

json_t *w19_clistatusb_91df97_finite(const json_t *v) {
    double d;
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type != JSON_NUMBER) return json_copy((json_t *)v);
    d = v->num_val;
    if (d != d || d < 0 || d > 1e6) return json_null();
    return json_number(d);
}

const char *w19_clistatusb_91df97_context_style(const json_t *percent_used) {
    if (!percent_used || percent_used->type == JSON_NULL) return DIM;
    {
        double p = percent_used->num_val;
        json_t *ladder;
        const char *r;
        static char buf[128];
        if (p >= 95) return "class:status-bar-critical";
        if (p > 80) return "class:status-bar-bad";
        ladder = json_array();
        {
            json_t *pair = json_array();
            json_append(pair, json_number(50));
            json_append(pair, json_string("warn"));
            json_append(ladder, pair);
        }
        r = w19_clistatusb_91df97_threshold_style(p, ladder, "good");
        snprintf(buf, sizeof buf, "%s", r);
        json_free(ladder);
        return buf;
    }
}

const char *w19_clistatusb_91df97_cache_style(double cache_pct) {
    static char buf[128];
    const char *r;
    json_t *ladder = json_array();
    json_t *p1 = json_array(), *p2 = json_array();
    json_append(p1, json_number(70));
    json_append(p1, json_string("good"));
    json_append(p2, json_number(40));
    json_append(p2, json_string("warn"));
    json_append(ladder, p1);
    json_append(ladder, p2);
    r = w19_clistatusb_91df97_threshold_style(cache_pct, ladder, "bad");
    snprintf(buf, sizeof buf, "%s", r);
    json_free(ladder);
    return buf;
}

const char *w19_clistatusb_91df97_battery_style(const char *category) {
    if (category) {
        if (!strcmp(category, "good")) return "class:status-bar-good";
        if (!strcmp(category, "warn")) return "class:status-bar-warn";
        if (!strcmp(category, "bad")) return "class:status-bar-bad";
        if (!strcmp(category, "critical")) return "class:status-bar-critical";
    }
    return DIM;
}

const char *w19_clistatusb_91df97_compression_style(double count) {
    static char buf[128];
    const char *r;
    json_t *ladder = json_array();
    json_t *p1 = json_array(), *p2 = json_array();
    json_append(p1, json_number(10));
    json_append(p1, json_string("bad"));
    json_append(p2, json_number(5));
    json_append(p2, json_string("warn"));
    json_append(ladder, p1);
    json_append(ladder, p2);
    r = w19_clistatusb_91df97_threshold_style(count, ladder, "dim");
    snprintf(buf, sizeof buf, "%s", r);
    json_free(ladder);
    return buf;
}

char *w19_clistatusb_91df97_context_bar(const json_t *percent_used, long long width) {
    double safe;
    long filled, i;
    size_t cap;
    char *out;
    size_t pos = 0;
    if (!percent_used || percent_used->type != JSON_NUMBER) safe = 0.0;
    else safe = percent_used->num_val;
    if (safe < 0) safe = 0;
    if (safe > 100) safe = 100;
    /* Python round() is round-half-to-even; rint() honours that mode by default. */
    filled = (long)rint((safe / 100.0) * (double)width);
    if (filled < 0) filled = 0;
    cap = (size_t)(filled > width ? filled : width) * 3 + 8;
    out = (char *)malloc(cap);
    if (!out) return dupstr("");
    out[pos++] = '[';
    for (i = 0; i < filled; i++) { memcpy(out + pos, GLYPH_FULL, 3); pos += 3; }
    for (i = filled; i < width; i++) { memcpy(out + pos, GLYPH_EMPTY, 3); pos += 3; }
    out[pos++] = ']';
    out[pos] = '\0';
    return out;
}

char *w19_clistatusb_91df97_prompt_elapsed(const json_t *prompt_start_time,
                                           double prompt_duration, int live, double now) {
    double elapsed, days, rem, hours, minutes_f, seconds_f;
    long minutes, seconds;
    char *time_str;
    const char *glyph = live ? GLYPH_TIMER_ON : GLYPH_TIMER_OFF;
    if ((!prompt_start_time || prompt_start_time->type == JSON_NULL) && prompt_duration == 0.0)
        return format_dup("%s 0s", GLYPH_TIMER_OFF);
    if (prompt_start_time && prompt_start_time->type == JSON_NUMBER)
        elapsed = now - prompt_start_time->num_val;
    else
        elapsed = prompt_duration;
    if (elapsed < 0) elapsed = 0;
    days = floor(elapsed / 86400.0);
    rem = elapsed - days * 86400.0;
    hours = floor(rem / 3600.0);
    rem -= hours * 3600.0;
    minutes_f = floor(rem / 60.0);
    seconds_f = rem - minutes_f * 60.0;
    minutes = (long)minutes_f;
    seconds = (long)seconds_f;
    if (days > 0)
        time_str = format_dup("%lldd %lldh %ldm", (long long)days, (long long)hours, minutes);
    else if (hours > 0)
        time_str = seconds ? format_dup("%lldh %ldm %lds", (long long)hours, minutes, seconds)
                           : format_dup("%lldh %ldm", (long long)hours, minutes);
    else if (minutes > 0)
        time_str = seconds ? format_dup("%ldm %lds", minutes, seconds)
                           : format_dup("%ldm", minutes);
    else
        time_str = format_dup("%llds", (long long)elapsed);
    {
        char *out = format_dup("%s %s", glyph, time_str);
        free(time_str);
        return out;
    }
}

char *w19_clistatusb_91df97_trim_text(const char *text, long long max_width) {
    const char *src = text ? text : "";
    size_t len = strlen(src), i;
    size_t out_len;
    char *out;
    if (max_width <= 0) return dupstr("");
    if ((long long)len <= max_width) return dupstr(src);
    if (max_width <= 3) {
        out = (char *)malloc((size_t)max_width + 1);
        if (!out) return dupstr("");
        for (i = 0; i < (size_t)max_width; i++) out[i] = '.';
        out[max_width] = '\0';
        return out;
    }
    out_len = 0;
    for (i = 0; i < len; i++) {
        if ((long long)out_len + 1 + 3 > max_width) break;
        out_len++;
    }
    {
        char *head = (char *)malloc(out_len + 1);
        char *trimmed;
        if (!head) return dupstr("");
        memcpy(head, src, out_len);
        head[out_len] = '\0';
        trimmed = py_rstrip_dup(head);
        free(head);
        out = format_dup("%s...", trimmed ? trimmed : "");
        free(trimmed);
    }
    return out;
}

/* (badge, left_width) or NULL. */
static char *title_badge_internal(const char *title, long long width, long long *left_width) {
    char *t = py_strip_dup(title ? title : "");
    long long title_width, suffix;
    char *trimmed, *badge;
    if (!t || !t[0] || width < 24) { free(t); return NULL; }
    title_width = width / 3;
    if (title_width > 30) title_width = 30;
    if (title_width < 6) title_width = 6;
    trimmed = w19_clistatusb_91df97_trim_text(t, title_width - 2);
    free(t);
    badge = format_dup(" %s ", trimmed ? trimmed : "");
    free(trimmed);
    suffix = cw(" " GLYPH_RULE) + cw(badge);
    *left_width = width - suffix;
    if (*left_width < 0) *left_width = 0;
    return badge;
}

json_t *w19_clistatusb_91df97_title_badge(const char *title, long long width) {
    long long left_width = 0;
    char *badge = title_badge_internal(title, width, &left_width);
    json_t *out;
    if (!badge) return json_null();
    out = json_array();
    json_append(out, json_string(badge));
    json_append(out, json_number((double)left_width));
    free(badge);
    return out;
}

char *w19_clistatusb_91df97_right_align(const char *text, const char *title, long long width) {
    long long left_width = 0;
    char *badge = title_badge_internal(title, width, &left_width);
    char *rstripped, *left, *out;
    long long pad, i;
    size_t cap, pos;
    if (!badge) return w19_clistatusb_91df97_trim_text(text, width);
    rstripped = py_rstrip_dup(text ? text : "");
    left = w19_clistatusb_91df97_trim_text(rstripped, left_width);
    free(rstripped);
    pad = left_width - cw(left);
    if (pad < 0) pad = 0;
    cap = strlen(left) + (size_t)pad + strlen(badge) + 8;
    out = (char *)malloc(cap);
    if (!out) { free(left); free(badge); return dupstr(""); }
    pos = 0;
    memcpy(out + pos, left, strlen(left));
    pos += strlen(left);
    for (i = 0; i < pad; i++) out[pos++] = ' ';
    memcpy(out + pos, " " GLYPH_RULE, 4);
    pos += 4;
    memcpy(out + pos, badge, strlen(badge));
    pos += strlen(badge);
    out[pos] = '\0';
    free(left);
    free(badge);
    return out;
}

int w19_clistatusb_91df97_minimal_chrome(long long width) {
    return width < 64 ? 1 : 0;
}

long long w19_clistatusb_91df97_scrollback_width(const json_t *width) {
    long long w;
    if (!width || width->type != JSON_NUMBER) w = 80;
    else {
        w = (long long)width->num_val;
        if (w == 0) w = 80;
    }
    return w < 32 ? 32 : w;
}

json_t *w19_clistatusb_91df97_cache_hit_rate(const json_t *snapshot, long long precision) {
    json_t *delta, *read_tok, *prompt_tok;
    json_t *out;
    char *label;
    double pct;
    if (!snapshot || snapshot->type != JSON_OBJECT) return json_null();
    delta = json_obj_get(snapshot, "cache_hit_pct");
    if (delta && delta->type == JSON_NUMBER) {
        char *f = format_dup("%.*f", (int)precision, delta->num_val);
        label = format_dup(GLYPH_CACHE " %s%%", f ? f : "");
        free(f);
        out = json_array();
        json_append(out, json_number(delta->num_val));
        json_append(out, json_string(label));
        free(label);
        return out;
    }
    read_tok = json_obj_get(snapshot, "session_cache_read_tokens");
    prompt_tok = json_obj_get(snapshot, "session_prompt_tokens");
    {
        double read_v = (read_tok && read_tok->type == JSON_NUMBER) ? read_tok->num_val : 0.0;
        double prompt_v = (prompt_tok && prompt_tok->type == JSON_NUMBER) ? prompt_tok->num_val : 0.0;
        if (!(read_v > 0 && prompt_v > 0)) return json_null();
        pct = read_v / prompt_v * 100.0;
    }
    {
        char *f = format_dup("%.*f", (int)precision, pct);
        label = format_dup(GLYPH_CACHE " %s%%", f ? f : "");
        free(f);
    }
    out = json_array();
    json_append(out, json_number(pct));
    json_append(out, json_string(label));
    free(label);
    return out;
}

char *w19_clistatusb_91df97_goal_segment(const json_t *snapshot) {
    json_t *active, *used, *max_turns;
    if (!snapshot || snapshot->type != JSON_OBJECT) return dupstr("");
    active = json_obj_get(snapshot, "goal_active");
    if (!py_truthy(active)) return dupstr("");
    used = json_obj_get(snapshot, "goal_turns_used");
    max_turns = json_obj_get(snapshot, "goal_max_turns");
    if (!used || used->type != JSON_NUMBER) used = NULL;
    if (!max_turns || max_turns->type != JSON_NUMBER) max_turns = NULL;
    if (max_turns && max_turns->num_val != 0.0)
        return format_dup(GLYPH_GOAL " goal %lld/%lld", used ? (long long)used->num_val : 0,
                          (long long)max_turns->num_val);
    return format_dup(GLYPH_GOAL " goal");
}

char *w19_clistatusb_91df97_stash_age(double stashed_at, double now) {
    long long secs = (long long)(now - stashed_at);
    long long mins;
    if (secs < 10) return dupstr("just now");
    if (secs < 90) return format_dup("%llds ago", secs);
    mins = (long long)floor((double)secs / 60.0);
    if (mins < 60) return format_dup("%lld min ago", mins);
    return format_dup("%lldh ago", (long long)floor((double)mins / 60.0));
}

static int cmp_str(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

json_t *w19_clistatusb_91df97_field_set(const json_t *cli_config) {
    json_t *display = NULL, *status_bar = NULL, *fields = NULL;
    json_t *out;
    char **names = NULL;
    size_t n = 0, i;
    if (cli_config && cli_config->type == JSON_OBJECT) {
        display = json_obj_get(cli_config, "display");
        if (display && display->type == JSON_OBJECT) {
            status_bar = json_obj_get(display, "status_bar");
            if (status_bar && status_bar->type == JSON_OBJECT)
                fields = json_obj_get(status_bar, "fields");
        }
    }
    if (!fields || fields->type != JSON_ARRAY || json_len(fields) == 0) return json_null();
    names = (char **)calloc(json_len(fields), sizeof *names);
    if (!names) return json_null();
    for (i = 0; i < json_len(fields); i++) {
        json_t *f = json_get(fields, i);
        if (!f) { names[n++] = dupstr(""); continue; }
        if (f->type == JSON_STRING) {
            names[n++] = dupstr(f->str_val ? f->str_val : "");
        } else if (f->type == JSON_NUMBER) {
            char tmp[64];
            if (f->num_val == (double)(long long)f->num_val)
                snprintf(tmp, sizeof tmp, "%lld", (long long)f->num_val);
            else
                snprintf(tmp, sizeof tmp, "%g", f->num_val);
            names[n++] = dupstr(tmp);
        } else if (f->type == JSON_BOOL) {
            names[n++] = dupstr(f->bool_val ? "True" : "False");
        } else if (f->type == JSON_NULL) {
            names[n++] = dupstr("None");
        } else {
            names[n++] = dupstr("");
        }
    }
    if (n > 1) qsort((void *)names, n, sizeof *names, cmp_str);
    out = json_array();
    for (i = 0; i < n; i++) json_append(out, json_string(names[i] ? names[i] : ""));
    for (i = 0; i < n; i++) free(names[i]);
    free(names);
    return out;
}
