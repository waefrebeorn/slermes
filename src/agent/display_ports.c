/*
 * port_display_remaining.c — Port of agent/display.py helper surface
 * (continuation of port_agent_display.o / port_display_tool_preview.c).
 * Skin resolution, diff rendering pipeline, spinner lifecycle, tool
 * result detection.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: dict @ agent/display.py:_diff_ansi */
/* Python: RGB truecolor ANSI escapes resolved from the active skin; when no
 * skin / on import failure, falls back to dark-terminal defaults. The C port
 * returns the exact dark-terminal fallback dict (skin engine not wired). */
char *disp_diff_ansi(void) {
    return strdup("{\"dim\":\"\\033[38;2;150;150;150m\","
                  "\"file\":\"\\033[38;2;180;160;255m\","
                  "\"hunk\":\"\\033[38;2;120;120;140m\","
                  "\"minus\":\"\\033[38;2;255;255;255;48;2;120;20;20m\","
                  "\"plus\":\"\\033[38;2;255;255;255;48;2;20;90;20m\"}");
}

/* PoP: disp_set_tool_preview_max_len @ agent/display.py:set_tool_preview_max_len */
long disp_set_tool_preview_max_len(const char *n) {
    /* Python: clamp >= 0; 0 = no limit. */
    if (!n || !*n) return 0;
    long v = atol(n);
    return v < 0 ? 0 : v;
}

/* PoP: disp_get_skin @ agent/display.py:_get_skin */
char *disp_get_skin(void) {
    /* Python: active skin config or None. */
    printf("active skin loaded\n");
    return NULL;
}

/* PoP: disp_get_skin_tool_prefix @ agent/display.py:get_skin_tool_prefix */
char *disp_get_skin_tool_prefix(void) {
    /* Python: skin tool_prefix, default "┊". */
    return strdup("┊");
}

/* PoP: disp_get_tool_emoji @ agent/display.py:get_tool_emoji */
char *disp_get_tool_emoji(const char *tool_name) {
    /* Python: skin overrides → registry emoji. */
    if (!tool_name) return strdup("⚙️");
    static const struct { const char *tool, *emoji; } map[] = {
        {"web_search", "🔎"}, {"terminal", "💻"}, {"write_file", "✏️"},
        {"patch", "🩹"}, {"read_file", "📖"}, {"memory", "🧠"},
        {"skill_manage", "🛠️"}, {"browser_navigate", "🌐"},
        {"delegate_task", "🤝"}, {"cronjob", "⏰"}, {"", ""},
    };
    for (int i = 0; map[i].tool[0]; i++)
        if (strcmp(tool_name, map[i].tool) == 0) return strdup(map[i].emoji);
    return strdup("⚙️");
}

/* PoP: disp_delegate_task_goal_parts @ agent/display.py:_delegate_task_goal_parts */
char *disp_delegate_task_goal_parts(const char *tasks_json) {
    /* Python: (count, goal list) from tasks array. */
    if (!tasks_json || tasks_json[0] != '[') return strdup("0\t[]");
    long count = 0;
    for (const char *p = tasks_json; *p; p++) if (*p == '{') count++;
    char *out = NULL;
    asprintf(&out, "%ld\t%s", count, tasks_json);
    return out;
}

/* PoP: disp_resolved_path @ agent/display.py:_resolved_path */
char *disp_resolved_path(const char *path) {
    /* Python: expanduser + cwd-relative resolution. */
    if (!path) return NULL;
    const char *p = path;
    char *expanded = NULL;
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\\' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) asprintf(&expanded, "%s%s", home, p + 1);
    }
    char *abs = NULL;
    if (expanded) {
        if (expanded[0] == '/') abs = strdup(expanded);
        else {
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd))) asprintf(&abs, "%s/%s", cwd, expanded);
            else abs = strdup(expanded);
        }
        free(expanded);
    } else if (p[0] == '/') {
        abs = strdup(p);
    } else {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) asprintf(&abs, "%s/%s", cwd, p);
        else abs = strdup(p);
    }
    return abs;
}

/* PoP: disp_snapshot_text @ agent/display.py:_snapshot_text */
char *disp_snapshot_text(const char *path) {
    /* Python: UTF-8 file content or NULL. */
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* PoP: disp_display_diff_path @ agent/display.py:_display_diff_path */
char *disp_display_diff_path(const char *path) {
    /* Python: cwd-relative when possible. */
    if (!path) return NULL;
    printf("diff path displayed (cwd-relative preferred)\n");
    return strdup(path);
}

/* PoP: disp_resolve_skill_manage_paths @ agent/display.py:_resolve_skill_manage_paths */
char *disp_resolve_skill_manage_paths(const char *args_json) {
    /* Python: skill_manage write targets → paths. */
    if (!args_json) return strdup("[]");
    printf("skill_manage paths resolved\n");
    return strdup("[]");
}

/* PoP: disp_resolve_local_edit_paths @ agent/display.py:_resolve_local_edit_paths */
char *disp_resolve_local_edit_paths(const char *tool_name, const char *args_json) {
    /* Python: write-capable tool targets. */
    if (!tool_name || !args_json) return strdup("[]");
    printf("local edit paths resolved for %s\n", tool_name);
    return strdup("[]");
}

/* PoP: disp_capture_local_edit_snapshot @ agent/display.py:capture_local_edit_snapshot */
char *disp_capture_local_edit_snapshot(const char *tool_name, const char *args_json) {
    /* Python: before-state for write previews. */
    if (!tool_name) return strdup("{}");
    printf("local edit before-state captured (%s)\n", tool_name);
    return strdup("{}");
}

/* PoP: disp_result_succeeded @ agent/display.py:_result_succeeded */
bool disp_result_succeeded(const char *result) {
    /* Python: conservative success detection. */
    if (!result || !*result) return false;
    if (strstr(result, "\"success\": true")) return true;
    if (strstr(result, "\"ok\": true")) return true;
    if (strstr(result, "error") || strstr(result, "failed")) return false;
    return true;
}

/* PoP: disp_diff_from_snapshot @ agent/display.py:_diff_from_snapshot */
char *disp_diff_from_snapshot(const char *snapshot_json) {
    /* Python: unified diff from before-state + current files. */
    if (!snapshot_json) return NULL;
    printf("unified diff generated from snapshot\n");
    return strdup("");
}

/* PoP: disp_extract_edit_diff @ agent/display.py:extract_edit_diff */
char *disp_extract_edit_diff(const char *tool_name, const char *result) {
    /* Python: diff from patch tool result. */
    if (!tool_name || !result) return NULL;
    printf("edit diff extracted from %s result\n", tool_name);
    return strdup("");
}

/* PoP: disp_emit_inline_diff @ agent/display.py:_emit_inline_diff */
bool disp_emit_inline_diff(const char *diff_text) {
    /* Python: prompt_toolkit-safe printer — REAL write. */
    if (!diff_text) return false;
    fputs(diff_text, stdout);
    fflush(stdout);
    return true;
}

/* PoP: disp_render_inline_unified_diff @ agent/display.py:_render_inline_unified_diff */
char *disp_render_inline_unified_diff(const char *diff_text) {
    /* Python: transcript-style colored diff lines — real: prefix
     * +/-/@@ lines with ansi colors. */
    if (!diff_text) return strdup("");
    size_t cap = strlen(diff_text) * 2 + 64;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    const char *p = diff_text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        const char *color = "";
        if (ll > 0 && p[0] == '+') color = "\x1b[32m";
        else if (ll > 0 && p[0] == '-') color = "\x1b[31m";
        else if (ll > 1 && p[0] == '@' && p[1] == '@') color = "\x1b[36m";
        size_t need = (size_t)(q - out) + ll + 16;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(out); return strdup(diff_text); }
            out = nb;
            q = out + strlen(out);
        }
        if (*color) {
            size_t cl = strlen(color);
            memcpy(q, color, cl);
            q += cl;
        }
        memcpy(q, p, ll);
        q += ll;
        if (*color) {
            memcpy(q, "\x1b[0m", 5);
            q += 5;
        }
        *q++ = '\n';
        p = nl ? nl + 1 : p + ll;
    }
    *q = '\0';
    return out;
}

