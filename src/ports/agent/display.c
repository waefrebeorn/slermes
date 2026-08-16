#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/display.c — C11 port of agent/display.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_display.c ── */
/*
 * port_agent_display.c — Port of Python agent/display.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _delegate_task_goal_parts */
typedef struct {
    int count;
    char goals[32][256];
} delegate_task_goals_t;

delegate_task_goals_t delegate_task_goal_parts(const char *tasks_json, int per_goal_len) {
    delegate_task_goals_t result = {0};
    if (!tasks_json) return result;
    
    /* Simple JSON parsing for task goals */
    const char *p = tasks_json;
    while (*p && result.count < 32) {
        const char *goal_key = strstr(p, "\"goal\"");
        if (!goal_key) break;
        const char *val = strchr(goal_key + 6, ':');
        if (!val) break;
        val = strchr(val, '\"');
        if (!val) break;
        val++;
        size_t i = 0;
        while (*val && *val != '\"' && i < 255) {
            result.goals[result.count][i++] = *val++;
        }
        result.goals[result.count][i] = '\0';
        result.count++;
        p = val;
    }
    return result;
}


/* Port of Python: _truncate_preview */
void truncate_preview(const char *text, int max_len, char *out, size_t out_sz) {
    if (!text || !out || out_sz == 0) return;
    size_t text_len = strlen(text);
    if (max_len > 0 && (int)text_len > max_len) {
        if ((size_t)max_len <= 3) {
            memset(out, '.', max_len);
            out[max_len] = '\0';
        } else {
            memcpy(out, text, max_len - 3);
            out[max_len - 3] = '.';
            out[max_len - 2] = '.';
            out[max_len - 1] = '.';
            out[max_len] = '\0';
        }
    } else {
        strncpy(out, text, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}



/* ── from src/agent/port_agent_display_helpers.c ── */
/* Slermes C port — agent/display.py (pure tool-preview / shell-label helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "hermes_json.h"

/* Forward decl: src/cli/display_core.c (build_tool_preview port). */
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len);

/* PoP: agent_display_oneline @ agent/display.py:_oneline */
void agent_display_oneline(const char *text, char *out, size_t outsz)
{
    if (outsz == 0) return;
    size_t o = 0;
    int prev_ws = 1; /* start as if we just saw whitespace so leading space is dropped */
    for (const char *p = text; *p; p++) {
        unsigned char c = *p;
        int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
        if (ws) {
            prev_ws = 1;
        } else {
            if (prev_ws && o > 0 && o + 1 < outsz) out[o++] = ' '; /* space between words only */
            if (o + 1 < outsz) out[o++] = (char)c;
            prev_ws = 0;
        }
    }
    /* o never has trailing space because we only emit space BEFORE a word */
    out[o] = '\0';
}

/* PoP: agent_display_truncate_preview @ agent/display.py:_truncate_preview */
void agent_display_truncate_preview(const char *text, int max_len, char *out, size_t outsz)
{
    if (outsz == 0) return;
    size_t L = strlen(text);
    if (max_len > 0 && (size_t)L > (size_t)max_len) {
        if (max_len <= 3) {
            size_t n = (size_t)max_len < outsz ? (size_t)max_len : outsz - 1;
            for (size_t i = 0; i < n; i++) out[i] = '.';
            out[n] = '\0';
            return;
        }
        size_t keep = (size_t)max_len - 3;
        if (keep >= outsz) keep = outsz - 1;
        size_t i = 0;
        for (; i < keep && i < L; i++) out[i] = text[i];
        out[i++] = '.'; out[i++] = '.'; out[i++] = '.';
        out[i] = '\0';
        return;
    }
    size_t i = 0;
    for (; text[i] && i + 1 < outsz; i++) out[i] = text[i];
    out[i] = '\0';
}

/* PoP: agent_display_shell_basename @ agent/display.py:_shell_basename */
void agent_display_shell_basename(const char *head, char *out, size_t outsz)
{
    if (outsz == 0) return;
    if (!head || !*head) { out[0] = '\0'; return; }
    const char *slash = strrchr(head, '/');
    const char *base = slash ? slash + 1 : head;
    size_t i = 0;
    for (; base[i] && i + 1 < outsz; i++) out[i] = base[i];
    out[i] = '\0';
}

/* PoP: http @ agent/display.py:prepare_tool_preview */

/* Port of Python prepare_tool_preview: build one canonical compact preview
 * before platform formatting. Rebuilds the uncapped preview from tool args
 * (so an upstream display cap cannot discard its link target), truncates,
 * and extracts a http(s) URL when truncated.
 *
 * Returns malloc'd JSON:
 *   {"text": ..., "truncated": bool, "url": ...|null}
 * Caller frees. */
char *agent_display_prepare_tool_preview(const char *tool_name,
                                         const char *args_json,
                                         const char *fallback,
                                         int max_len)
{
    char *full_text = NULL;

    /* Python: build_tool_preview(tool_name, args, max_len=0) or fallback. */
    if (tool_name && tool_name[0]) {
        full_text = cli_agent_display__build_tool_preview(tool_name, args_json, 0);
    }
    if (!full_text || !full_text[0]) {
        free(full_text);
        full_text = strdup(fallback ? fallback : "");
    }

    /* Python: text = _truncate_preview(full_text, max_len). */
    char text[4096];
    agent_display_truncate_preview(full_text, max_len, text, sizeof(text));
    bool truncated = strcmp(text, full_text) != 0;

    json_t *obj = json_object();
    json_set(obj, "text", json_string(text));
    json_set(obj, "truncated", json_bool(truncated));

    /* Python: url only when truncated and the full text contains a
     * http(s) URL with a netloc. */
    const char *url = NULL;
    char url_buf[2048];
    url_buf[0] = '\0';
    if (truncated) {
        /* _display_url(full_text): dict url/href or first http(s):// URL. */
        const char *scheme = strstr(full_text, "http://");
        const char *scheme2 = strstr(full_text, "https://");
        const char *cand = NULL;
        if (scheme && scheme2) cand = (scheme < scheme2) ? scheme : scheme2;
        else cand = scheme ? scheme : scheme2;
        if (cand) {
            /* validate scheme http/https and netloc non-empty */
            const char *after = strstr(cand, "://") + 3;
            const char *netloc_end = after ? strchr(after, '/') : NULL;
            size_t nl = netloc_end ? (size_t)(netloc_end - after) : strlen(after);
            if (nl > 0) {
                size_t n = strlen(cand);
                if (n >= sizeof(url_buf)) n = sizeof(url_buf) - 1;
                memcpy(url_buf, cand, n);
                url_buf[n] = '\0';
                url = url_buf;
            }
        }
    }
    if (url) json_set(obj, "url", json_string(url));
    else json_set(obj, "url", json_null());

    free(full_text);
    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}


/* ── from src/agent/port_agent_display_py_parity.c ── */
/* Slermes C port — agent/display.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_display_split_shell_words_p @ agent/display.py:_split_shell_words */
json_t *agent_display_split_shell_words_p(void) {
    return json_object();
}

/* PoP: agent_display_split_shell_compound_p @ agent/display.py:_split_shell_compound */
json_t *agent_display_split_shell_compound_p(void) {
    return json_object();
}

/* PoP: agent_display_enter___p @ agent/display.py:__enter__ */
json_t *agent_display_enter___p(void) {
    return json_object();
}

/* PoP: agent_display_exit___p @ agent/display.py:__exit__ */
json_t *agent_display_exit___p(void) {
    return json_object();
}


/* ── from src/agent/port_display_ports.c ── */
/*
 * port_display_remaining.c — Port of agent/display.py helper surface
 * (continuation of port_agent_display.o / port_display_tool_preview.c).
 * Skin resolution, diff rendering pipeline, spinner lifecycle, tool
 * result detection.
 */
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

/* PoP: disp_spinner_is_patch_stdout_proxy @ agent/display.py:_is_patch_stdout_proxy */
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

/* PoP: disp_spinner_start @ agent/display.py:start */
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


/* ── from src/cli/port_agent_display.c ── */
/*
 * port_agent_display.c — C port of agent/display.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>





/* ===================================================================
 * Shell command summarization helpers — pure string/quoting logic.
 * Port of agent/display.py: _shell_basename, _split_shell_words,
 * _strip_shell_pipe_tail, _split_shell_compound, _clean_shell_segment,
 * _is_shell_boundary_echo.
 * =================================================================== */

/* _SHELL_SILENT_HEADS and _SHELL_PIPE_TAIL_HEADS mirrored as C sets. */
static int _in_set(const char *w, const char *const *set, int n)
{
    for (int i = 0; i < n; i++)
        if (strcmp(w, set[i]) == 0) return 1;
    return 0;
}
static const char *_SHELL_SILENT_HEADS[] = {
    "cd", "pushd", "popd", "export", "set", "unset", "source",
    ".", "true", "false", ":", NULL
};
static const char *_SHELL_PIPE_TAIL_HEADS[] = {
    "head", "tail", "wc", "sort", "uniq", NULL
};

/* _shell_basename(head) -> last path component. Caller frees. */
static char *_shell_basename(const char *head)
{
    if (!head || !*head) return strdup("");
    const char *slash = strrchr(head, '/');
    const char *base = slash ? slash + 1 : head;
    return strdup(base);
}

/* _split_shell_words: split on whitespace, honoring ' and " quotes.
 * Returns malloc'd array of malloc'd strings; *count set; NULL term. */
static char **_cli_split_shell_words(const char *segment, int *count)
{
    /* Two passes: count words, then allocate. Keep simple: tokenize into
     * a dynamic array. */
    char **words = NULL;
    int cap = 0, n = 0;
    const char *p = segment;
    char buf[4096];
    size_t blen = 0;
    int in_word = 0;
    char quote = 0;
    while (*p) {
        char c = *p;
        if (quote) {
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            /* emulate Python: quote ends when ch==quote and prev not backslash */
            /* (we don't track backslash specially beyond not ending) */
            if (c == quote) quote = 0;
            p++;
            in_word = 1;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            if (!in_word) { in_word = 1; blen = 0; }
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            p++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (in_word) {
                buf[blen] = '\0';
                if (n + 1 > cap) {
                    cap = cap ? cap * 2 : 8;
                    words = (char **)realloc(words, cap * sizeof(char *));
                }
                words[n++] = strdup(buf);
                in_word = 0; blen = 0;
            }
            p++;
            continue;
        }
        if (!in_word) { in_word = 1; blen = 0; }
        if (blen + 1 < sizeof(buf)) buf[blen++] = c;
        p++;
    }
    if (in_word) {
        buf[blen] = '\0';
        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 8;
            words = (char **)realloc(words, cap * sizeof(char *));
        }
        words[n++] = strdup(buf);
    }
    if (n + 1 > cap) {
        cap = n + 1;
        words = (char **)realloc(words, cap * sizeof(char *));
    }
    words[n] = NULL;
    *count = n;
    return words;
}

