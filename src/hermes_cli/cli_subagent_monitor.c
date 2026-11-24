/*
 * cli_subagent_monitor.c - C11 port of upstream
 * hermes_cli/cli_subagent_monitor.py (gate w23_clisubagen_f7c046).
 *
 * PoP: w23_clisubagen_f7c046_clip @ hermes_cli/cli_subagent_monitor.py:_clip
 * PoP: w23_clisubagen_f7c046_counts @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor._counts
 * PoP: w23_clisubagen_f7c046_title @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor._title
 * PoP: w23_clisubagen_f7c046_agent_activity @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor._agent_activity
 * PoP: w23_clisubagen_f7c046_collapsed_activity @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor._collapsed_activity
 * PoP: w23_clisubagen_f7c046_process_activity @ hermes_cli/cli_process_dock.py:process_activity
 * PoP: w23_clisubagen_f7c046_dock_text @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor.dock_text
 * PoP: w23_clisubagen_f7c046_select @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor.select
 * PoP: w23_clisubagen_f7c046_view @ hermes_cli/cli_subagent_monitor.py:SubagentMonitor.roster
 * PoP: w23_clisubagen_f7c046_modal_prompt_active @ hermes_cli/cli_subagent_monitor.py:modal_prompt_active
 * PoP: w23_clisubagen_f7c046_read_tail @ hermes_cli/cli_subagent_monitor.py:read_tail
 * PoP: w23_clisubagen_f7c046_consts @ hermes_cli/cli_subagent_monitor.py:QUEUE_ROWS
 *
 * The subagent backend, the process registry, the session dock rows and the
 * clock are the seams: the caller supplies the snapshot those sources would
 * have returned, and this file owns only what the monitor draws from it.  The
 * English catalog strings the dock paints are inlined verbatim.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w23_clisubagen_f7c046.h"

#define A_MID "\xc2\xb7"           /* U+00B7 MIDDLE DOT */
#define A_ELL "\xe2\x80\xa6"       /* U+2026 HORIZONTAL ELLIPSIS */
#define A_BULLET "\xe2\x97\x8f"    /* U+25CF BLACK CIRCLE */
#define A_GEAR "\xe2\x9a\x99"      /* U+2699 GEAR */
#define A_CHECK "\xe2\x9c\x94"     /* U+2714 HEAVY CHECK MARK */
#define A_CROSS "\xe2\x9c\x98"     /* U+2718 HEAVY BALLOT X */
#define A_HOURGLASS "\xe2\x8f\xb3" /* U+23F3 HOURGLASS */
#define A_PAUSE "\xe2\x8f\xb8"     /* U+23F8 PAUSE */

#define A_SEP " " A_MID " "

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n, cap;
} sb_t;

static void sb_init(sb_t *b) {
    b->cap = 128;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_grow(sb_t *b, size_t extra) {
    size_t want;
    char *q;
    if (!b->p) return;
    want = b->n + extra + 1;
    if (want <= b->cap) return;
    while (b->cap < want) b->cap *= 2;
    q = (char *)realloc(b->p, b->cap);
    if (!q) return;
    b->p = q;
}

static void sb_putn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_grow(b, n);
    if (!b->p) return;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_puts(sb_t *b, const char *s) {
    if (s) sb_putn(b, s, strlen(s));
}

static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static void sb_putf(sb_t *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) sb_putn(b, tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
}

static char *sb_take(sb_t *b) {
    char *out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

static char *sa_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* ------------------------------------------------------------------ */
/* UTF-8, cell width, printability                                     */
/* ------------------------------------------------------------------ */

static unsigned sa_u8_dec(const unsigned char *s, size_t len, size_t *adv) {
    unsigned cp;
    size_t need, i;
    unsigned char c = s[0];

    if (c < 0x80) {
        *adv = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        need = 1;
        cp = (unsigned)(c & 0x1F);
        if (cp < 2) {
            *adv = 1;
            return 0xFFFD;
        }
    } else if ((c & 0xF0) == 0xE0) {
        need = 2;
        cp = (unsigned)(c & 0x0F);
    } else if ((c & 0xF8) == 0xF0) {
        need = 3;
        cp = (unsigned)(c & 0x07);
    } else {
        *adv = 1;
        return 0xFFFD;
    }
    for (i = 1; i <= need; i++) {
        if (i >= len) {
            *adv = i;
            return 0xFFFD;
        }
        if ((s[i] & 0xC0) != 0x80) {
            *adv = i;
            return 0xFFFD;
        }
        cp = (cp << 6) | (unsigned)(s[i] & 0x3F);
    }
    if (need == 2 && cp < 0x800) {
        *adv = 1;
        return 0xFFFD;
    }
    if (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)) {
        *adv = 1;
        return 0xFFFD;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        *adv = 1;
        return 0xFFFD;
    }
    *adv = need + 1;
    return cp;
}

static void sa_u8_enc(sb_t *b, unsigned cp) {
    char o[4];
    if (cp < 0x80) {
        o[0] = (char)cp;
        sb_putn(b, o, 1);
    } else if (cp < 0x800) {
        o[0] = (char)(0xC0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 2);
    } else if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 3);
    } else {
        o[0] = (char)(0xF0 | (cp >> 18));
        o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[3] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 4);
    }
}