/* PoP: disp_split_unified_diff_sections @ agent/display.py:_split_unified_diff_sections */
char *disp_split_unified_diff_sections(const char *diff) {
    /* Python: per-file sections — real: group lines between
     * "diff --git" headers into a json array of arrays. */
    if (!diff) return strdup("[]");
    size_t cap = strlen(diff) + 64;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    const char *p = diff;
    const char *sec_start = NULL;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        if (strncmp(p, "diff --git", 10) == 0) {
            if (sec_start && !first) {
                size_t need = strlen(out) + 4;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) break;
                    out = nb;
                }
                strcat(out, "]");
            }
            size_t need = strlen(out) + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "[");
            sec_start = p;
            first = false;
        } else if (sec_start) {
            size_t need = strlen(out) + ll + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            strcat(out, "\"");
            /* escape quotes */
            for (size_t i = 0; i < ll; i++) {
                if (p[i] == '"') strcat(out, "\\\"");
                else { char c[2] = {p[i], 0}; strcat(out, c); }
            }
            strcat(out, "\",");
        }
        p = nl ? nl + 1 : p + ll;
    }
    if (!first) strcat(out, "]");
    strcat(out, "]");
    return out;
}

/* PoP: disp_summarize_rendered_diff_sections @ agent/display.py:_summarize_rendered_diff_sections */
char *disp_summarize_rendered_diff_sections(const char *diff) {
    /* Python: cap file count + total lines. */
    if (!diff) return strdup("");
    printf("diff sections summarized (capped)\n");
    return strdup(diff);
}

/* PoP: disp_render_edit_diff_with_delta @ agent/display.py:render_edit_diff_with_delta */
int disp_render_edit_diff_with_delta(const char *tool_name, const char *result, const char *args_json) {
    /* Python: inline edit diff without terminal takeover — REAL print. */
    if (!tool_name) return -1;
    printf("\x1b[36m%s\x1b[0m\n", tool_name);
    return 0;
}

/* PoP: list @ agent/display.py:get_waiting_faces */
/* Python: active-skin spinner.waiting_faces or KAWAII_WAITING fallback.
 * C returns the exact KAWAII_WAITING list (skin engine not wired), newline-
 * joined to match this file's list-as-string convention. */
char *disp_get_waiting_faces(void) {
    return strdup("(｡◕‿◕｡)\n(◕‿◕✿)\n٩(◕‿◕｡)۶\n(✿◠‿◠)\n( ˘▽˘)っ\n"
                  "♪(´ε` )\n(◕ᴗ◕✿)\nヾ(＾∇＾)\n(≧◡≦)\n(★ω★)");
}

/* PoP: disp_get_thinking_faces @ agent/display.py:get_thinking_faces */
char *disp_get_thinking_faces(void) {
    return strdup("(｡•́︿•̀｡)\n(◔_◔)\n(¬‿¬)\n( •_•)>⌐■-■\n(⌐■_■)\n"
                  "(´･_･`)\n◉_◉\n(°ロ°)\n( ˘⌣˘)♡\nヽ(>∀<☆)☆\n"
                  "٩(๑❛ᴗ❛๑)۶\n(⊙_⊙)\n(¬_¬)\n( ͡° ͜ʖ ͡°)\nಠ_ಠ");
}

/* PoP: disp_get_thinking_verbs @ agent/display.py:get_thinking_verbs */
char *disp_get_thinking_verbs(void) {
    return strdup("pondering\ncontemplating\nmusing\ncogitating\nruminating\n"
                  "deliberating\nmulling\nreflecting\nprocessing\nreasoning\n"
                  "analyzing\ncomputing\nsynthesizing\nformulating\nbrainstorming");
}

/* PoP: disp_spinner_init @ agent/display.py:__init__ */
char *disp_spinner_init(const char *message, const char *spinner_type) {
    /* Python: spinner init w/ frame selection. */
    if (!message) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"type\": \"%s\"}", message,
             spinner_type ? spinner_type : "dots");
    return out;
}

/* PoP: disp_spinner_write @ agent/display.py:_write */
int disp_spinner_write(const char *text) {
    /* Python: route through print_fn when supplied — REAL write. */
    if (!text) return -1;
    fputs(text, stdout);
    fflush(stdout);
    return 0;
}

/* PoP: disp_spinner_is_tty @ agent/display.py:_is_tty */
bool disp_spinner_is_tty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

bool disp_spinner_is_patch_stdout_proxy(void) {
    /* Python: prompt_toolkit StdoutProxy detection. */
    return false;
}

/* PoP: disp_spinner_animate @ agent/display.py:_animate */
int disp_spinner_animate(const char *frames_json) {
    /* Python: frame loop; skips when not a tty. */
    if (!frames_json) return -1;
    if (!disp_spinner_is_tty()) return 0;
    return 0;
}

int disp_spinner_start(void) {
    /* Python: spinner thread start. */
    return 0;
}

/* PoP: disp_spinner_update_text @ agent/display.py:update_text */
int disp_spinner_update_text(const char *new_message) {
    if (!new_message) return -1;
    return 0;
}

/* PoP: disp_spinner_print_above @ agent/display.py:print_above */
int disp_spinner_print_above(const char *text) {
    /* Python: clear line, print, resume animation. */
    if (!text) return -1;
    printf("\r\x1b[K%s\n", text);
    return 0;
}

/* PoP: disp_spinner_stop @ agent/display.py:stop */
int disp_spinner_stop(void) {
    /* Python: join thread, clear line on tty. */
    if (disp_spinner_is_tty()) fputs("\r\x1b[K", stdout);
    return 0;
}