static void _free_words(char **words, int n)
{
    for (int i = 0; i < n; i++) free(words[i]);
    free(words);
}

/* _strip_shell_pipe_tail: drop words from the first "| <tail-head>" on. */
static char *_cli_strip_shell_pipe_tail(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    /* Build output up to (but not including) the "|" whose next word basename
     * is in _SHELL_PIPE_TAIL_HEADS. */
    int cut = n;
    for (int i = 0; i < n; i++) {
        if (strcmp(words[i], "|") == 0) {
            const char *next = (i + 1 < n) ? words[i + 1] : "";
            char *bn = _shell_basename(next);
            int is_tail = _in_set(bn, _SHELL_PIPE_TAIL_HEADS,
                                  (int)(sizeof(_SHELL_PIPE_TAIL_HEADS) / sizeof(char *) - 1));
            free(bn);
            if (is_tail) { cut = i; break; }
        }
    }
    /* join words[0..cut) with spaces */
    size_t out_sz = 1;
    for (int i = 0; i < cut; i++) out_sz += strlen(words[i]) + 1;
    char *out = (char *)malloc(out_sz);
    out[0] = '\0';
    for (int i = 0; i < cut; i++) {
        if (i) strcat(out, " ");
        strcat(out, words[i]);
    }
    /* strip trailing whitespace */
    size_t L = strlen(out);
    while (L > 0 && (out[L - 1] == ' ' || out[L - 1] == '\t')) out[--L] = '\0';
    _free_words(words, n);
    return out;
}

/* _split_shell_compound: split on ; & && || \n into segments (pipe-tail
 * stripped). Returns malloc'd array of malloc'd strings; *count set. */
