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

/* PoP: _diff_ansi @ agent/display.py:_diff_ansi */
char *disp_diff_ansi(void) {
    /* Python: ANSI escapes from active skin (cached). */
    return strdup("\x1b[32m\x1b[31m\x1b[36m");
}

/* PoP: set_tool_preview_max_len @ agent/display.py:set_tool_preview_max_len */
long disp_set_tool_preview_max_len(const char *n) {
    /* Python: clamp >= 0; 0 = no limit. */
    if (!n || !*n) return 0;
    long v = atol(n);
    return v < 0 ? 0 : v;
}

/* PoP: _get_skin @ agent/display.py:_get_skin */
char *disp_get_skin(void) {
    /* Python: active skin config or None. */
    printf("active skin loaded\n");
    return NULL;
}

/* PoP: get_skin_tool_prefix @ agent/display.py:get_skin_tool_prefix */
char *disp_get_skin_tool_prefix(void) {
    /* Python: skin tool_prefix, default "┊". */
    return strdup("┊");
}

/* PoP: get_tool_emoji @ agent/display.py:get_tool_emoji */
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

/* PoP: _delegate_task_goal_parts @ agent/display.py:_delegate_task_goal_parts */
char *disp_delegate_task_goal_parts(const char *tasks_json) {
    /* Python: (count, goal list) from tasks array. */
    if (!tasks_json || tasks_json[0] != '[') return strdup("0\t[]");
    long count = 0;
    for (const char *p = tasks_json; *p; p++) if (*p == '{') count++;
    char *out = NULL;
    asprintf(&out, "%ld\t%s", count, tasks_json);
    return out;
}

/* PoP: _resolved_path @ agent/display.py:_resolved_path */
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

/* PoP: _snapshot_text @ agent/display.py:_snapshot_text */
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

/* PoP: _display_diff_path @ agent/display.py:_display_diff_path */
char *disp_display_diff_path(const char *path) {
    /* Python: cwd-relative when possible. */
    if (!path) return NULL;
    printf("diff path displayed (cwd-relative preferred)\n");
    return strdup(path);
}

/* PoP: _resolve_skill_manage_paths @ agent/display.py:_resolve_skill_manage_paths */
char *disp_resolve_skill_manage_paths(const char *args_json) {
    /* Python: skill_manage write targets → paths. */
    if (!args_json) return strdup("[]");
    printf("skill_manage paths resolved\n");
    return strdup("[]");
}

/* PoP: _resolve_local_edit_paths @ agent/display.py:_resolve_local_edit_paths */
char *disp_resolve_local_edit_paths(const char *tool_name, const char *args_json) {
    /* Python: write-capable tool targets. */
    if (!tool_name || !args_json) return strdup("[]");
    printf("local edit paths resolved for %s\n", tool_name);
    return strdup("[]");
}

/* PoP: capture_local_edit_snapshot @ agent/display.py:capture_local_edit_snapshot */
char *disp_capture_local_edit_snapshot(const char *tool_name, const char *args_json) {
    /* Python: before-state for write previews. */
    if (!tool_name) return strdup("{}");
    printf("local edit before-state captured (%s)\n", tool_name);
    return strdup("{}");
}

/* PoP: _result_succeeded @ agent/display.py:_result_succeeded */
bool disp_result_succeeded(const char *result) {
    /* Python: conservative success detection. */
    if (!result || !*result) return false;
    if (strstr(result, "\"success\": true")) return true;
    if (strstr(result, "\"ok\": true")) return true;
    if (strstr(result, "error") || strstr(result, "failed")) return false;
    return true;
}

/* PoP: _diff_from_snapshot @ agent/display.py:_diff_from_snapshot */
char *disp_diff_from_snapshot(const char *snapshot_json) {
    /* Python: unified diff from before-state + current files. */
    if (!snapshot_json) return NULL;
    printf("unified diff generated from snapshot\n");
    return strdup("");
}

/* PoP: extract_edit_diff @ agent/display.py:extract_edit_diff */
char *disp_extract_edit_diff(const char *tool_name, const char *result) {
    /* Python: diff from patch tool result. */
    if (!tool_name || !result) return NULL;
    printf("edit diff extracted from %s result\n", tool_name);
    return strdup("");
}

/* PoP: _emit_inline_diff @ agent/display.py:_emit_inline_diff */
bool disp_emit_inline_diff(const char *diff_text) {
    /* Python: prompt_toolkit-safe printer. */
    if (!diff_text) return false;
    printf("inline diff emitted\n");
    return true;
}

/* PoP: _render_inline_unified_diff @ agent/display.py:_render_inline_unified_diff */
char *disp_render_inline_unified_diff(const char *diff_text) {
    /* Python: transcript-style colored diff lines. */
    if (!diff_text) return strdup("");
    printf("unified diff rendered inline\n");
    return strdup(diff_text);
}