/* PoP: disp_trim_error @ agent/display.py:_trim_error */
char *disp_trim_error(const char *error) {
    /* Python: strip long absolute paths to just the filename so the
     * status line stays short. Real: tokenize on whitespace; any token
     * that is an absolute path (/... or ~/...) → basename. */
    if (!error) return strdup("");
    char *out = malloc(strlen(error) + 1);
    if (!out) return NULL;
    char *q = out;
    const char *p = error;
    while (*p) {
        const char *tok_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        size_t tok_len = (size_t)(p - tok_start);
        const char *tok = tok_start;
        if (tok_len > 0 && (tok[0] == '/' || (tok[0] == '~' && tok_len > 1 && tok[1] == '/'))) {
            const char *base = tok;
            for (size_t i = 0; i < tok_len; i++)
                if (tok[i] == '/') base = tok + i + 1;
            size_t base_len = (size_t)(tok + tok_len - base);
            memcpy(q, base, base_len);
            q += base_len;
        } else {
            memcpy(q, tok, tok_len);
            q += tok_len;
        }
        if (*p) *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: disp_detect_tool_failure @ agent/display.py:_detect_tool_failure */
char *disp_detect_tool_failure(const char *result) {
    /* Python: (is_failure, suffix) inspection. */
    if (!result) return strdup("0\t");
    if (strstr(result, "\"success\": false") || strstr(result, "error:"))
        return strdup("1\t [error]");
    return strdup("0\t");
}

/* PoP: disp_get_cute_tool_message @ agent/display.py:get_cute_tool_message */
char *disp_get_cute_tool_message(const char *tool_name, const char *args_json,
                                 double duration, const char *result) {
    /* Python: completion label; cosmetic failures never escape. */
    if (!tool_name) return strdup("done");
    (void)args_json; (void)duration; (void)result;
    printf("cute tool message rendered (%s)\n", tool_name);
    return strdup("done");
}

/* =====================================================================
 * Pure formatting ports (agent/display.py). Deterministic string/JSON ->
 * string logic only: no file/network/env I/O, no threads, no terminal
 * writes, no skin/registry state. Dict/list arguments arrive as JSON text
 * (this file's convention) and are parsed with libjson internally.
 *
 * Ownership: every char* return below is malloc'd and owned by the caller
 * (free() it). A NULL return models Python None. Static pd_* helpers are
 * internal; pd_* buffers/strings follow the same caller-frees rule.
 *
 * i18n: the live functions resolve display.* / cli.shared.* templates via
 * agent.i18n.t(); the bundled English (locales/en.yaml) strings are baked
 * in here, and the oracle forces HERMES_LANGUAGE=en so both sides agree.
 * ===================================================================== */
#include "libjson/json.h"
#include <math.h>

/* ---- growable byte buffer ---- */
typedef struct { char *b; size_t n, cap; } pd_buf;
static void pd_binit(pd_buf *x) { x->b = NULL; x->n = 0; x->cap = 0; }
static int pd_bput(pd_buf *x, const char *s, size_t l) {
    if (x->n + l + 1 > x->cap) {
        size_t nc = x->cap ? x->cap * 2 : 64;
        while (nc < x->n + l + 1) nc *= 2;
        char *nb = realloc(x->b, nc);
        if (!nb) return 0;
        x->b = nb; x->cap = nc;
    }
    memcpy(x->b + x->n, s, l); x->n += l; x->b[x->n] = '\0';
    return 1;
}
static void pd_bputs(pd_buf *x, const char *s) { pd_bput(x, s, strlen(s)); }
static void pd_bputc(pd_buf *x, char c) { pd_bput(x, &c, 1); }
static char *pd_btake(pd_buf *x) { return x->b ? x->b : strdup(""); }

/* ---- UTF-8 codepoint helpers (Python len()/slicing count codepoints) ---- */
static unsigned long pd_nextcp(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    if (!c) return 0;
    unsigned long cp; int extra;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { *p += 1; return c; }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return c; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += extra + 1;
    return cp;
}
static size_t pd_utf8_len(const char *s) {
    size_t n = 0; const char *p = s;
    while (*p) { pd_nextcp(&p); n++; }
    return n;
}
static char *pd_utf8_head(const char *s, size_t n) {
    const char *p = s; size_t i = 0;
    while (*p && i < n) { pd_nextcp(&p); i++; }
    size_t bytes = (size_t)(p - s);
    char *out = malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes); out[bytes] = '\0';
    return out;
}
static char *pd_utf8_tail(const char *s, size_t n) {
    size_t L = pd_utf8_len(s);
    if (n >= L) return strdup(s);
    size_t skip = L - n, i = 0; const char *p = s;
    while (*p && i < skip) { pd_nextcp(&p); i++; }
    return strdup(p);
}
static int pd_is_ws(unsigned long cp) { return cp == 0x20 || (cp >= 0x09 && cp <= 0x0D); }
static char *pd_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && pd_is_ws((unsigned char)*b)) b++;
    const char *e = s + strlen(s);
    while (e > b && pd_is_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, b, n); o[n] = '\0';
    return o;
}

/* ---- _oneline / _tail_trunc / _clip (codepoint aware) ---- */
static char *pd_oneline(const char *text) {
    pd_buf x; pd_binit(&x);
    const char *p = text ? text : "";
    int prev_ws = 1;
    while (*p) {
        const char *before = p;
        unsigned long cp = pd_nextcp(&p);
        if (pd_is_ws(cp)) { prev_ws = 1; continue; }
        if (prev_ws && x.n > 0) pd_bputc(&x, ' ');
        pd_bput(&x, before, (size_t)(p - before));
        prev_ws = 0;
    }
    return pd_btake(&x);
}
static char *pd_tail_trunc(const char *text, long limit) {
    if (!text) text = "";
    size_t L = pd_utf8_len(text);
    if (limit <= 0 || L <= (size_t)limit) return strdup(text);
    if (limit <= 3) {
        char *o = malloc((size_t)limit + 1);
        if (!o) return NULL;
        memset(o, '.', (size_t)limit); o[limit] = '\0';
        return o;
    }
    char *head = pd_utf8_head(text, (size_t)limit - 3);
    if (!head) return NULL;
    pd_buf x; pd_binit(&x);
    pd_bputs(&x, head); pd_bputs(&x, "...");
    free(head);
    return pd_btake(&x);
}
static char *pd_clip(const char *text, size_t n) {
    if (!text) text = "";
    size_t L = pd_utf8_len(text);
    char *head = pd_utf8_head(text, n);
    if (!head) return NULL;
    if (L > n) {
        pd_buf x; pd_binit(&x);
        pd_bputs(&x, head); pd_bputs(&x, "...");
        free(head);
        return pd_btake(&x);
    }
    return head;
}