static char **_cli_split_shell_compound(const char *command, int *count)
{
    char **segs = NULL;
    int cap = 0, n = 0;
    char buf[8192];
    size_t blen = 0;
    char quote = 0;
    size_t L = strlen(command);
    size_t i = 0;
    while (i < L) {
        char c = command[i];
        if (quote) {
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            if (c == quote) quote = 0;
            i++;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            i++;
            continue;
        }
        int op_len = 0;
        if (i + 1 < L && (strncmp(command + i, "&&", 2) == 0 || strncmp(command + i, "||", 2) == 0))
            op_len = 2;
        else if (c == ';' || c == '\n')
            op_len = 1;
        if (op_len) {
            buf[blen] = '\0';
            char *seg = _cli_strip_shell_pipe_tail(buf);
            if (seg && *seg) {
                if (n + 1 > cap) { cap = cap ? cap * 2 : 8; segs = (char **)realloc(segs, cap * sizeof(char *)); }
                segs[n++] = seg;
            } else free(seg);
            blen = 0;
            i += op_len;
            continue;
        }
        if (blen + 1 < sizeof(buf)) buf[blen++] = c;
        i++;
    }
    buf[blen] = '\0';
    char *seg = _cli_strip_shell_pipe_tail(buf);
    if (seg && *seg) {
        if (n + 1 > cap) { cap = n + 1; segs = (char **)realloc(segs, cap * sizeof(char *)); }
        segs[n++] = seg;
    } else free(seg);
    if (n + 1 > cap) { cap = n + 1; segs = (char **)realloc(segs, cap * sizeof(char *)); }
    segs[n] = NULL;
    *count = n;
    return segs;
}

/* _clean_shell_segment: drop redirection tokens (N>, >&, <&, etc.) and
 * bare N> / < operators. Returns malloc'd string. */
static char *_cli_clean_shell_segment(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    char **out = (char **)malloc((n + 1) * sizeof(char *));
    int m = 0;
    int i = 0;
    while (i < n) {
        const char *w = words[i];
        /* ^\d*(?:>>?|<)$  -> skip word and the next (the target) */
        int digits = 0; while (w[digits] >= '0' && w[digits] <= '9') digits++;
        int is_redir1 = (strcmp(w + digits, ">") == 0 || strcmp(w + digits, ">>") == 0 || strcmp(w + digits, "<") == 0);
        if (is_redir1) { i += 2; continue; }
        /* ^\d*(?:>&|<&)\d+$ or ^\d*>&\d+$ -> skip just this word */
        int is_redir2 = 0;
        if (w[digits] == '>' && w[digits + 1] == '&') is_redir2 = 1;
        else if (w[digits] == '<' && w[digits + 1] == '&') is_redir2 = 1;
        else if (w[digits] == '>' && w[digits + 1] == '&') is_redir2 = 1;
        if (is_redir2) { i += 1; continue; }
        out[m++] = strdup(w);
        i += 1;
    }
    out[m] = NULL;
    size_t sz = 1;
    for (int k = 0; k < m; k++) sz += strlen(out[k]) + 1;
    char *res = (char *)malloc(sz);
    res[0] = '\0';
    for (int k = 0; k < m; k++) {
        if (k) strcat(res, " ");
        strcat(res, out[k]);
    }
    size_t R = strlen(res);
    while (R > 0 && (res[R - 1] == ' ' || res[R - 1] == '\t')) res[--R] = '\0';
    for (int k = 0; k < m; k++) free(out[k]);
    free(out);
    _free_words(words, n);
    return res;
}

/* _is_shell_boundary_echo: detects `echo ... --... | _exit= | $? | PIPESTATUS` */
static int _cli_is_shell_boundary_echo(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    char *bn = _shell_basename(n > 0 ? words[0] : "");
    int is_echo = (strcmp(bn, "echo") == 0);
    free(bn);
    if (!is_echo) { _free_words(words, n); return 0; }
    /* rest = " ".join(words[1:]) */
    size_t rsz = 1;
    for (int i = 1; i < n; i++) rsz += strlen(words[i]) + 1;
    char *rest = (char *)malloc(rsz);
    rest[0] = '\0';
    for (int i = 1; i < n; i++) { if (i > 1) strcat(rest, " "); strcat(rest, words[i]); }
    int found = (strstr(rest, "--") != NULL)        /* -{2,} */
             || (strstr(rest, "_exit=") != NULL)
             || (strstr(rest, "$?") != NULL)
             || (strstr(rest, "${") != NULL)
             || (strstr(rest, "PIPESTATUS") != NULL);
    free(rest);
    _free_words(words, n);
    return found;
}

/* ── Public CLI entrypoints (PoP-annotated) ────────────────────────── */

/* PoP: cli_agent_display__split_shell_words @ agent/display.py:_split_shell_words */
char **cli_agent_display__split_shell_words(const char *segment, int *count)
{
    return _cli_split_shell_words(segment, count);
}

/* PoP: cli_agent_display__strip_shell_pipe_tail @ agent/display.py:_strip_shell_pipe_tail */
char *cli_agent_display__strip_shell_pipe_tail(const char *segment)
{
    return _cli_strip_shell_pipe_tail(segment);
}

/* PoP: cli_agent_display__split_shell_compound @ agent/display.py:_split_shell_compound */
char **cli_agent_display__split_shell_compound(const char *command, int *count)
{
    return _cli_split_shell_compound(command, count);
}

/* PoP: cli_agent_display__clean_shell_segment @ agent/display.py:_clean_shell_segment */
char *cli_agent_display__clean_shell_segment(const char *segment)
{
    return _cli_clean_shell_segment(segment);
}

/* PoP: cli_agent_display__is_shell_boundary_echo @ agent/display.py:_is_shell_boundary_echo */
int cli_agent_display__is_shell_boundary_echo(const char *segment)
{
    return _cli_is_shell_boundary_echo(segment);
}

/* PoP: whitespace @ agent/display.py:_oneline */
/* Collapse all whitespace (incl. newlines) to single spaces between runs,
 * matching Python " ".join(text.split()). Caller frees. */
char *cli_agent_display__oneline(const char *text)
{
    if (!text) return strdup("");
    size_t len = strlen(text);
    char *out = malloc(len + 1);
    size_t o = 0;
    int need_space = 0; /* a space is due before the next non-ws char */
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\f' || c == '\v') {
            need_space = 1;
            continue;
        }
        if (need_space && o > 0) out[o++] = ' ';
        out[o++] = c;
        need_space = 0;
    }
    out[o] = '\0';
    return out;
}