/* prompt_toolkit get_cwidth: 0 for control / zero-width, 2 for East Asian
 * wide, 1 otherwise. */
static int sa_cwidth_cp(unsigned cp) {
    if (cp == 0) return 0;
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (cp >= 0x0300 && cp <= 0x036F) return 0;
    if (cp >= 0x200B && cp <= 0x200F) return 0;
    if (cp == 0xFEFF) return 0;
    if (cp == 0x3000) return 2;
    if (cp >= 0x1100 && cp <= 0x115F) return 2;
    if (cp >= 0x2E80 && cp <= 0x303E) return 2;
    if (cp >= 0x3041 && cp <= 0x33FF) return 2;
    if (cp >= 0x3400 && cp <= 0x4DBF) return 2;
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 2;
    if (cp >= 0xA000 && cp <= 0xA4CF) return 2;
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 2;
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;
    if (cp >= 0xFE30 && cp <= 0xFE6F) return 2;
    if (cp >= 0xFF00 && cp <= 0xFF60) return 2;
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return 2;
    if (cp >= 0x1F300 && cp <= 0x1F64F) return 2;
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return 2;
    if (cp >= 0x20000 && cp <= 0x2FFFD) return 2;
    return 1;
}

static int sa_cwidth(const char *s) {
    size_t i = 0, n;
    int total = 0;
    if (!s) return 0;
    n = strlen(s);
    while (i < n) {
        size_t adv = 0;
        unsigned cp = sa_u8_dec((const unsigned char *)s + i, n - i, &adv);
        total += sa_cwidth_cp(cp);
        i += adv ? adv : 1;
    }
    return total;
}

/* str.isprintable(): false for control / separator / format / private-use. */
static int sa_printable_cp(unsigned cp) {
    if (cp < 0x20) return 0;
    if (cp >= 0x7F && cp <= 0xA0) return 0;
    if (cp == 0xAD) return 0;
    if (cp >= 0x2000 && cp <= 0x200F) return 0;
    if (cp >= 0x2028 && cp <= 0x202F) return 0;
    if (cp == 0x205F) return 0;
    if (cp == 0x3000) return 0;
    if (cp == 0xFEFF) return 0;
    if (cp >= 0xFFF9 && cp <= 0xFFFB) return 0;
    if (cp >= 0xE000 && cp <= 0xF8FF) return 0;
    if (cp >= 0xF0000) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Python str() coercions                                              */
/* ------------------------------------------------------------------ */

static void sa_py_repr_into(sb_t *b, const json_t *v);

static void sa_fmt_num(sb_t *b, double d) {
    char buf[64];
    if (d == (double)(long long)d) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    } else {
        int p;
        buf[0] = '\0';
        for (p = 12; p <= 17; p++) {
            snprintf(buf, sizeof(buf), "%.*g", p, d);
            if (strtod(buf, NULL) == d) break;
        }
        if (!buf[0]) snprintf(buf, sizeof(buf), "%g", d);
    }
    sb_puts(b, buf);
}

static void sa_repr_str_into(sb_t *b, const char *s) {
    sb_putc(b, '\'');
    if (s) {
        for (; *s; s++) {
            unsigned char c = (unsigned char)*s;
            if (c == '\'') sb_puts(b, "\\'");
            else if (c == '\\') sb_puts(b, "\\\\");
            else if (c == '\n') sb_puts(b, "\\n");
            else if (c == '\r') sb_puts(b, "\\r");
            else if (c == '\t') sb_puts(b, "\\t");
            else sb_putc(b, (char)c);
        }
    }
    sb_putc(b, '\'');
}