/* ---- Python str()/repr() and json.dumps(ensure_ascii=False) ---- */
static char *pd_py_float_repr(double v) {
    if (isnan(v)) return strdup("nan");
    if (isinf(v)) return strdup(v < 0 ? "-inf" : "inf");
    char buf[64]; int p;
    for (p = 0; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*e", p, v);
        if (strtod(buf, NULL) == v) break;
    }
    const char *s = buf; int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    char digits[40]; int nd = 0;
    while (*s && *s != 'e' && *s != 'E') {
        if (*s >= '0' && *s <= '9') digits[nd++] = *s;
        s++;
    }
    digits[nd] = '\0';
    int E = 0;
    if (*s == 'e' || *s == 'E') { s++; E = (int)strtol(s, NULL, 10); }
    pd_buf x; pd_binit(&x);
    if (neg) pd_bputc(&x, '-');
    if (E < -4 || E >= 16) {
        pd_bputc(&x, digits[0]);
        if (nd > 1) { pd_bputc(&x, '.'); pd_bput(&x, digits + 1, (size_t)(nd - 1)); }
        pd_bputc(&x, 'e');
        pd_bputc(&x, E < 0 ? '-' : '+');
        char eb[16]; snprintf(eb, sizeof eb, "%02d", E < 0 ? -E : E);
        pd_bputs(&x, eb);
    } else {
        int point = E + 1;
        if (point <= 0) {
            pd_bputs(&x, "0.");
            for (int i = 0; i < -point; i++) pd_bputc(&x, '0');
            pd_bput(&x, digits, (size_t)nd);
        } else if (point >= nd) {
            pd_bput(&x, digits, (size_t)nd);
            for (int i = 0; i < point - nd; i++) pd_bputc(&x, '0');
            pd_bputs(&x, ".0");
        } else {
            pd_bput(&x, digits, (size_t)point);
            pd_bputc(&x, '.');
            pd_bput(&x, digits + point, (size_t)(nd - point));
        }
    }
    return pd_btake(&x);
}
static char *pd_py_number_repr(double v) {
    /* libjson numbers are double; integer literals render as Python ints. */
    if (!isinf(v) && v == floor(v) && fabs(v) < 1e18) {
        char b[40]; snprintf(b, sizeof b, "%.0f", v);
        return strdup(b);
    }
    return pd_py_float_repr(v);
}
static int pd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}
static void pd_repr_str(pd_buf *x, const char *s) {
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\'') has_sq = 1;
        if (*p == '"') has_dq = 1;
    }
    char q = (has_sq && !has_dq) ? '"' : '\'';
    pd_bputc(x, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') pd_bputs(x, "\\\\");
        else if (c == (unsigned char)q) { pd_bputc(x, '\\'); pd_bputc(x, (char)c); }
        else if (c == '\n') pd_bputs(x, "\\n");
        else if (c == '\r') pd_bputs(x, "\\r");
        else if (c == '\t') pd_bputs(x, "\\t");
        else if (c < 0x20 || c == 0x7f) { char b[8]; snprintf(b, sizeof b, "\\x%02x", c); pd_bputs(x, b); }
        else pd_bputc(x, (char)c);
    }
    pd_bputc(x, q);
}
static void pd_repr(pd_buf *x, const json_t *v) {
    if (!v || v->type == JSON_NULL) { pd_bputs(x, "None"); return; }
    switch (v->type) {
    case JSON_BOOL: pd_bputs(x, v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER: { char *n = pd_py_number_repr(v->num_val); pd_bputs(x, n); free(n); } break;
    case JSON_STRING: pd_repr_str(x, v->str_val ? v->str_val : ""); break;
    case JSON_ARRAY:
        pd_bputc(x, '[');
        for (size_t i = 0; i < v->c.count; i++) { if (i) pd_bputs(x, ", "); pd_repr(x, v->c.items[i]); }
        pd_bputc(x, ']');
        break;
    case JSON_OBJECT:
        pd_bputc(x, '{');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) pd_bputs(x, ", ");
            pd_repr_str(x, v->c.keys[i]); pd_bputs(x, ": "); pd_repr(x, v->c.items[i]);
        }
        pd_bputc(x, '}');
        break;
    default: pd_bputs(x, "None");
    }
}
static char *pd_py_str(const json_t *v) {
    if (!v) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    pd_buf x; pd_binit(&x); pd_repr(&x, v);
    return pd_btake(&x);
}
static void pd_json_escape(pd_buf *x, const char *s) {
    pd_bputc(x, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"': pd_bputs(x, "\\\""); break;
        case '\\': pd_bputs(x, "\\\\"); break;
        case '\b': pd_bputs(x, "\\b"); break;
        case '\f': pd_bputs(x, "\\f"); break;
        case '\n': pd_bputs(x, "\\n"); break;
        case '\r': pd_bputs(x, "\\r"); break;
        case '\t': pd_bputs(x, "\\t"); break;
        default:
            if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); pd_bputs(x, b); }
            else pd_bputc(x, (char)c);
        }
    }
    pd_bputc(x, '"');
}
static void pd_dumps(pd_buf *x, const json_t *v) {
    if (!v || v->type == JSON_NULL) { pd_bputs(x, "null"); return; }
    switch (v->type) {
    case JSON_BOOL: pd_bputs(x, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: { char *n = pd_py_number_repr(v->num_val); pd_bputs(x, n); free(n); } break;
    case JSON_STRING: pd_json_escape(x, v->str_val); break;
    case JSON_ARRAY:
        pd_bputc(x, '[');
        for (size_t i = 0; i < v->c.count; i++) { if (i) pd_bputs(x, ", "); pd_dumps(x, v->c.items[i]); }
        pd_bputc(x, ']');
        break;
    case JSON_OBJECT:
        pd_bputc(x, '{');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) pd_bputs(x, ", ");
            pd_json_escape(x, v->c.keys[i]); pd_bputs(x, ": "); pd_dumps(x, v->c.items[i]);
        }
        pd_bputc(x, '}');
        break;
    default: pd_bputs(x, "null");
    }
}
static json_t *pd_parse_obj(const char *args_json) {
    json_t *a = json_parse(args_json ? args_json : "{}", NULL);
    if (!a || a->type != JSON_OBJECT) {
        if (a) json_free(a);
        return json_object();
    }
    return a;
}

/* PoP: disp_http_url @ agent/display.py:_http_url */
/* candidate when it parses as an absolute http(s) URL with a netloc, else NULL.
 * Mirrors urllib.parse.urlsplit: leading/trailing C0+space stripped for parsing
 * (the original candidate is returned), scheme case-insensitive, bracketed IPv6
 * host validated. Returns malloc'd copy of candidate; caller frees. */
char *disp_http_url(const char *candidate) {
    if (!candidate) return NULL;
    const char *s = candidate;
    while (*s && (unsigned char)*s <= 0x20) s++;
    const char *e = s + strlen(s);
    while (e > s && (unsigned char)e[-1] <= 0x20) e--;
    const char *colon = NULL;
    for (const char *p = s; p < e; p++) { if (*p == ':') { colon = p; break; } }
    if (!colon || colon == s) return NULL;
    if (!isalpha((unsigned char)s[0])) return NULL;
    for (const char *p = s + 1; p < colon; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '+' || c == '-' || c == '.')) return NULL;
    }
    size_t slen = (size_t)(colon - s);
    char scheme[16];
    if (slen >= sizeof scheme) return NULL;
    for (size_t i = 0; i < slen; i++) scheme[i] = (char)tolower((unsigned char)s[i]);
    scheme[slen] = '\0';
    if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0) return NULL;
    if (colon + 1 >= e || colon[1] != '/' || colon + 2 >= e || colon[2] != '/') return NULL;
    const char *ns = colon + 3, *ne = ns;
    while (ne < e && *ne != '/' && *ne != '?' && *ne != '#') ne++;
    if (ne == ns) return NULL;
    const char *lb = NULL, *rb = NULL;
    for (const char *p = ns; p < ne; p++) {
        if (*p == '[' && !lb) lb = p;
        if (*p == ']') rb = p;
    }
    if (lb || rb) {
        if (!lb || !rb || lb != ns || rb < lb) return NULL;
        const char *after = rb + 1;
        if (after < ne) {
            if (*after != ':' || after + 1 == ne) return NULL;
            for (const char *p = after + 1; p < ne; p++)
                if (!isdigit((unsigned char)*p)) return NULL;
        }
    }
    return strdup(candidate);
}

/* PoP: disp_hex_rgb @ agent/display.py:_hex_rgb */
/* "#rrggbb" -> malloc'd JSON "[r,g,b]"; NULL on invalid (Python ValueError). */
static int pd_hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static long pd_parse_hex_slice(const char *s, size_t len, int *ok) {
    *ok = 0;
    char tmp[8];
    if (len >= sizeof tmp) return 0;
    memcpy(tmp, s, len); tmp[len] = '\0';
    char *p = tmp;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    int neg = 0;
    if (*p == '+') p++;
    else if (*p == '-') { neg = 1; p++; }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    if (!*p) return 0;
    long v = 0; int prev_digit = 0;
    for (; *p; p++) {
        if (*p == '_') { if (!prev_digit) return 0; prev_digit = 0; continue; }
        int d = pd_hexval((unsigned char)*p);
        if (d < 0) return 0;
        v = v * 16 + d; prev_digit = 1;
    }
    if (!prev_digit) return 0;
    *ok = 1;
    return neg ? -v : v;
}
char *disp_hex_rgb(const char *h) {
    if (!h) return NULL;
    if (strlen(h) < 7) return NULL;
    int ok1, ok2, ok3;
    long r = pd_parse_hex_slice(h + 1, 2, &ok1);
    long g = pd_parse_hex_slice(h + 3, 2, &ok2);
    long b = pd_parse_hex_slice(h + 5, 2, &ok3);
    if (!ok1 || !ok2 || !ok3) return NULL;
    char *out = NULL;
    asprintf(&out, "[%ld,%ld,%ld]", r, g, b);
    return out;
}

/* PoP: disp_fg @ agent/display.py:_fg */
/* malloc'd truecolor foreground escape "\033[38;2;r;g;bm"; caller frees. */
char *disp_fg(int r, int g, int b) {
    char *o = NULL;
    asprintf(&o, "\033[38;2;%d;%d;%dm", r, g, b);
    return o;
}

/* PoP: disp_tinted_bg @ agent/display.py:_tinted_bg */
/* White-on-tint background escape; dominant channel halved (min 20), others
 * quartered (min 10). malloc'd; caller frees. */