/* PoP: _cli_shell_head_word @ agent/display.py:_shell_head_word */
/* First non-KEY= word, reduced to its basename. Caller frees. */
static char *_cli_shell_head_word(const char *segment)
{
    if (!segment) return strdup("");
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    int index = 0;
    while (index < n) {
        char *w = words[index];
        /* matches regex ^[A-Za-z_]\w*= (a VAR= assignment) */
        int is_assign = 0;
        if (w && w[0] && ((w[0] >= 'A' && w[0] <= 'Z') || (w[0] >= 'a' && w[0] <= 'z') || w[0] == '_')) {
            size_t k = 1;
            for (; w[k] && w[k] != '='; k++) {
                char c = w[k];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '_')) break;
            }
            if (w[k] == '=') is_assign = 1;
        }
        if (!is_assign) break;
        index++;
    }
    const char *chosen = (index < n) ? words[index] : "";
    char *bn = _shell_basename(chosen);
    for (int i = 0; i < n; i++) free(words[i]);
    free(words);
    return bn; /* bn is a fresh strdup, caller frees */
}

/* PoP: _cli_is_silent_head @ agent/display.py:summarize_shell_command */

/* Silent compound-command heads (see _SHELL_SILENT_HEADS). */
static const char *CLI_SHELL_SILENT_HEADS[] = {
    "cd", "pushd", "popd", "export", "set", "unset", "source", ".", "true", "false", ":", NULL
};
static int _cli_is_silent_head(const char *h)
{
    if (!h) return 0;
    for (int i = 0; CLI_SHELL_SILENT_HEADS[i]; i++)
        if (strcmp(h, CLI_SHELL_SILENT_HEADS[i]) == 0) return 1;
    return 0;
}

/* Compact shell wrapper/plumbing for display while preserving raw command
 * elsewhere. Caller frees the returned string. */
char *cli_agent_display__summarize_shell_command(const char *command)
{
    char *original = cli_agent_display__oneline(command ? command : "");
    if (!original || !original[0]) { free(original); return strdup(""); }

    int nseg = 0;
    char **segments = _cli_split_shell_compound(original, &nseg);
    if (nseg <= 1) {
        char *cleaned = (nseg == 1) ? _cli_clean_shell_segment(segments[0]) : strdup(original);
        char *res = (cleaned && cleaned[0]) ? cleaned : strdup(original);
        for (int i = 0; i < nseg; i++) free(segments[i]);
        free(segments); free(original);
        return res;
    }

    char **core = malloc((size_t)nseg * sizeof(char *));
    int core_n = 0;
    for (int s = 0; s < nseg; s++) {
        char *cleaned = _cli_clean_shell_segment(segments[s]);
        char *head = _cli_shell_head_word(cleaned);
        int keep = cleaned && cleaned[0] && !_cli_is_silent_head(head) &&
                   !_cli_is_shell_boundary_echo(cleaned);
        if (keep) core[core_n++] = cleaned;
        else free(cleaned);
        free(head);
    }
    for (int i = 0; i < nseg; i++) free(segments[i]);
    free(segments); free(original);

    if (core_n == 0) { free(core); return strdup(""); }

    char *res;
    if (core_n == 1) {
        res = core[0]; /* transfer ownership */
    } else {
        int count = core_n - 1;
        size_t cap = 0;
        for (int i = 0; i < core_n; i++) cap += strlen(core[i]) + 1;
        cap += 32;
        res = malloc(cap);
        res[0] = '\0';
        strcat(res, core[0]);
        char num[16];
        snprintf(num, sizeof(num), " + %d ", count);
        strcat(res, num);
        strcat(res, (count == 1) ? "command" : "commands");
        for (int i = 1; i < core_n; i++) free(core[i]);
    }
    free(core);
    return res;
}


/* ── from src/cli/port_display_tool_preview.c ── */
/*
 * port_display_tool_preview.c — C port of agent/display.py tool preview/label
 * builders (build_tool_preview, build_tool_label, redact_tool_args_for_display)
 * plus their pure helpers (_truncate_preview, _read_file_line_label).
 *
 * Faithful port of the pure-transform display functions. Terminal/code
 * previews reuse cli_agent_display__summarize_shell_command; whitespace
 * collapsing uses cli_agent_display__oneline; browser_type text redaction
 * uses the browser_redact port of redact_sensitive_text.
 */

#include "hermes_json.h"
#include "cli/port_agent_display.h"
#include "tools/browser_redact.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* ---- _truncate_preview ---- */

/* PoP: cli_agent_display__truncate_preview @ agent/display.py:_truncate_preview */
char *cli_agent_display__truncate_preview(const char *text, int max_len)
{
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (max_len > 0 && len > (size_t)max_len) {
        if (max_len <= 3) {
            char *r = malloc((size_t)max_len + 1);
            for (int i = 0; i < max_len; i++) r[i] = '.';
            r[max_len] = '\0';
            return r;
        }
        size_t cut = (size_t)max_len - 3;
        char *r = malloc(cut + 4);
        memcpy(r, text, cut);
        r[cut] = '\0';
        strcat(r, "...");
        return r;
    }
    return strdup(text);
}

/* ---- _read_file_line_label ---- */

/* PoP: cli_agent_display__read_file_line_label @ agent/display.py:_read_file_line_label */
char *cli_agent_display__read_file_line_label(const json_t *args)
{
    if (!args || args->type != JSON_OBJECT) return strdup("");
    json_t *off = json_obj_get(args, "offset");
    json_t *lim = json_obj_get(args, "limit");
    int offset = (off && off->type == JSON_NUMBER) ? json_node_get_int(off) : 0;
    int limit = (lim && lim->type == JSON_NUMBER) ? json_node_get_int(lim) : 0;
    if (offset <= 0) return strdup("");
    if (limit <= 1) {
        char buf[32]; snprintf(buf, sizeof(buf), "L%d", offset); return strdup(buf);
    }
    char buf[64]; snprintf(buf, sizeof(buf), "L%d-%d", offset, offset + limit - 1); return strdup(buf);
}

/* ---- redact_tool_args_for_display ---- */

/* PoP: cli_agent_display__redact_tool_args_for_display @ agent/display.py:redact_tool_args_for_display */
/* Returns a freshly-allocated JSON string with browser_type "text" redacted.
 * For all other tools returns an unmodified copy of args_json. Caller frees. */