static void sa_py_repr_into(sb_t *b, const json_t *v) {
    size_t i, n;
    if (!v) {
        sb_puts(b, "None");
        return;
    }
    switch (v->type) {
    case JSON_NULL: sb_puts(b, "None"); break;
    case JSON_BOOL: sb_puts(b, v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER: sa_fmt_num(b, v->num_val); break;
    case JSON_STRING: sa_repr_str_into(b, v->str_val); break;
    case JSON_ARRAY:
        sb_putc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            sa_py_repr_into(b, json_get(v, i));
        }
        sb_putc(b, ']');
        break;
    case JSON_OBJECT:
        sb_putc(b, '{');
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            sa_repr_str_into(b, json_object_get_key_at(v, i));
            sb_puts(b, ": ");
            sa_py_repr_into(b, json_object_get_at(v, i));
        }
        sb_putc(b, '}');
        break;
    default: sb_puts(b, "None"); break;
    }
}

static void sa_py_str_into(sb_t *b, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        sb_puts(b, v->str_val ? v->str_val : "");
        return;
    }
    if (v && v->type == JSON_NUMBER) {
        sa_fmt_num(b, v->num_val);
        return;
    }
    sa_py_repr_into(b, v);
}

static int sa_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY: return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default: return 0;
    }
}

/* str(value or "") */
static char *sa_str_or_empty(const json_t *v) {
    sb_t b;
    sb_init(&b);
    if (sa_truthy(v)) sa_py_str_into(&b, v);
    return sb_take(&b);
}

static int sa_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* " ".join(text.split()) */
static char *sa_collapse(const char *s) {
    sb_t b;
    int pending = 0;
    sb_init(&b);
    if (!s) return sb_take(&b);
    for (; *s; s++) {
        if (sa_is_space(*s)) {
            if (b.n) pending = 1;
            continue;
        }
        if (pending) {
            sb_putc(&b, ' ');
            pending = 0;
        }
        sb_putc(&b, *s);
    }
    return sb_take(&b);
}

/* ''.join(c for c in text if c.isprintable()) */
static char *sa_printables(const char *s) {
    sb_t b;
    size_t i = 0, n;
    sb_init(&b);
    if (!s) return sb_take(&b);
    n = strlen(s);
    while (i < n) {
        size_t adv = 0;
        unsigned cp = sa_u8_dec((const unsigned char *)s + i, n - i, &adv);
        if (sa_printable_cp(cp)) sb_putn(&b, s + i, adv ? adv : 1);
        i += adv ? adv : 1;
    }
    return sb_take(&b);
}

static int sa_pdiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

static int sa_imax(int a, int b) { return a > b ? a : b; }
static int sa_imin(int a, int b) { return a < b ? a : b; }

/* ------------------------------------------------------------------ */
/* _clip                                                               */
/* ------------------------------------------------------------------ */

char *w23_clisubagen_f7c046_clip(const json_t *value, int width) {
    char *raw = sa_str_or_empty(value);
    char *collapsed = sa_collapse(raw);
    char *text = sa_printables(collapsed);
    sb_t out;
    size_t i = 0, n;

    free(raw);
    free(collapsed);
    if (!text) return NULL;
    sb_init(&out);
    if (sa_cwidth(text) <= width) {
        char *res = sa_dup(text);
        free(text);
        free(sb_take(&out));
        return res;
    }
    n = strlen(text);
    {
        int prefix = 0;
        int limit = sa_imax(0, width - 1);
        while (i < n) {
            size_t adv = 0;
            unsigned cp = sa_u8_dec((const unsigned char *)text + i, n - i, &adv);
            int w = sa_cwidth_cp(cp);
            if (prefix + w > limit) break;
            sb_putn(&out, text + i, adv ? adv : 1);
            prefix += w;
            i += adv ? adv : 1;
        }
    }
    if (width) sb_puts(&out, A_ELL);
    free(text);
    return sb_take(&out);
}

/* ------------------------------------------------------------------ */
/* snapshot helpers                                                    */
/* ------------------------------------------------------------------ */

static const json_t *sa_rows(const json_t *req, const char *key) {
    const json_t *v = req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
    return v && v->type == JSON_ARRAY ? v : NULL;
}

static size_t sa_row_count(const json_t *rows) { return rows ? json_len(rows) : 0; }