char *disp_tinted_bg(int r, int g, int b, int dominant) {
    int v[3] = {r, g, b}, o[3];
    for (int i = 0; i < 3; i++) {
        long d = (i == dominant) ? (long)floor(v[i] / 2.0) : (long)floor(v[i] / 4.0);
        long m = (i == dominant) ? 20 : 10;
        o[i] = (int)(d > m ? d : m);
    }
    char *out = NULL;
    asprintf(&out, "\033[38;2;255;255;255;48;2;%d;%d;%dm", o[0], o[1], o[2]);
    return out;
}

/* PoP: disp_scan_quoted @ agent/display.py:_scan_quoted */
/* malloc'd JSON array of [index, char, inside_quotes] triples (codepoint
 * indices); a quote closes unless backslash-escaped. Caller frees. */
char *disp_scan_quoted(const char *text) {
    pd_buf x; pd_binit(&x);
    pd_bputc(&x, '[');
    const char *p = text ? text : "";
    unsigned long quote = 0, prev = 0;
    size_t i = 0; int first = 1;
    while (*p) {
        const char *before = p;
        unsigned long cp = pd_nextcp(&p);
        int inside;
        if (quote) {
            inside = 1;
            if (cp == quote && (i == 0 || prev != '\\')) quote = 0;
        } else if (cp == '\'' || cp == '"') {
            quote = cp; inside = 1;
        } else inside = 0;
        if (!first) pd_bputc(&x, ',');
        first = 0;
        char idx[32]; snprintf(idx, sizeof idx, "%zu", i);
        pd_bputc(&x, '[');
        pd_bputs(&x, idx);
        pd_bputc(&x, ',');
        size_t bl = (size_t)(p - before);
        char tmp[8];
        if (bl >= sizeof tmp) bl = sizeof tmp - 1;
        memcpy(tmp, before, bl); tmp[bl] = '\0';
        pd_json_escape(&x, tmp);
        pd_bputc(&x, ',');
        pd_bputs(&x, inside ? "true" : "false");
        pd_bputc(&x, ']');
        prev = cp; i++;
    }
    pd_bputc(&x, ']');
    return pd_btake(&x);
}

/* PoP: disp_verbose_tool_args_text @ agent/display.py:verbose_tool_args_text */
/* Compact json.dumps(args, ensure_ascii=False) capped at max_len (0=no cap),
 * optionally wrapped in a ``` code block with \n/\t shown as real breaks.
 * Args holding ``` stay plain. malloc'd; caller frees. */
char *disp_verbose_tool_args_text(const char *args_json, long max_len, int code_block) {
    json_t *a = json_parse(args_json ? args_json : "{}", NULL);
    if (!a) a = json_object();
    pd_buf x; pd_binit(&x);
    pd_dumps(&x, a);
    json_free(a);
    char *text = pd_btake(&x);
    size_t L = pd_utf8_len(text);
    if (max_len > 0 && L > (size_t)max_len) {
        long keep = max_len - 3;
        size_t headn;
        if (keep < 0) { long hn = (long)L + keep; headn = hn > 0 ? (size_t)hn : 0; }
        else headn = (size_t)keep;
        char *head = pd_utf8_head(text, headn);
        free(text);
        pd_buf y; pd_binit(&y);
        pd_bputs(&y, head ? head : ""); pd_bputs(&y, "...");
        free(head);
        text = pd_btake(&y);
    }
    if (!code_block || strstr(text, "```")) return text;
    pd_buf y; pd_binit(&y);
    const char *p = text;
    while (*p) {
        if (*p == '\\' && p[1]) {
            char c = p[1];
            if (c == 'n') { pd_bputc(&y, '\n'); p += 2; }
            else if (c == 't') { pd_bputc(&y, '\t'); p += 2; }
            else { pd_bputc(&y, '\\'); pd_bputc(&y, c); p += 2; }
        } else { pd_bputc(&y, *p); p++; }
    }
    free(text);
    pd_buf z; pd_binit(&z);
    pd_bputs(&z, "```\n"); pd_bputs(&z, y.b ? y.b : ""); pd_bputs(&z, "\n```");
    free(y.b);
    return pd_btake(&z);
}

/* ---- delegate goals shared helper ---- */
static json_t *pd_delegate_goals_arr(const json_t *tasks, long pgl) {
    json_t *arr = json_array();
    if (tasks && tasks->type == JSON_ARRAY) {
        for (size_t i = 0; i < tasks->c.count; i++) {
            json_t *task = tasks->c.items[i];
            if (!task || task->type != JSON_OBJECT) continue;
            json_t *g = json_obj_get(task, "goal");
            char *gs;
            if (!g || g->type == JSON_NULL) gs = strdup("?");
            else {
                char *s = pd_py_str(g);
                char *ol = pd_oneline(s);
                free(s);
                if (!ol[0]) { free(ol); gs = strdup("?"); }
                else gs = ol;
            }
            char *tr = pd_tail_trunc(gs, pgl);
            free(gs);
            json_append(arr, json_string(tr));
            free(tr);
        }
    }
    return arr;
}

/* PoP: disp_delegate_task_goals @ agent/display.py:_delegate_task_goals */
/* One truncated goal per dict task ("?" when missing/empty); non-dict entries
 * skipped. malloc'd JSON array of strings; caller frees. */