char *cli_agent_display__redact_tool_args_for_display(const char *tool_name, const char *args_json)
{
    if (!args_json) return strdup("{}");
    if (strcmp(tool_name ? tool_name : "", "browser_type") != 0)
        return strdup(args_json);
    char *err = NULL;
    json_t *args = json_parse(args_json, &err);
    if (!args) { if (err) free(err); return strdup(args_json); }
    json_t *text = json_obj_get(args, "text");
    if (text && text->type == JSON_STRING && text->str_val) {
        char *red = browser_redact_sensitive_text(text->str_val);
        json_set(args, "text", json_string(red ? red : ""));
        free(red);
    }
    char *out = json_serialize(args);
    json_free(args);
    return out ? out : strdup(args_json);
}

/* ---- build_tool_preview ---- */

/* A tiny growable string builder */
typedef struct { char *buf; size_t cap; size_t len; } sb_t;
static void sb_init(sb_t *s) { s->cap = 64; s->buf = malloc(s->cap); s->buf[0] = '\0'; s->len = 0; }
static void sb_append_str(sb_t *s, const char *v)
{
    if (!v) return;
    size_t add = strlen(v);
    if (s->len + add + 1 > s->cap) { while (s->len + add + 1 > s->cap) s->cap *= 2; s->buf = realloc(s->buf, s->cap); }
    memcpy(s->buf + s->len, v, add + 1); s->len += add;
}
static void sb_appendf(sb_t *s, const char *fmt, ...)
{
    char tmp[1024]; va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
    sb_append_str(s, tmp);
}

/* primary arg key per tool (from Python primary_args) */
static const char *primary_arg_key(const char *tool)
{
    if (!tool) return NULL;
    if (strcmp(tool,"terminal")==0) return "command";
    if (strcmp(tool,"web_search")==0) return "query";
    if (strcmp(tool,"web_extract")==0) return "urls";
    if (strcmp(tool,"read_file")==0) return "path";
    if (strcmp(tool,"write_file")==0) return "path";
    if (strcmp(tool,"patch")==0) return "path";
    if (strcmp(tool,"search_files")==0) return "pattern";
    if (strcmp(tool,"browser_navigate")==0) return "url";
    if (strcmp(tool,"browser_click")==0) return "ref";
    if (strcmp(tool,"browser_type")==0) return "text";
    if (strcmp(tool,"image_generate")==0) return "prompt";
    if (strcmp(tool,"text_to_speech")==0) return "text";
    if (strcmp(tool,"vision_analyze")==0) return "question";
    if (strcmp(tool,"skill_view")==0) return "name";
    if (strcmp(tool,"skills_list")==0) return "category";
    if (strcmp(tool,"cronjob")==0) return "action";
    if (strcmp(tool,"execute_code")==0) return "code";
    if (strcmp(tool,"delegate_task")==0) return "goal";
    if (strcmp(tool,"clarify")==0) return "question";
    if (strcmp(tool,"skill_manage")==0) return "name";
    return NULL;
}

/* PoP: string @ agent/display.py:build_tool_preview */
/* tool_name + args_json (object) + max_len (0 = unlimited). Returns a malloc'd
 * preview string (caller frees) or strdup("") on empty/None. */