static const char *sa_key_of(const json_t *row) {
    const json_t *v;
    if (!row || row->type != JSON_OBJECT) return NULL;
    v = json_obj_get(row, "key");
    if (!v) v = json_obj_get(row, "subagent_id");
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

static int sa_status_running(const json_t *row) {
    const json_t *s = row && row->type == JSON_OBJECT ? json_obj_get(row, "status") : NULL;
    return s && s->type == JSON_STRING && s->str_val && strcmp(s->str_val, "running") == 0;
}

static json_t *sa_row_at(const json_t *rows, size_t i) { return rows ? json_get(rows, i) : NULL; }

/* ------------------------------------------------------------------ */
/* fragments                                                           */
/* ------------------------------------------------------------------ */

char *w23_clisubagen_f7c046_counts(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    const json_t *goal = req && req->type == JSON_OBJECT ? json_obj_get(req, "goal") : NULL;
    const json_t *badge = req && req->type == JSON_OBJECT ? json_obj_get(req, "loop_badge") : NULL;
    const json_t *queued = sa_rows(req, "queued");
    int session = (req && req->type == JSON_OBJECT) ? (json_get_bool(req, "session", 1) ? 1 : 0) : 1;
    sb_t out;
    int first = 1;
    size_t i, n;

    sb_init(&out);
    if (sa_row_count(entries)) {
        sb_putf(&out, "%llu live", (unsigned long long)sa_row_count(entries));
        first = 0;
    }
    n = sa_row_count(processes);
    if (n) {
        size_t running = 0;
        for (i = 0; i < n; i++)
            if (sa_status_running(sa_row_at(processes, i))) running++;
        if (!first) sb_puts(&out, A_SEP);
        first = 0;
        if (running) sb_putf(&out, "%llu %s", (unsigned long long)running, running == 1 ? "proc" : "procs");
        else sb_putf(&out, "%llu done", (unsigned long long)n);
    }
    if (session && sa_truthy(goal)) {
        const char *g = goal->str_val ? goal->str_val : "";
        if (!first) sb_puts(&out, A_SEP);
        first = 0;
        if (strncmp(g, A_HOURGLASS, 3) == 0) sb_puts(&out, "goal parked");
        else if (strncmp(g, A_PAUSE, 3) == 0) sb_puts(&out, "goal paused");
        else sb_puts(&out, "goal active");
    }
    if (session && sa_truthy(badge)) {
        if (!first) sb_puts(&out, A_SEP);
        first = 0;
        sa_py_str_into(&out, badge);
    }
    if (session && sa_row_count(queued)) {
        if (!first) sb_puts(&out, A_SEP);
        first = 0;
        sb_putf(&out, "%llu queued", (unsigned long long)sa_row_count(queued));
    }
    return sb_take(&out);
}

char *w23_clisubagen_f7c046_title(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    if (sa_row_count(entries) && sa_row_count(processes)) return sa_dup("Live work");
    if (sa_row_count(entries)) return sa_dup("Subagents");
    return sa_dup("Processes");
}

char *w23_clisubagen_f7c046_agent_activity(const json_t *row) {
    const json_t *tool = row && row->type == JSON_OBJECT ? json_obj_get(row, "last_tool") : NULL;
    sb_t b;
    sb_init(&b);
    if (sa_truthy(tool)) {
        sb_puts(&b, "last: ");
        sa_py_str_into(&b, tool);
    } else {
        const json_t *status = row && row->type == JSON_OBJECT ? json_obj_get(row, "status") : NULL;
        if (sa_truthy(status)) sa_py_str_into(&b, status);
        else sb_puts(&b, "starting");
    }
    return sb_take(&b);
}

char *w23_clisubagen_f7c046_process_activity(const json_t *row) {
    sb_t b;
    sb_init(&b);
    if (!row || row->type != JSON_OBJECT) {
        sb_puts(&b, "starting");
        return sb_take(&b);
    }
    if (sa_status_running(row)) {
        const json_t *detail = json_obj_get(row, "detail");
        sa_py_str_into(&b, json_obj_get(row, "elapsed"));
        sb_puts(&b, "s " A_MID " ");
        if (sa_truthy(detail)) {
            sb_puts(&b, "last: ");
            sa_py_str_into(&b, detail);
        } else {
            sb_puts(&b, "starting");
        }
        return sb_take(&b);
    }
    {
        const json_t *status = json_obj_get(row, "status");
        const char *st = status && status->type == JSON_STRING ? status->str_val : "";
        if (st && strcmp(st, "killed") == 0) sb_puts(&b, "killed");
        else if (st && strcmp(st, "lost") == 0) sb_puts(&b, "lost");
        else {
            sb_puts(&b, "exit ");
            sa_py_str_into(&b, json_obj_get(row, "exit_code"));
        }
        sb_puts(&b, A_SEP);
        sa_py_str_into(&b, json_obj_get(row, "since_exit"));
        sb_puts(&b, "s ago");
    }
    return sb_take(&b);
}

char *w23_clisubagen_f7c046_collapsed_activity(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    if (sa_row_count(entries)) return w23_clisubagen_f7c046_agent_activity(sa_row_at(entries, 0));
    if (sa_row_count(processes)) return w23_clisubagen_f7c046_process_activity(sa_row_at(processes, 0));
    {
        const json_t *goal = req && req->type == JSON_OBJECT ? json_obj_get(req, "goal") : NULL;
        const json_t *prompt = req && req->type == JSON_OBJECT ? json_obj_get(req, "loop_prompt") : NULL;
        const json_t *queued = sa_rows(req, "queued");
        sb_t b;
        if (sa_truthy(goal)) return sa_str_or_empty(goal);
        if (sa_truthy(prompt)) return sa_str_or_empty(prompt);
        sb_init(&b);
        sb_puts(&b, "next: ");
        if (sa_row_count(queued)) sa_py_str_into(&b, sa_row_at(queued, 0));
        return sb_take(&b);
    }
}

/* ------------------------------------------------------------------ */
/* dock_text                                                           */
/* ------------------------------------------------------------------ */

static char *clip_text(const char *s, int width) {
    json_t *tmp = json_string(s ? s : "");
    char *out = w23_clisubagen_f7c046_clip(tmp, width);
    json_free(tmp);
    return out;
}

static char *clip_node(const json_t *v, int width) { return w23_clisubagen_f7c046_clip(v, width); }

typedef struct {
    char **items;
    size_t n, cap;
} lines_t;

static void lines_init(lines_t *l) {
    l->cap = 8;
    l->n = 0;
    l->items = (char **)malloc(l->cap * sizeof(char *));
}

static void lines_add(lines_t *l, char *s) {
    if (!l->items) {
        free(s);
        return;
    }
    if (l->n + 2 > l->cap) {
        char **q;
        l->cap *= 2;
        q = (char **)realloc(l->items, l->cap * sizeof(char *));
        if (!q) {
            free(s);
            return;
        }
        l->items = q;
    }
    l->items[l->n++] = s;
}

static void lines_free(lines_t *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->n = l->cap = 0;
}

static char *collapsed_dock(const json_t *req, int columns) {
    char *count = w23_clisubagen_f7c046_counts(req);
    char *title = w23_clisubagen_f7c046_title(req);
    char *activity = w23_clisubagen_f7c046_collapsed_activity(req);
    int has_blocks = sa_row_count(sa_rows(req, "entries")) || sa_row_count(sa_rows(req, "processes"));
    char *cands[4];
    int ncand = 0, i, width;
    char *heading = NULL;
    char *out;

    if (has_blocks) {
        sb_t t;
        sb_init(&t);
        sb_puts(&t, title);
        sb_puts(&t, A_SEP);
        sb_puts(&t, count);
        sb_puts(&t, " " A_MID " Ctrl+T expand " A_MID " Ctrl+R restore");
        cands[ncand++] = sb_take(&t);
        sb_init(&t);
        sb_puts(&t, count);
        sb_puts(&t, " " A_MID " Ctrl+T expand " A_MID " Ctrl+R restore");
        cands[ncand++] = sb_take(&t);
        sb_init(&t);
        sb_puts(&t, count);
        sb_puts(&t, " " A_MID " Ctrl+T " A_MID " Ctrl+R");
        cands[ncand++] = sb_take(&t);
        cands[ncand++] = sa_dup(count);
    } else {
        sb_t t;
        sb_init(&t);
        sb_puts(&t, count);
        sb_puts(&t, " " A_MID " Ctrl+R restore");
        cands[ncand++] = sb_take(&t);
        sb_init(&t);
        sb_puts(&t, count);
        sb_puts(&t, " " A_MID " Ctrl+R");
        cands[ncand++] = sb_take(&t);
        cands[ncand++] = sa_dup(count);
    }
    width = sa_imax(0, columns - 1);
    for (i = 0; i < ncand; i++) {
        if (sa_cwidth(cands[i]) <= width) {
            heading = sa_dup(cands[i]);
            break;
        }
    }
    if (!heading) heading = sa_dup(count);
    for (i = 0; i < ncand; i++) free(cands[i]);

    {
        int room = width - sa_cwidth(heading) - sa_cwidth(A_SEP);
        if (sa_cwidth(activity) <= room || (room >= 12 && !has_blocks)) {
            json_t *aj = json_string(activity);
            char *mid = w23_clisubagen_f7c046_clip(aj, room);
            sb_t joined;
            sb_init(&joined);
            sb_puts(&joined, heading);
            sb_puts(&joined, A_SEP);
            sb_puts(&joined, mid);
            free(mid);
            json_free(aj);
            free(heading);
            heading = sb_take(&joined);
        }
    }
    {
        sb_t sp;
        sb_init(&sp);
        sb_putc(&sp, ' ');
        sb_puts(&sp, heading);
        out = clip_text(sp.p, sa_imax(0, columns));
        free(sb_take(&sp));
    }
    free(heading);
    free(count);
    free(title);
    free(activity);
    return out;
}

char *w23_clisubagen_f7c046_dock_text(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    const json_t *goal = req && req->type == JSON_OBJECT ? json_obj_get(req, "goal") : NULL;
    const json_t *loop = req && req->type == JSON_OBJECT ? json_obj_get(req, "loop") : NULL;
    const json_t *queued = sa_rows(req, "queued");
    int columns = (int)json_get_num(req, "columns", 0);
    int rows = (int)json_get_num(req, "rows", 0);

    if (!sa_row_count(entries) && !sa_row_count(processes) && !sa_truthy(goal) && !sa_truthy(loop) &&
        !sa_row_count(queued))
        return sa_dup("");
    if (json_get_bool(req, "collapsed", 0)) return collapsed_dock(req, columns);

    columns = sa_imax(0, columns - 2);
    {
        lines_t lines;
        int budget, agent_budget, agent_count, proc_count;
        lines_init(&lines);
        if (sa_truthy(goal)) {
            sb_t t;
            sb_init(&t);
            sb_putc(&t, ' ');
            sa_py_str_into(&t, goal);
            lines_add(&lines, clip_text(t.p, columns));
            free(sb_take(&t));
        }
        if (sa_truthy(loop)) {
            sb_t t;
            sb_init(&t);
            sb_putc(&t, ' ');
            sa_py_str_into(&t, loop);
            lines_add(&lines, clip_text(t.p, columns));
            free(sb_take(&t));
        }
        budget = sa_imax(1, sa_imin(4, sa_pdiv(rows - 10, 3)));
        agent_budget = sa_row_count(processes) ? sa_imax(1, budget - sa_imax(1, budget / 2)) : budget;
        agent_count = sa_imin((int)sa_row_count(entries), agent_budget);
        if (sa_row_count(entries)) {
            int hidden = (int)sa_row_count(entries) - agent_count;
            int i;
            sb_t t;
            sb_init(&t);
            sb_putc(&t, ' ');
            sb_putf(&t, "Subagents " A_MID " %llu live " A_MID " Ctrl+T expand " A_MID " Ctrl+R collapse",
                    (unsigned long long)sa_row_count(entries));
            lines_add(&lines, clip_text(t.p, columns));
            free(sb_take(&t));
            for (i = 0; i < agent_count; i++) {
                const json_t *row = sa_row_at(entries, (size_t)i);
                char *agent_act = w23_clisubagen_f7c046_agent_activity(row);
                sb_t activity;
                int goal_width;
                char *goal_piece;
                sb_t line;
                sb_init(&activity);
                sa_py_str_into(&activity, row ? json_obj_get(row, "elapsed") : NULL);
                sb_puts(&activity, "s " A_MID " ");
                sb_puts(&activity, agent_act);
                goal_width = sa_imax(3, columns - sa_cwidth(activity.p) - 5);
                goal_piece = clip_node(row ? json_obj_get(row, "goal") : NULL, goal_width);
                sb_init(&line);
                sb_puts(&line, " " A_BULLET " ");
                sb_puts(&line, goal_piece);
                sb_puts(&line, A_SEP);
                sb_puts(&line, activity.p);
                lines_add(&lines, clip_text(line.p, columns));
                free(sb_take(&line));
                free(sb_take(&activity));
                free(goal_piece);
                free(agent_act);
            }
            if (hidden > 0) {
                sb_t more;
                sb_init(&more);
                sb_putc(&more, ' ');
                sb_putf(&more, "+%d more " A_MID " Ctrl+T all subagents", hidden);
                lines_add(&lines, clip_text(more.p, columns));
                free(sb_take(&more));
            }
        }
        if (sa_row_count(processes)) {
            size_t nproc = sa_row_count(processes), i;
            size_t running = 0;
            int done;
            proc_count = sa_imin((int)nproc, sa_imax(1, budget - agent_count));
            for (i = 0; i < nproc; i++)
                if (sa_status_running(sa_row_at(processes, i))) running++;
            done = (int)nproc - (int)running;
            {
                sb_t summary, t;
                sb_init(&summary);
                sb_init(&t);
                if (running) sb_putf(&summary, "%llu running", (unsigned long long)running);
                if (done) {
                    if (summary.n) sb_puts(&summary, A_SEP);
                    sb_putf(&summary, "%d done", done);
                }
                sb_putc(&t, ' ');
                sb_puts(&t, "Processes " A_MID " ");
                sb_puts(&t, summary.p);
                if (!sa_row_count(entries))
                    sb_puts(&t, " " A_MID " Ctrl+T expand " A_MID " Ctrl+R collapse");
                lines_add(&lines, clip_text(t.p, columns));
                free(sb_take(&summary));
                free(sb_take(&t));
            }
            for (i = 0; i < (size_t)proc_count; i++) {
                const json_t *row = sa_row_at(processes, i);
                char *activity = w23_clisubagen_f7c046_process_activity(row);
                int command_width = sa_imax(3, columns - sa_cwidth(activity) - 5);
                const json_t *status = row ? json_obj_get(row, "status") : NULL;
                const char *st = status && status->type == JSON_STRING ? status->str_val : "?";
                const char *glyph = A_CROSS;
                char *cmd;
                sb_t line;
                if (strcmp(st, "running") == 0) glyph = A_GEAR;
                else if (strcmp(st, "done") == 0) glyph = A_CHECK;
                else if (strcmp(st, "lost") == 0) glyph = "?";
                cmd = clip_node(row ? json_obj_get(row, "command") : NULL, command_width);
                sb_init(&line);
                sb_putc(&line, ' ');
                sb_puts(&line, glyph);
                sb_putc(&line, ' ');
                sb_puts(&line, cmd);
                sb_puts(&line, A_SEP);
                sb_puts(&line, activity);
                lines_add(&lines, clip_text(line.p, columns));
                free(sb_take(&line));
                free(cmd);
                free(activity);
            }
            if ((int)nproc > proc_count) {
                sb_t more;
                sb_init(&more);
                sb_putc(&more, ' ');
                sb_putf(&more, "+%d more " A_MID " Ctrl+T all processes", (int)nproc - proc_count);
                lines_add(&lines, clip_text(more.p, columns));
                free(sb_take(&more));
            }
        }
        if (sa_row_count(queued)) {
            size_t nq = sa_row_count(queued);
            int shown = (int)nq;
            int limit = rows >= 24 ? W23_CLISUBAGEN_F7C046_QUEUE_ROWS : 1;
            int i;
            if (shown > limit) shown = limit;
            {
                sb_t t;
                sb_init(&t);
                sb_putc(&t, ' ');
                sb_putf(&t, "Queue " A_MID " %llu queued " A_MID " /queue list", (unsigned long long)nq);
                if (!sa_row_count(entries) && !sa_row_count(processes))
                    sb_puts(&t, " " A_MID " Ctrl+R collapse");
                lines_add(&lines, clip_text(t.p, columns));
                free(sb_take(&t));
            }
            for (i = 0; i < shown; i++) {
                sb_t t;
                sb_init(&t);
                sb_puts(&t, "  ");
                sb_putf(&t, "%d. ", i + 1);
                sa_py_str_into(&t, sa_row_at(queued, (size_t)i));
                lines_add(&lines, clip_text(t.p, columns));
                free(sb_take(&t));
            }
            if ((int)nq > shown) {
                sb_t more;
                sb_init(&more);
                sb_puts(&more, "  ");
                sb_putf(&more, "+%d more", (int)nq - shown);
                lines_add(&lines, clip_text(more.p, columns));
                free(sb_take(&more));
            }
        }
        {
            sb_t out;
            size_t i;
            sb_init(&out);
            for (i = 0; i < lines.n; i++) {
                if (i) sb_putc(&out, '\n');
                sb_putc(&out, ' ');
                sb_puts(&out, lines.items[i]);
            }
            lines_free(&lines);
            return sb_take(&out);
        }
    }
}

/* ------------------------------------------------------------------ */
/* select / view / modal guard / transcript tail                       */
/* ------------------------------------------------------------------ */

json_t *w23_clisubagen_f7c046_select(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    const char *selected = req && req->type == JSON_OBJECT ? json_get_str(req, "selected_id", NULL) : NULL;
    int delta = (int)json_get_num(req, "delta", 0);
    size_t n_entries = sa_row_count(entries);
    size_t total = n_entries + sa_row_count(processes);
    size_t i, index = 0, target;
    int found = 0;

    if (!total) {
        if (selected) return json_string(selected);
        return json_null();
    }
    for (i = 0; i < n_entries && !found; i++) {
        const char *k = sa_key_of(sa_row_at(entries, i));
        if (k && selected && strcmp(k, selected) == 0) {
            index = i;
            found = 1;
        }
    }
    for (i = 0; i < sa_row_count(processes) && !found; i++) {
        const char *k = sa_key_of(sa_row_at(processes, i));
        if (k && selected && strcmp(k, selected) == 0) {
            index = n_entries + i;
            found = 1;
        }
    }
    if (!found) index = 0;
    {
        long long moved = (long long)index + (long long)delta;
        long long mod = (long long)total;
        long long wrapped = moved % mod;
        const json_t *row;
        const char *key;
        if (wrapped < 0) wrapped += mod;
        target = (size_t)wrapped;
        row = target < n_entries ? sa_row_at(entries, target)
                                 : sa_row_at(processes, target - n_entries);
        key = sa_key_of(row);
        return key ? json_string(key) : json_null();
    }
}

json_t *w23_clisubagen_f7c046_view(const json_t *req) {
    const json_t *entries = sa_rows(req, "entries");
    const json_t *processes = sa_rows(req, "processes");
    const json_t *goal = req && req->type == JSON_OBJECT ? json_obj_get(req, "goal") : NULL;
    const json_t *loop = req && req->type == JSON_OBJECT ? json_obj_get(req, "loop") : NULL;
    const json_t *queued = sa_rows(req, "queued");
    const char *selected = req && req->type == JSON_OBJECT ? json_get_str(req, "selected_id", NULL) : NULL;
    json_t *o = json_object();
    json_t *roster = json_array();
    json_t *sel = NULL;
    int sel_is_process = 0;
    size_t i;

    for (i = 0; i < sa_row_count(entries); i++) {
        const char *k = sa_key_of(sa_row_at(entries, i));
        json_append(roster, k ? json_string(k) : json_null());
        if (k && selected && strcmp(k, selected) == 0) {
            if (sel) json_free(sel);
            sel = json_string(k);
            sel_is_process = 0;
        }
    }
    for (i = 0; i < sa_row_count(processes); i++) {
        const json_t *row = sa_row_at(processes, i);
        const char *k = sa_key_of(row);
        json_append(roster, k ? json_string(k) : json_null());
        if (k && selected && strcmp(k, selected) == 0) {
            const json_t *kind = json_obj_get(row, "kind");
            if (sel) json_free(sel);
            sel = json_string(k);
            sel_is_process =
                kind && kind->type == JSON_STRING && kind->str_val && strcmp(kind->str_val, "process") == 0;
        }
    }
    json_set(o, "has_rows",
             json_bool(sa_row_count(entries) || sa_row_count(processes) || sa_truthy(goal) ||
                       sa_truthy(loop) || sa_row_count(queued)));
    json_set(o, "roster", roster);
    json_set(o, "selected", sel ? sel : json_null());
    json_set(o, "selected_is_process", json_bool(sel_is_process));
    return o;
}

int w23_clisubagen_f7c046_modal_prompt_active(const json_t *attrs) {
    static const char *names[] = {
        "_clarify_state", "_approval_state", "_slash_confirm_state", "_sudo_state",
        "_secret_state", "_connection_state", "_model_picker_state", "_command_palette_state"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        const json_t *v = attrs && attrs->type == JSON_OBJECT ? json_obj_get(attrs, names[i]) : NULL;
        if (sa_truthy(v)) return 1;
    }
    return 0;
}

char *w23_clisubagen_f7c046_read_tail(const char *path) {
    static const char *unavailable = "Live transcript not available yet.";
    FILE *fh;
    long size;
    char *buf;
    size_t got;
    sb_t out;

    if (!path || !path[0]) return sa_dup(unavailable);
    fh = fopen(path, "rb");
    if (!fh) return sa_dup(unavailable);
    if (fseek(fh, 0, SEEK_END) != 0) {
        fclose(fh);
        return sa_dup(unavailable);
    }
    size = ftell(fh);
    if (size < 0) size = 0;
    if (fseek(fh, size > 32768 ? size - 32768 : 0, SEEK_SET) != 0) {
        fclose(fh);
        return sa_dup(unavailable);
    }
    buf = (char *)malloc(32769);
    if (!buf) {
        fclose(fh);
        return sa_dup(unavailable);
    }
    got = fread(buf, 1, 32768, fh);
    fclose(fh);
    buf[got] = '\0';

    sb_init(&out);
    {
        size_t i = 0, n = got;
        while (i < n) {
            size_t adv = 0;
            unsigned cp = sa_u8_dec((const unsigned char *)buf + i, n - i, &adv);
            if (cp == '\n' || cp == '\t' || sa_printable_cp(cp)) {
                if (cp == 0xFFFD) sa_u8_enc(&out, cp);
                else sb_putn(&out, buf + i, adv ? adv : 1);
            }
            i += adv ? adv : 1;
        }
    }
    free(buf);
    return sb_take(&out);
}

json_t *w23_clisubagen_f7c046_consts(void) {
    json_t *o = json_object();
    json_set(o, "queue_rows", json_number((double)W23_CLISUBAGEN_F7C046_QUEUE_ROWS));
    return o;
}