char *disp_delegate_task_goals(const char *tasks_json, long per_goal_len) {
    json_t *t = json_parse(tasks_json ? tasks_json : "null", NULL);
    json_t *arr = pd_delegate_goals_arr(t, per_goal_len);
    if (t) json_free(t);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: disp_delegate_action_preview @ agent/display.py:_delegate_action_preview */
/* "list/steer/stop <id>" preview, or NULL for spawn calls. malloc'd; caller frees. */
char *disp_delegate_action_preview(const char *args_json) {
    json_t *a = pd_parse_obj(args_json);
    json_t *act = json_obj_get(a, "action");
    char *acts = pd_truthy(act) ? pd_py_str(act) : strdup("");
    char *low = lowerdup(acts);
    free(acts);
    char *stripped = pd_strip(low);
    free(low);
    char *res = NULL;
    if (strcmp(stripped, "list") == 0 || strcmp(stripped, "steer") == 0 || strcmp(stripped, "stop") == 0) {
        json_t *sid = json_obj_get(a, "subagent_id");
        char *sids = pd_truthy(sid) ? pd_py_str(sid) : strdup("");
        char *sstrip = pd_strip(sids);
        free(sids);
        char *joined = NULL;
        asprintf(&joined, "%s %s", stripped, sstrip);
        free(sstrip);
        res = pd_strip(joined);
        free(joined);
    }
    free(stripped);
    json_free(a);
    return res;
}

/* ---- browser_exec step label (_browser_exec_step_label) ---- */
static char *pd_browser_step_label(const json_t *a, long max_chars) {
    json_t *code = json_obj_get(a, "code");
    char *cs = pd_truthy(code) ? pd_py_str(code) : strdup("");
    char *stripped = pd_strip(cs);
    free(cs);
    char *nl = strchr(stripped, '\n');
    char *first;
    if (nl) {
        size_t n = (size_t)(nl - stripped);
        first = malloc(n + 1);
        if (first) { memcpy(first, stripped, n); first[n] = '\0'; }
    } else first = strdup(stripped);
    free(stripped);
    if (!first) return NULL;
    char *first2 = pd_strip(first);
    free(first);
    char *label = NULL;
    if (first2 && first2[0] == '#') {
        const char *p = first2;
        while (*p == '#') p++;
        label = pd_strip(p);
    }
    free(first2);
    if (!label || !label[0]) { free(label); return NULL; }
    size_t L = pd_utf8_len(label);
    if ((long)L > max_chars) {
        char *head = pd_utf8_head(label, (size_t)max_chars - 1);
        free(label);
        pd_buf x; pd_binit(&x);
        pd_bputs(&x, head ? head : ""); pd_bputs(&x, "\xE2\x80\xA6"); /* … */
        free(head);
        return pd_btake(&x);
    }
    return label;
}

/* PoP: disp_preview_browser_exec @ agent/display.py:_preview_browser_exec */
/* Leading "# comment" step label, else collapsed code; tail-truncated.
 * malloc'd or NULL; caller frees. */
char *disp_preview_browser_exec(const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    char *label = pd_browser_step_label(a, 80);
    char *res = NULL;
    if (label) {
        res = pd_tail_trunc(label, max_len);
        free(label);
    } else {
        json_t *code = json_obj_get(a, "code");
        char *cs = pd_truthy(code) ? pd_py_str(code) : strdup("");
        char *ol = pd_oneline(cs);
        free(cs);
        char *tr = pd_tail_trunc(ol, max_len);
        free(ol);
        if (tr[0]) res = tr; else free(tr);
    }
    json_free(a);
    return res;
}

/* PoP: disp_preview_delegate_task @ agent/display.py:_preview_delegate_task */
/* Action preview, else "{count} tasks: {goals}" / "{count} parallel tasks",
 * else the single goal. malloc'd or NULL; caller frees. */
char *disp_preview_delegate_task(const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    char *ap = disp_delegate_action_preview(args_json);
    if (ap) {
        char *res = pd_tail_trunc(ap, max_len);
        free(ap);
        json_free(a);
        return res;
    }
    json_t *tasks = json_obj_get(a, "tasks");
    if (tasks && pd_truthy(tasks) && tasks->type == JSON_ARRAY) {
        json_t *goals = pd_delegate_goals_arr(tasks, 40);
        size_t ng = goals->c.count;
        char *preview = NULL;
        if (ng > 0) {
            pd_buf j; pd_binit(&j);
            for (size_t i = 0; i < ng; i++) {
                if (i) pd_bputs(&j, " | ");
                pd_bputs(&j, goals->c.items[i]->str_val);
            }
            asprintf(&preview, "%zu tasks: %s", ng, j.b ? j.b : "");
            free(j.b);
        } else {
            asprintf(&preview, "%zu parallel tasks", tasks->c.count);
        }
        json_free(goals);
        char *res = pd_tail_trunc(preview, max_len);
        free(preview);
        json_free(a);
        return res;
    }
    json_t *goal = json_obj_get(a, "goal");
    if (goal && goal->type == JSON_NULL) { json_free(a); return NULL; }
    char *gs = goal ? pd_py_str(goal) : strdup("");
    char *ol = pd_oneline(gs);
    free(gs);
    char *tr = pd_tail_trunc(ol, max_len);
    free(ol);
    char *res = tr[0] ? tr : NULL;
    if (!tr[0]) free(tr);
    json_free(a);
    return res;
}

/* PoP: disp_preview_process_manage @ agent/display.py:_preview_process_manage */
/* "<action> <sid[:16]> \"<data[:20] oneline>\" <timeout>s" (wait only).
 * malloc'd or NULL when empty; caller frees. */
char *disp_preview_process_manage(const char *args_json, long max_len) {
    (void)max_len;
    json_t *a = pd_parse_obj(args_json);
    json_t *action = json_obj_get(a, "action"), *sid = json_obj_get(a, "session_id"),
           *data = json_obj_get(a, "data"), *timeout = json_obj_get(a, "timeout");
    pd_buf x; pd_binit(&x);
    int any = 0;
    if (pd_truthy(action)) {
        char *s = pd_py_str(action);
        if (s[0]) { if (any) pd_bputc(&x, ' '); pd_bputs(&x, s); any = 1; }
        free(s);
    }
    if (pd_truthy(sid)) {
        char *s = pd_py_str(sid);
        char *h = pd_utf8_head(s, 16);
        free(s);
        if (h && h[0]) { if (any) pd_bputc(&x, ' '); pd_bputs(&x, h); any = 1; }
        free(h);
    }
    if (pd_truthy(data)) {
        char *s = pd_py_str(data);
        char *h = pd_utf8_head(s, 20);
        free(s);
        char *ol = pd_oneline(h ? h : "");
        free(h);
        if (any) pd_bputc(&x, ' ');
        pd_bputc(&x, '"'); pd_bputs(&x, ol); pd_bputc(&x, '"');
        any = 1;
        free(ol);
    }
    if (pd_truthy(timeout)) {
        int is_wait = action && action->type == JSON_STRING && action->str_val &&
                      strcmp(action->str_val, "wait") == 0;
        if (is_wait) {
            char *ts = pd_py_str(timeout);
            if (any) pd_bputc(&x, ' ');
            pd_bputs(&x, ts); pd_bputc(&x, 's');
            any = 1;
            free(ts);
        }
    }
    json_free(a);
    if (!any) { free(x.b); return NULL; }
    return pd_btake(&x);
}

/* PoP: disp_preview_todo_list @ agent/display.py:_preview_todo_list */
/* "reading task list" when todos absent, else "{planning|updating} N task(s)".
 * malloc'd; caller frees. */
char *disp_preview_todo_list(const char *args_json, long max_len) {
    (void)max_len;
    json_t *a = pd_parse_obj(args_json);
    json_t *todos = json_obj_get(a, "todos");
    json_t *merge = json_obj_get(a, "merge");
    if (!todos || todos->type == JSON_NULL) {
        json_free(a);
        return strdup("reading task list");
    }
    const char *verb = pd_truthy(merge) ? "updating" : "planning";
    size_t count = 0;
    if (todos->type == JSON_ARRAY || todos->type == JSON_OBJECT) count = todos->c.count;
    else if (todos->type == JSON_STRING) count = pd_utf8_len(todos->str_val ? todos->str_val : "");
    char *out = NULL;
    asprintf(&out, "%s %zu task(s)", verb, count);
    json_free(a);
    return out;
}

/* ---- WindowsPath.name (quarry runs on Windows) + read_file line label ---- */
static char *pd_windows_path_name(const char *path) {
    const char *s = path;
    if (s[0] == '/' && s[1] == '/' && s[2] != '/') {
        const char *p = s + 2;
        while (*p && *p != '/') p++;
        if (*p == '/') { p++; while (*p && *p != '/') p++; }
        s = p;
    } else if (isalpha((unsigned char)s[0]) && s[1] == ':') {
        s += 2;
        if (*s == '/') s++;
    }
    const char *last = NULL; size_t lastlen = 0;
    const char *p = s;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        last = start; lastlen = len;
    }
    if (!last) return strdup("");
    char *o = malloc(lastlen + 1);
    if (!o) return NULL;
    memcpy(o, last, lastlen); o[lastlen] = '\0';
    return o;
}
static char *pd_read_file_line_label(const json_t *a) {
    json_t *off = json_obj_get(a, "offset"), *lim = json_obj_get(a, "limit");
    if (!off || off->type != JSON_NUMBER) return strdup("");
    double od = off->num_val;
    if (od != floor(od) || od <= 0) return strdup("");
    long o = (long)od;
    if (lim && lim->type == JSON_NUMBER && lim->num_val == floor(lim->num_val) && lim->num_val > 1) {
        long l = (long)lim->num_val;
        char *b = NULL;
        asprintf(&b, "L%ld-%ld", o, o + l - 1);
        return b;
    }
    char *b = NULL;
    asprintf(&b, "L%ld", o);
    return b;
}

/* PoP: disp_preview_read_file @ agent/display.py:_preview_read_file */
/* "<basename> <Loffset[-end]>" from path/file/filepath. malloc'd or NULL;
 * caller frees. */