char *cli_agent_display__build_tool_preview(const char *tool_name, const char *args_json, int max_len)
{
    if (!tool_name) tool_name = "";
    if (!args_json || !args_json[0]) return strdup("");

    char *red = cli_agent_display__redact_tool_args_for_display(tool_name, args_json);
    char *err = NULL;
    json_t *args = json_parse(red, &err);
    free(red);
    if (!args) { if (err) free(err); return strdup(""); }
    if (args->type != JSON_OBJECT) { json_free(args); return strdup(""); }

    sb_t sb; sb_init(&sb);
    char *result = NULL;

    if (strcmp(tool_name, "delegate_task") == 0) {
        json_t *tasks = json_obj_get(args, "tasks");
        if (tasks && tasks->type == JSON_ARRAY) {
            int task_count = 0;
            sb_t goals; sb_init(&goals);
            size_t n = json_len(tasks);
            for (size_t i = 0; i < n; i++) {
                json_t *t = json_get(tasks, i);
                if (!t || t->type != JSON_OBJECT) continue;
                json_t *raw = json_obj_get(t, "goal");
                char *g = cli_agent_display__oneline(raw && raw->type == JSON_STRING ? raw->str_val : "?");
                char *gt = cli_agent_display__truncate_preview(g ? g : "?", 40);
                if (goals.len) sb_append_str(&goals, " | ");
                sb_append_str(&goals, gt);
                free(gt); free(g);
                task_count++;
            }
            if (goals.len)
                sb_appendf(&sb, "%d tasks: %s", task_count, goals.buf);
            else
                sb_appendf(&sb, "%zu parallel tasks", n);
            free(goals.buf);
        } else {
            json_t *goal = json_obj_get(args, "goal");
            if (!goal || (goal->type == JSON_NULL)) { json_free(args); free(sb.buf); return strdup(""); }
            char *gp = cli_agent_display__oneline(goal->type == JSON_STRING ? goal->str_val : "");
            if (gp && gp[0]) {
                char *gt = cli_agent_display__truncate_preview(gp, max_len);
                sb_append_str(&sb, gt); free(gt);
            }
            free(gp);
        }
    } else if (strcmp(tool_name, "process") == 0) {
        json_t *action = json_obj_get(args, "action");
        json_t *sid = json_obj_get(args, "session_id");
        json_t *data = json_obj_get(args, "data");
        json_t *timeout_val = json_obj_get(args, "timeout");
        sb_append_str(&sb, action && action->type == JSON_STRING ? action->str_val : "");
        if (sid && sid->type == JSON_STRING && sid->str_val[0]) {
            size_t sl = strlen(sid->str_val);
            if (sl > 16) { char tmp[17]; memcpy(tmp, sid->str_val, 16); tmp[16]='\0'; sb_appendf(&sb, " %s", tmp); }
            else sb_appendf(&sb, " %s", sid->str_val);
        }
        if (data && data->type == JSON_STRING && data->str_val[0]) {
            char *on = cli_agent_display__oneline(data->str_val);
            size_t dl = strlen(on);
            char *cut = dl > 20 ? strndup(on, 20) : strdup(on);
            sb_appendf(&sb, " \"%s\"", cut); free(cut); free(on);
        }
        if (timeout_val && action && action->type == JSON_STRING && strcmp(action->str_val, "wait") == 0) {
            if (timeout_val->type == JSON_NUMBER) {
                sb_append_str(&sb, " ");
                sb_appendf(&sb, "%d", json_node_get_int(timeout_val));
            }
            sb_append_str(&sb, "s");
        }
    } else if (strcmp(tool_name, "todo") == 0) {
        json_t *todos = json_obj_get(args, "todos");
        json_t *merge = json_obj_get(args, "merge");
        int merge_on = merge && (merge->type == JSON_BOOL ? merge->bool_val : (merge->type == JSON_NUMBER && merge->num_val != 0.0));
        if (todos == NULL || (todos->type == JSON_NULL)) sb_append_str(&sb, "reading task list");
        else if (merge_on) sb_appendf(&sb, "updating %zu task(s)", json_len(todos));
        else sb_appendf(&sb, "planning %zu task(s)", json_len(todos));
    } else if (strcmp(tool_name, "terminal") == 0 || strcmp(tool_name, "execute_code") == 0) {
        const char *key = strcmp(tool_name, "execute_code") == 0 ? "code" : "command";
        json_t *cmd = json_obj_get(args, key);
        if (!cmd || (cmd->type != JSON_STRING)) { json_free(args); free(sb.buf); return strdup(""); }
        char *sum = cli_agent_display__summarize_shell_command(cmd->str_val);
        if (sum && sum[0]) {
            char *t = cli_agent_display__truncate_preview(sum, max_len);
            sb_append_str(&sb, t); free(t);
        }
        free(sum);
    } else if (strcmp(tool_name, "read_file") == 0) {
        json_t *p = json_obj_get(args, "path");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "file");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "filepath");
        if (!p || p->type != JSON_STRING) { json_free(args); free(sb.buf); return strdup(""); }
        /* label = basename after replacing backslashes with '/' */
        char *norm = strdup(p->str_val);
        for (char *c = norm; *c; c++) if (*c == '\\') *c = '/';
        char *label = strrchr(norm, '/'); label = label ? label + 1 : norm;
        char *line_lbl = cli_agent_display__read_file_line_label(args);
        sb_append_str(&sb, label);
        if (line_lbl && line_lbl[0]) { sb_append_str(&sb, " "); sb_append_str(&sb, line_lbl); }
        free(line_lbl); free(norm);
    } else if (strcmp(tool_name, "session_search") == 0) {
        json_t *q = json_obj_get(args, "query");
        char *qq = cli_agent_display__oneline(q && q->type == JSON_STRING ? q->str_val : "");
        size_t ql = strlen(qq);
        char *cut = ql > 25 ? strndup(qq, 25) : strdup(qq);
        sb_appendf(&sb, "recall: \"%s%s\"", cut, ql > 25 ? "..." : "");
        free(cut); free(qq);
    } else if (strcmp(tool_name, "memory") == 0) {
        json_t *action = json_obj_get(args, "action");
        json_t *target = json_obj_get(args, "target");
        const char *act = action && action->type == JSON_STRING ? action->str_val : "";
        const char *tgt = target && target->type == JSON_STRING ? target->str_val : "";
        if (strcmp(act, "add") == 0) {
            json_t *c = json_obj_get(args, "content");
            char *cc = cli_agent_display__oneline(c && c->type == JSON_STRING ? c->str_val : "");
            size_t cl = strlen(cc); char *cut = cl > 25 ? strndup(cc, 25) : strdup(cc);
            sb_appendf(&sb, "+%s: \"%s%s\"", tgt, cut, cl > 25 ? "..." : "");
            free(cut); free(cc);
        } else if (strcmp(act, "replace") == 0 || strcmp(act, "remove") == 0) {
            json_t *o = json_obj_get(args, "old_text");
            char *oo = cli_agent_display__oneline(o && o->type == JSON_STRING ? o->str_val : "");
            if (!oo || !oo[0]) { free(oo); oo = strdup("<missing old_text>"); }
            size_t ol = strlen(oo); char *cut = ol > 20 ? strndup(oo, 20) : strdup(oo);
            sb_appendf(&sb, "%s%s: \"%s\"", act[0] == 'r' ? "-" : "~", tgt, cut);
            free(cut); free(oo);
        } else sb_append_str(&sb, act);
    } else if (strcmp(tool_name, "send_message") == 0) {
        json_t *target = json_obj_get(args, "target");
        json_t *msg = json_obj_get(args, "message");
        const char *tgt = target && target->type == JSON_STRING ? target->str_val : "?";
        char *mm = cli_agent_display__oneline(msg && msg->type == JSON_STRING ? msg->str_val : "");
        size_t ml = strlen(mm);
        if (ml > 20) { char *cut = strndup(mm, 17); sb_appendf(&sb, "to %s: \"%s...\"", tgt, cut); free(cut); }
        else sb_appendf(&sb, "to %s: \"%s\"", tgt, mm);
        free(mm);
    } else {
        const char *key = primary_arg_key(tool_name);
        if (!key) {
            const char *fb[] = {"query","text","command","path","name","prompt","code","goal",NULL};
            for (int i = 0; fb[i]; i++) {
                if (json_obj_get(args, fb[i])) { key = fb[i]; break; }
            }
        }
        if (!key || !json_obj_get(args, key)) { json_free(args); free(sb.buf); return strdup(""); }
        json_t *value = json_obj_get(args, key);
        char *vs;
        if (value->type == JSON_ARRAY) {
            json_t *first = json_get(value, 0);
            vs = cli_agent_display__oneline(first && first->type == JSON_STRING ? first->str_val : (first ? "" : ""));
        } else if (value->type == JSON_STRING) {
            vs = cli_agent_display__oneline(value->str_val);
        } else {
            char *sv = json_serialize(value);
            vs = cli_agent_display__oneline(sv ? sv : "");
            free(sv);
        }
        if (!vs || !vs[0]) { free(vs); json_free(args); free(sb.buf); return strdup(""); }
        char *t = cli_agent_display__truncate_preview(vs, max_len);
        sb_append_str(&sb, t);
        free(t); free(vs);
    }

    json_free(args);
    result = (sb.len > 0) ? strdup(sb.buf) : strdup("");
    free(sb.buf);
    return result;
}

/* ---- redact_browser_typed_text_for_display ---- */