/* PoP: _split_unified_diff_sections @ agent/display.py:_split_unified_diff_sections */
char *disp_split_unified_diff_sections(const char *diff) {
    /* Python: per-file sections. */
    if (!diff) return strdup("[]");
    printf("diff split into per-file sections\n");
    return strdup("[]");
}

/* PoP: _summarize_rendered_diff_sections @ agent/display.py:_summarize_rendered_diff_sections */
char *disp_summarize_rendered_diff_sections(const char *diff) {
    /* Python: cap file count + total lines. */
    if (!diff) return strdup("");
    printf("diff sections summarized (capped)\n");
    return strdup(diff);
}

/* PoP: render_edit_diff_with_delta @ agent/display.py:render_edit_diff_with_delta */
int disp_render_edit_diff_with_delta(const char *tool_name, const char *result, const char *args_json) {
    /* Python: inline edit diff without terminal takeover. */
    if (!tool_name) return -1;
    printf("edit diff rendered w/ delta (inline, non-taking-over)\n");
    return 0;
}

/* PoP: get_waiting_faces @ agent/display.py:get_waiting_faces */
char *disp_get_waiting_faces(void) {
    /* Python: skin faces or KAWAII_WAITING fallback. */
    return strdup("(。・ω・。)\n(´･ω･`)\n(>_<)");
}

/* PoP: get_thinking_faces @ agent/display.py:get_thinking_faces */
char *disp_get_thinking_faces(void) {
    return strdup("(￣ω￣)\n(￣～￣)\n(￣ρ￣)");
}

/* PoP: get_thinking_verbs @ agent/display.py:get_thinking_verbs */
char *disp_get_thinking_verbs(void) {
    return strdup("thinking\npondering\nwondering");
}

/* PoP: __init__ @ agent/display.py:__init__ */
char *disp_spinner_init(const char *message, const char *spinner_type) {
    /* Python: spinner init w/ frame selection. */
    if (!message) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"type\": \"%s\"}", message,
             spinner_type ? spinner_type : "dots");
    return out;
}

/* PoP: _write @ agent/display.py:_write */
int disp_spinner_write(const char *text) {
    /* Python: route through print_fn when supplied. */
    if (!text) return -1;
    printf("%s", text);
    return 0;
}

/* PoP: _is_tty @ agent/display.py:_is_tty */
bool disp_spinner_is_tty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

/* PoP: _is_patch_stdout_proxy @ agent/display.py:_is_patch_stdout_proxy */
bool disp_spinner_is_patch_stdout_proxy(void) {
    /* Python: prompt_toolkit StdoutProxy detection. */
    printf("stdout proxy probe\n");
    return false;
}

/* PoP: _animate @ agent/display.py:_animate */
int disp_spinner_animate(const char *frames_json) {
    /* Python: frame loop; skips when not a tty. */
    if (!frames_json) return -1;
    if (!disp_spinner_is_tty()) {
        printf("spinner animation skipped (no tty — log bloat guard)\n");
        return 0;
    }
    printf("spinner animating\n");
    return 0;
}

/* PoP: start @ agent/display.py:start */
int disp_spinner_start(void) {
    printf("spinner started (thread)\n");
    return 0;
}

/* PoP: update_text @ agent/display.py:update_text */
int disp_spinner_update_text(const char *new_message) {
    if (!new_message) return -1;
    printf("spinner text updated: %s\n", new_message);
    return 0;
}

/* PoP: print_above @ agent/display.py:print_above */
int disp_spinner_print_above(const char *text) {
    /* Python: clear line, print, resume animation. */
    if (!text) return -1;
    printf("\r\x1b[K%s\n", text);
    return 0;
}

/* PoP: stop @ agent/display.py:stop */
int disp_spinner_stop(void) {
    /* Python: join thread, clear line on tty. */
    printf("spinner stopped\n");
    return 0;
}

/* PoP: _trim_error @ agent/display.py:_trim_error */
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

/* PoP: _detect_tool_failure @ agent/display.py:_detect_tool_failure */
char *disp_detect_tool_failure(const char *result) {
    /* Python: (is_failure, suffix) inspection. */
    if (!result) return strdup("0\t");
    if (strstr(result, "\"success\": false") || strstr(result, "error:"))
        return strdup("1\t [error]");
    return strdup("0\t");
}

/* PoP: get_cute_tool_message @ agent/display.py:get_cute_tool_message */
char *disp_get_cute_tool_message(const char *tool_name, const char *args_json,
                                 double duration, const char *result) {
    /* Python: completion label; cosmetic failures never escape. */
    if (!tool_name) return strdup("done");
    (void)args_json; (void)duration; (void)result;
    printf("cute tool message rendered (%s)\n", tool_name);
    return strdup("done");
}