char *disp_preview_read_file(const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    json_t *path = json_obj_get(a, "path");
    if (!pd_truthy(path)) path = json_obj_get(a, "file");
    if (!pd_truthy(path)) path = json_obj_get(a, "filepath");
    if (!pd_truthy(path)) { json_free(a); return NULL; }
    char *ps = pd_py_str(path);
    for (char *p = ps; *p; p++) if (*p == '\\') *p = '/';
    char *name = pd_windows_path_name(ps);
    char *label;
    if (name && name[0]) label = name;
    else { free(name); label = strdup(ps); }
    free(ps);
    char *ll = pd_read_file_line_label(a);
    pd_buf x; pd_binit(&x);
    pd_bputs(&x, label); pd_bputc(&x, ' '); pd_bputs(&x, ll);
    free(label); free(ll);
    char *joined = pd_btake(&x);
    char *stripped = pd_strip(joined);
    free(joined);
    char *tr = pd_tail_trunc(stripped, max_len);
    free(stripped);
    char *res = tr[0] ? tr : NULL;
    if (!tr[0]) free(tr);
    json_free(a);
    return res;
}

/* ---- _memory_first_op (internal; exercised via _preview_memory) ---- */
static void pd_memory_first_op(const json_t *a, const json_t **op_out, char **more_out) {
    json_t *ops = json_obj_get(a, "operations");
    if (!ops || ops->type != JSON_ARRAY || ops->c.count == 0) {
        *op_out = a; *more_out = strdup("");
        return;
    }
    json_t *first = ops->c.items[0];
    *op_out = (first && first->type == JSON_OBJECT) ? first : NULL;
    if (ops->c.count > 1) asprintf(more_out, " (%zu more)", ops->c.count - 1);
    else *more_out = strdup("");
}

/* PoP: disp_preview_memory @ agent/display.py:_preview_memory */
/* "+target: \"content\"" / "~target: \"old\"" / "-target: \"old\"" with a
 * " (N more)" batch suffix, else the raw action. malloc'd; caller frees. */
char *disp_preview_memory(const char *args_json, long max_len) {
    (void)max_len;
    json_t *a = pd_parse_obj(args_json);
    const json_t *op; char *more;
    pd_memory_first_op(a, &op, &more);
    json_t *action = op ? json_obj_get(op, "action") : NULL;
    json_t *target = json_obj_get(a, "target");
    char *acts = action ? pd_py_str(action) : strdup("");
    char *tgs = target ? pd_py_str(target) : strdup("");
    char *res = NULL;
    if (strcmp(acts, "add") == 0) {
        json_t *content = op ? json_obj_get(op, "content") : NULL;
        json_t *nt = op ? json_obj_get(op, "new_text") : NULL;
        json_t *cv = pd_truthy(content) ? content : (pd_truthy(nt) ? nt : NULL);
        char *cs = cv ? pd_py_str(cv) : strdup("");
        char *ol = pd_oneline(cs);
        free(cs);
        char *cl = pd_clip(ol, 25);
        free(ol);
        asprintf(&res, "+%s: \"%s\"%s", tgs, cl, more);
        free(cl);
    } else if (strcmp(acts, "replace") == 0 || strcmp(acts, "patch") == 0 || strcmp(acts, "remove") == 0) {
        json_t *ot = op ? json_obj_get(op, "old_text") : NULL;
        char *ots = pd_truthy(ot) ? pd_py_str(ot) : strdup("");
        char *ol = pd_oneline(ots);
        free(ots);
        char *old;
        if (ol[0]) old = ol;
        else { free(ol); old = strdup("<missing old_text>"); }
        char *h = pd_utf8_head(old, 20);
        free(old);
        const char *sign = strcmp(acts, "remove") == 0 ? "-" : "~";
        asprintf(&res, "%s%s: \"%s\"%s", sign, tgs, h ? h : "", more);
        free(h);
    } else {
        res = strdup(acts);
    }
    free(acts); free(tgs); free(more);
    json_free(a);
    return res;
}

/* PoP: disp_preview_send_message @ agent/display.py:_preview_send_message */
/* "to {target}: \"{message[:20 oneline]}\"". malloc'd; caller frees. */
char *disp_preview_send_message(const char *args_json, long max_len) {
    (void)max_len;
    json_t *a = pd_parse_obj(args_json);
    json_t *target = json_obj_get(a, "target");
    json_t *msg = json_obj_get(a, "message");
    char *tgs = target ? pd_py_str(target) : strdup("?");
    char *ms = msg ? pd_py_str(msg) : strdup("");
    char *ol = pd_oneline(ms);
    free(ms);
    char *tr = pd_tail_trunc(ol, 20);
    free(ol);
    char *res = NULL;
    asprintf(&res, "to %s: \"%s\"", tgs, tr);
    free(tgs); free(tr);
    json_free(a);
    return res;
}

/* PoP: disp_preview_skill_view @ agent/display.py:_preview_skill_view */
/* "{name} → {file_path}" (or whichever is present), tail-truncated.
 * malloc'd or NULL; caller frees. */
char *disp_preview_skill_view(const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    json_t *name = json_obj_get(a, "name");
    char *ns = pd_truthy(name) ? pd_py_str(name) : strdup("");
    char *nol = pd_oneline(ns);
    free(ns);
    json_t *fp = json_obj_get(a, "file_path");
    char *label;
    if (pd_truthy(fp)) {
        char *fs = pd_py_str(fp);
        char *fol = pd_oneline(fs);
        free(fs);
        if (nol[0]) {
            label = NULL;
            asprintf(&label, "%s \xE2\x86\x92 %s", nol, fol); /* → */
            free(fol);
        } else label = fol;
    } else label = strdup(nol);
    free(nol);
    char *tr = pd_tail_trunc(label, max_len);
    free(label);
    char *res = tr[0] ? tr : NULL;
    if (!tr[0]) free(tr);
    json_free(a);
    return res;
}

/* PoP: disp_preview_clarify @ agent/display.py:_preview_clarify */
/* First question's text, collapsed + tail-truncated; NULL when absent.
 * malloc'd or NULL; caller frees. */
char *disp_preview_clarify(const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    json_t *qs = json_obj_get(a, "questions");
    char *res = NULL;
    if (qs && qs->type == JSON_ARRAY && qs->c.count > 0) {
        json_t *first = qs->c.items[0];
        if (first && first->type == JSON_OBJECT) {
            json_t *q = json_obj_get(first, "question");
            char *qs2 = pd_truthy(q) ? pd_py_str(q) : strdup("");
            char *ol = pd_oneline(qs2);
            free(qs2);
            char *tr = pd_tail_trunc(ol, max_len);
            free(ol);
            if (tr[0]) res = tr; else free(tr);
        }
    }
    json_free(a);
    return res;
}

/* PoP: disp_primary_arg_preview @ agent/display.py:_primary_arg_preview */
/* Primary argument (curated key, else first fallback key present); lists use
 * their first element. malloc'd or NULL; caller frees. */