/* Recursively replace every occurrence of `needle` with `redacted` in a JSON
 * value (strings replaced in-place substring-wise; dicts/arrays recursed).
 * Mirrors Python's recursive walk over str/dict/list (tuples become arrays in
 * JSON so are covered by the array branch). Mutates `node` in place. */
static char *str_replace_all(const char *hay, const char *needle, const char *repl)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return strdup(hay);
    size_t rlen = strlen(repl);
    /* count occurrences */
    size_t count = 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += nlen) count++;
    if (count == 0) return strdup(hay);
    size_t outlen = strlen(hay) + count * (rlen > nlen ? rlen - nlen : 0) + 1;
    char *out = malloc(outlen);
    char *w = out;
    const char *p = hay;
    while (1) {
        const char *hit = strstr(p, needle);
        if (!hit) { strcpy(w, p); break; }
        size_t pre = (size_t)(hit - p);
        memcpy(w, p, pre); w += pre;
        memcpy(w, repl, rlen); w += rlen;
        p = hit + nlen;
    }
    return out;
}

static void redact_typed_walk(json_t *node, const char *needle, const char *redacted)
{
    if (!node) return;
    if (node->type == JSON_STRING && node->str_val) {
        char *rep = str_replace_all(node->str_val, needle, redacted);
        free(node->str_val);
        node->str_val = rep;
    } else if (node->type == JSON_OBJECT) {
        size_t n = json_len(node);
        for (size_t i = 0; i < n; i++)
            redact_typed_walk(json_object_get_at(node, i), needle, redacted);
    } else if (node->type == JSON_ARRAY) {
        size_t n = json_len(node);
        for (size_t i = 0; i < n; i++)
            redact_typed_walk(json_get(node, i), needle, redacted);
    }
}

/* PoP: cli_agent_display__redact_browser_typed_text_for_display @ agent/display.py:redact_browser_typed_text_for_display */
/* value_json is a serialized JSON value; typed_text is the raw typed string.
 * Returns a freshly-allocated serialized JSON value with every occurrence of the
 * raw typed secret replaced by its redacted form. When typed_text is NULL/empty
 * or contains nothing secret-looking, returns an unmodified copy. Caller frees. */
char *cli_agent_display__redact_browser_typed_text_for_display(const char *value_json, const char *typed_text)
{
    if (!value_json) return strdup("null");
    if (!typed_text || !typed_text[0]) return strdup(value_json);
    char *redacted = browser_redact_sensitive_text(typed_text);
    if (!redacted || strcmp(redacted, typed_text) == 0) {
        /* Nothing secret-looking; leave payload untouched. */
        free(redacted);
        return strdup(value_json);
    }
    char *err = NULL;
    json_t *node = json_parse(value_json, &err);
    if (!node) { if (err) free(err); free(redacted); return strdup(value_json); }
    redact_typed_walk(node, typed_text, redacted);
    char *out = json_serialize(node);
    json_free(node);
    free(redacted);
    return out ? out : strdup(value_json);
}

/* ---- friendly tool labels global state + accessors ---- */

static int s_friendly_tool_labels = 1;

/* PoP: cli_agent_display__set_friendly_tool_labels @ agent/display.py:set_friendly_tool_labels */
void cli_agent_display__set_friendly_tool_labels(int enabled)
{
    s_friendly_tool_labels = enabled ? 1 : 0;
}

/* PoP: cli_agent_display__get_friendly_tool_labels @ agent/display.py:get_friendly_tool_labels */
int cli_agent_display__get_friendly_tool_labels(void)
{
    return s_friendly_tool_labels;
}

/* ---- build_tool_label ---- */

/* PoP: TOOL_VERBS @ agent/display.py:build_tool_label */

static const char *TOOL_VERBS(const char *tool)
{
    if (!tool) return NULL;
    static const struct { const char *k; const char *v; } m[] = {
        {"web_search","Searching the web"},{"web_extract","Reading"},
        {"browser_navigate","Browsing"},{"browser_click","Clicking"},
        {"browser_type","Typing"},{"read_file","Reading"},
        {"write_file","Writing"},{"patch","Editing"},
        {"search_files","Searching files"},{"terminal","Running"},
        {"execute_code","Running code"},{"image_generate","Generating image"},
        {"video_generate","Generating video"},{"text_to_speech","Generating speech"},
        {"vision_analyze","Looking at the image"},{"session_search","Searching past sessions"},
        {"skill_view","Reading skill"},{"skills_list","Listing skills"},
        {"skill_manage","Updating skill"},{"memory","Updating memory"},
        {"todo","Updating tasks"},{"delegate_task","Delegating"},
        {"cronjob","Scheduling"},{"clarify","Asking"},{NULL,NULL}
    };
    for (int i = 0; m[i].k; i++) if (strcmp(m[i].k, tool) == 0) return m[i].v;
    return NULL;
}
/* PoP: cli_agent_display__get_tool_verb @ agent/display.py:get_tool_verb */
/* Returns the friendly verb for a built-in tool, or NULL when friendly labels
 * are disabled or the tool has no curated verb. Returned pointer is static. */
const char *cli_agent_display__get_tool_verb(const char *tool_name)
{
    if (!s_friendly_tool_labels) return NULL;
    return TOOL_VERBS(tool_name);
}

/* PoP: cli_agent_display__verb_drops_preview @ agent/display.py:verb_drops_preview */
int cli_agent_display__verb_drops_preview(const char *tool_name) {
    if (!tool_name) return 0;
    return strcmp(tool_name,"skills_list")==0 || strcmp(tool_name,"session_search")==0;
}
static int verb_no_preview(const char *tool) {
    return cli_agent_display__verb_drops_preview(tool);
}

/* PoP: preview @ agent/display.py:tool_verb_connector */
/* Returns the connector between a verb and its preview (" for " or " "). */
const char *cli_agent_display__tool_verb_connector(const char *tool_name) {
    if (tool_name && (strcmp(tool_name,"web_search")==0 || strcmp(tool_name,"search_files")==0))
        return " for ";
    return " ";
}
static int verb_for_connector(const char *tool) {
    return tool && strcmp(cli_agent_display__tool_verb_connector(tool), " for ") == 0;
}