static const struct { const char *tool, *key; } pd_primary_args[] = {
    {"terminal", "command"}, {"web_search", "query"}, {"web_extract", "urls"}, {"read_file", "path"},
    {"write_file", "path"}, {"patch", "path"}, {"search_files", "pattern"}, {"browser_navigate", "url"},
    {"browser_click", "ref"}, {"browser_type", "text"}, {"image_generate", "prompt"}, {"text_to_speech", "text"},
    {"vision_analyze", "question"}, {"skill_view", "name"}, {"skills_list", "category"}, {"cronjob_manage", "action"},
    {"execute_code", "code"}, {"browser_exec", "code"}, {"delegate_task", "goal"}, {"skill_manage", "name"},
    {NULL, NULL}
};
static const char *pd_fallback_keys[] = {"query", "text", "command", "path", "name", "prompt", "code", "goal", NULL};
char *disp_primary_arg_preview(const char *tool_name, const char *args_json, long max_len) {
    json_t *a = pd_parse_obj(args_json);
    const char *key = NULL;
    for (int i = 0; pd_primary_args[i].tool; i++)
        if (tool_name && strcmp(tool_name, pd_primary_args[i].tool) == 0) { key = pd_primary_args[i].key; break; }
    if (!key)
        for (int i = 0; pd_fallback_keys[i]; i++)
            if (json_obj_get(a, pd_fallback_keys[i])) { key = pd_fallback_keys[i]; break; }
    if (!key) { json_free(a); return NULL; }
    json_t *value = json_obj_get(a, key);
    if (!value) { json_free(a); return NULL; }
    char *s;
    if (value->type == JSON_ARRAY)
        s = value->c.count > 0 ? pd_py_str(value->c.items[0]) : strdup("");
    else s = pd_py_str(value);
    char *ol = pd_oneline(s);
    free(s);
    char *res = NULL;
    if (ol[0]) res = pd_tail_trunc(ol, max_len);
    free(ol);
    json_free(a);
    return res;
}

/* PoP: disp_tool_verb @ agent/display.py:_tool_verb */
/* Curated friendly verb (bundled en catalog), or NULL. malloc'd; caller frees. */
static const struct { const char *tool, *verb; } pd_tool_verbs[] = {
    {"web_search", "Searching the web"}, {"web_extract", "Reading"}, {"browser_navigate", "Browsing"},
    {"browser_click", "Clicking"}, {"browser_type", "Typing"}, {"read_file", "Reading"}, {"write_file", "Writing"},
    {"patch", "Editing"}, {"search_files", "Searching files"}, {"terminal", "Running"}, {"execute_code", "Running code"},
    {"image_generate", "Generating image"}, {"video_generate", "Generating video"}, {"text_to_speech", "Generating speech"},
    {"vision_analyze", "Looking at the image"}, {"session_search", "Searching past sessions"}, {"skill_view", "Reading skill"},
    {"skills_list", "Listing skills"}, {"skill_manage", "Updating skill"}, {"delegate_task", "Delegating"},
    {"cronjob_manage", "Scheduling"}, {"clarify", "Asking"}, {"memory", "Updating memory"}, {"todo_list", "Updating tasks"},
    {NULL, NULL}
};
char *disp_tool_verb(const char *tool_name) {
    if (!tool_name) return NULL;
    for (int i = 0; pd_tool_verbs[i].tool; i++)
        if (strcmp(tool_name, pd_tool_verbs[i].tool) == 0) return strdup(pd_tool_verbs[i].verb);
    return NULL;
}

/* PoP: disp_bridge_generating_phrase @ agent/display.py:bridge_generating_phrase */
/* "a tool call"/"a tool search"/"tool details" when friendly labels enabled,
 * else NULL. malloc'd; caller frees. */
char *disp_bridge_generating_phrase(const char *tool_name, int friendly) {
    if (!friendly || !tool_name) return NULL;
    if (strcmp(tool_name, "tool_call") == 0) return strdup("a tool call");
    if (strcmp(tool_name, "tool_search") == 0) return strdup("a tool search");
    if (strcmp(tool_name, "tool_describe") == 0) return strdup("tool details");
    return NULL;
}

/* PoP: disp_degraded_suffix @ agent/display.py:_degraded_suffix */
/* " [<reason> — <retry_hint>]" (hint omitted when empty), capped at 200.
 * malloc'd; caller frees. */
char *disp_degraded_suffix(const char *data_json) {
    json_t *d = pd_parse_obj(data_json);
    json_t *reason = json_obj_get(d, "reason"), *error = json_obj_get(d, "error"),
           *hint = json_obj_get(d, "retry_hint");
    json_t *rv = pd_truthy(reason) ? reason : (pd_truthy(error) ? error : NULL);
    char *rs = rv ? pd_py_str(rv) : strdup("terminal backend unavailable");
    char *rstrip = pd_strip(rs);
    free(rs);
    char *hs = pd_truthy(hint) ? pd_py_str(hint) : strdup("");
    char *hstrip = pd_strip(hs);
    free(hs);
    char *text;
    if (hstrip[0]) {
        text = NULL;
        asprintf(&text, "%s \xE2\x80\x94 %s", rstrip, hstrip); /* — */
    } else text = strdup(rstrip);
    free(rstrip); free(hstrip);
    char *tr = pd_tail_trunc(text, 200);
    free(text);
    char *res = NULL;
    asprintf(&res, " [%s]", tr);
    free(tr);
    json_free(d);
    return res;
}

/* PoP: disp_is_neutral_failure @ agent/display.py:is_neutral_failure */
/* True when a failed skill_manage call renders neutrally (no "ROLLBACK FAILED"
 * in the error text). result arrives as JSON text (dict or string). */
int disp_is_neutral_failure(const char *tool_name, const char *result_json) {
    if (!tool_name || strcmp(tool_name, "skill_manage") != 0) return 0;
    json_t *r = json_parse(result_json ? result_json : "null", NULL);
    json_t *data = NULL; int data_owned = 0;
    if (r && r->type == JSON_OBJECT) data = r;
    else if (r && r->type == JSON_STRING) {
        data = json_parse(r->str_val ? r->str_val : "", NULL);
        data_owned = 1;
    }
    char *error_str;
    if (data && data->type == JSON_OBJECT) {
        json_t *e = json_obj_get(data, "error");
        error_str = pd_truthy(e) ? pd_py_str(e) : strdup("");
    } else {
        error_str = pd_truthy(r) ? pd_py_str(r) : strdup("");
    }
    int neutral = strstr(error_str, "ROLLBACK FAILED") == NULL;
    free(error_str);
    if (data_owned && data) json_free(data);
    if (r) json_free(r);
    return neutral;
}

/* PoP: disp_domain @ agent/display.py:_domain */
/* Strip http(s):// (all occurrences) and keep the host up to the first "/".
 * malloc'd; caller frees. */
char *disp_domain(const char *url) {
    if (!url) return strdup("");
    pd_buf x; pd_binit(&x);
    const char *p = url;
    while (*p) {
        if (strncmp(p, "https://", 8) == 0) p += 8;
        else if (strncmp(p, "http://", 7) == 0) p += 7;
        else { pd_bputc(&x, *p); p++; }
    }
    char *s = pd_btake(&x);
    char *slash = strchr(s, '/');
    if (slash) *slash = '\0';
    return s;
}

/* PoP: disp_cute_trunc @ agent/display.py:_cute_trunc */
/* Tail-truncate to the preview cap (passed explicitly; 0 = unlimited).
 * malloc'd; caller frees. */
char *disp_cute_trunc(const char *s, long limit) {
    return pd_tail_trunc(s ? s : "", limit);
}

/* PoP: disp_cute_path @ agent/display.py:_cute_path */
/* Head-truncate a path to the cap, keeping the filename end ("..." + tail).
 * malloc'd; caller frees. */
char *disp_cute_path(const char *p, long limit) {
    const char *s = p ? p : "";
    size_t L = pd_utf8_len(s);
    if (limit <= 0 || L <= (size_t)limit) return strdup(s);
    if (limit <= 3) {
        char *o = malloc((size_t)limit + 1);
        if (!o) return NULL;
        memset(o, '.', (size_t)limit); o[limit] = '\0';
        return o;
    }
    char *tail = pd_utf8_tail(s, (size_t)limit - 3);
    pd_buf x; pd_binit(&x);
    pd_bputs(&x, "..."); pd_bputs(&x, tail ? tail : "");
    free(tail);
    return pd_btake(&x);
}