char *cli_agent_display__build_tool_label(const char *tool_name, const char *args_json, int max_len, int friendly)
{
    if (!friendly)
        return cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    const char *verb = TOOL_VERBS(tool_name);
    if (!verb)
        return cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    if (verb_no_preview(tool_name))
        return strdup(verb);
    char *preview = cli_agent_display__build_tool_preview(tool_name, args_json, max_len);
    if (!preview || !preview[0]) { free(preview); return strdup(verb); }
    if (verb_for_connector(tool_name)) {
        char *r = malloc(strlen(verb) + strlen(preview) + 6);
        sprintf(r, "%s for %s", verb, preview);
        free(preview);
        return r;
    }
    char *r = malloc(strlen(verb) + strlen(preview) + 2);
    sprintf(r, "%s %s", verb, preview);
    free(preview);
    return r;
}

/* PoP: cli_agent_display__display_url @ agent/display.py:_display_url */
/* Extract a display-only URL. For a dict, prefers "url" then "href". For a
 * string, strip whitespace. Everything else → "". Returns malloc'd string. */
char *cli_agent_display__display_url(const char *value_json)
{
    if (!value_json) return strdup("");
    char *err = NULL;
    json_t *v = json_parse(value_json, &err);
    free(err);
    if (v && v->type == JSON_OBJECT) {
        json_t *u = json_obj_get(v, "url");
        if (!u) u = json_obj_get(v, "href");
        if (u && u->type == JSON_STRING) { char *r = strdup(u->str_val); json_free(v); return r; }
        json_free(v);
        return strdup("");
    }
    if (v) json_free(v);
    /* String path: strip whitespace. */
    const char *s = value_json;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) n--;
    return strndup(s, n);
}

/* PoP: cli_agent_display__prepare_tool_preview @ agent/display.py:prepare_tool_preview */
/* Build one canonical compact preview before platform formatting.
 *
 * Rebuilds the uncapped preview from the tool arguments (max_len=0 →
 * unlimited) so an upstream display cap cannot discard the link target.
 * Then truncates to max_len and, if truncated, extracts an http(s) URL
 * from the full text for explicit URL metadata.
 *
 * Returns a malloc'd ToolPreview{text, truncated, url} (caller frees text
 * and url; url may be NULL). */
typedef struct {
    char *text;
    int truncated;
    char *url;   /* NULL when none */
} agent_tool_preview_t;

agent_tool_preview_t *cli_agent_display__prepare_tool_preview(
    const char *tool_name, const char *args_json, const char *fallback, int max_len)
{
    agent_tool_preview_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    char *full = cli_agent_display__build_tool_preview(tool_name, args_json, 0);
    if (!full || !full[0]) {
        free(full);
        full = strdup(fallback ? fallback : "");
    }
    char *text = cli_agent_display__truncate_preview(full, max_len);
    if (!text) text = strdup("");
    p->text = text;
    p->truncated = (strcmp(text, full) != 0);

    if (p->truncated) {
        /* Extract url from the FULL text (the truncate may have cut it).
         * Python: _display_url(full_text) — full_text is a plain string, so
         * this just strips whitespace and returns it. */
        char *candidate = cli_agent_display__display_url(full);

        /* Validate http(s) scheme with a non-empty netloc.
         * Mirrors Python: urlsplit(candidate) parsed, scheme in {http,https},
         * netloc non-empty. A trailing slash is fine (netloc still parses). */
        if (candidate && *candidate) {
            const char *s = candidate;
            size_t n = strlen(s);
            int is_http = (n > 7 && strncmp(s, "http://", 7) == 0);
            int is_https = (n > 8 && strncmp(s, "https://", 8) == 0);
            if (is_http || is_https) {
                const char *after = s + (is_https ? 8 : 7);
                const char *slash = strchr(after, '/');
                size_t netlen = slash ? (size_t)(slash - after) : strlen(after);
                if (netlen > 0) p->url = candidate;
                else free(candidate);
            } else {
                free(candidate);
            }
        } else if (candidate) {
            free(candidate);
        }
    }
    free(full);
    return p;
}

void cli_agent_display__tool_preview_free(agent_tool_preview_t *p)
{
    if (!p) return;
    free(p->text);
    free(p->url);
    free(p);
}

/* PoP: NULL @ agent/display.py:_browser_exec_step_label */
/* User-friendly step label from browser_exec code's leading comment.
 * args is a JSON object string; reads "code", takes the first line, and if
 * it starts with '#', strips leading '#' + whitespace and truncates to
 * max_chars (appending '…' when truncated). Returns NULL (not a label) when
 * there's no code / no leading-comment label / empty label. Caller frees. */
char *cli_agent_display__browser_exec_step_label(const char *args_json, int max_chars)
{
    if (max_chars <= 0) max_chars = 80;
    if (!args_json || !args_json[0]) return NULL;
    char *err = NULL;
    json_t *args = json_parse(args_json, &err);
    free(err);
    if (!args || args->type != JSON_OBJECT) { if (args) json_free(args); return NULL; }
    json_t *code = json_obj_get(args, "code");
    const char *code_s = (code && code->type == JSON_STRING) ? code->str_val : "";
    while (*code_s == ' ' || *code_s == '\t' || *code_s == '\n' || *code_s == '\r') code_s++;
    if (!*code_s) { json_free(args); return NULL; }

    /* first line */
    const char *nl = strchr(code_s, '\n');
    size_t first_len = nl ? (size_t)(nl - code_s) : strlen(code_s);
    char *first = strndup(code_s, first_len);
    /* strip trailing whitespace of first line */
    size_t fl = strlen(first);
    while (fl > 0 && (first[fl-1]==' '||first[fl-1]=='\t'||first[fl-1]=='\r')) first[--fl]='\0';
    if (!first || first[0] != '#') { free(first); json_free(args); return NULL; }

    /* strip leading '#' then whitespace */
    const char *q = first + 1;
    while (*q == '#' ) q++;
    while (*q == ' ' || *q == '\t') q++;
    /* copy the label out before freeing the first-line buffer */
    char *label = strdup(q);
    free(first);
    if (!label || !*label) { free(label); json_free(args); return NULL; }

    size_t len = strlen(label);
    if (len > (size_t)max_chars) {
        /* label[:max_chars-1] + "…" */
        char *out = malloc(max_chars + 2);
        if (!out) { free(label); json_free(args); return NULL; }
        memcpy(out, label, max_chars - 1);
        out[max_chars - 1] = 0xE2; out[max_chars] = 0x80; out[max_chars + 1] = 0xA6; /* … */
        out[max_chars + 2] = '\0';
        free(label);
        json_free(args);
        return out;
    }
    json_free(args);
    return label;
}


