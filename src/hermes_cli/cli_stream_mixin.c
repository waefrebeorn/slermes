/*
 * cli_stream_mixin.c — C port of upstream Hermes hermes_cli/cli_stream_mixin.py.
 * Gate: w15_clistream_56f8e8
 *
 * PoP: w15_clistream_56f8e8_slow_command_status @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._slow_command_status
 * PoP: w15_clistream_56f8e8_current_reasoning_callback @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._current_reasoning_callback
 * PoP: w15_clistream_56f8e8_reset_stream_state @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._reset_stream_state
 * PoP: w15_clistream_56f8e8_flush_reasoning_preview @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._flush_reasoning_preview
 *
 * The oracle stubs t() as identity (so translation keys are the return values),
 * rich.markup.escape as identity, and injects terminal width through `columns`.
 */
#include "noc_w15_clistream_56f8e8.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THINKING_PREFIX "  cli.stream.thinking_label "
#define THINKING_PREFIX_LEN ((int)(sizeof(THINKING_PREFIX) - 1))

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Copy s[start:end_exclusive] into a fresh NUL-terminated string. */
static char *substr_dup(const char *s, size_t start, size_t end) {
    if (end < start) end = start;
    size_t n = end - start;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s + start, n); p[n] = '\0'; }
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

json_t *w15_clistream_56f8e8_slow_command_status(const char *command) {
    char *cmd = dupstr(command ? command : "");
    if (!cmd) return json_string("cli.stream.processing_command");
    for (char *p = cmd; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *start = cmd;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    *end = '\0';  /* NUL at the shortened length */
    const char *out = NULL;
    if (!strcmp(start, "/reload-mcp")) out = "cli.stream.busy_reload_mcp";
    else if (!strcmp(start, "/reload-skills") || !strcmp(start, "/reload_skills"))
        out = "cli.stream.busy_reload_skills";
    else {
        static const char *const prefixes[] = {
            "/skills search", "/skills browse", "/skills inspect", "/skills install",
            "/skills", "/browser", NULL
        };
        static const char *const keys[] = {
            "cli.stream.busy_skills_search", "cli.stream.busy_skills_browse",
            "cli.stream.busy_skills_inspect", "cli.stream.busy_skills_install",
            "cli.stream.busy_skills", "cli.stream.busy_browser", NULL
        };
        for (int i = 0; prefixes[i]; i++)
            if (!strncmp(start, prefixes[i], strlen(prefixes[i]))) { out = keys[i]; break; }
    }
    if (!out) out = "cli.stream.processing_command";
    json_t *res = json_string(out);
    free(cmd);
    return res;
}

json_t *w15_clistream_56f8e8_current_reasoning_callback(const json_t *show_reasoning,
                                                        const json_t *streaming_enabled,
                                                        const json_t *verbose) {
    int show = json_truthy(show_reasoning);
    int streaming = json_truthy(streaming_enabled);
    int verb = json_truthy(verbose);
    if (show && streaming) return json_string("stream");
    if (verb && !show) return json_string("on_reasoning");
    return json_null();
}

json_t *w15_clistream_56f8e8_reset_stream_state(void) {
    json_t *out = json_object();
    json_set(out, "_stream_buf", json_string(""));
    json_set(out, "_stream_started", json_bool(0));
    json_set(out, "_stream_box_opened", json_bool(0));
    json_set(out, "_stream_text_ansi", json_string(""));
    json_set(out, "_stream_prefilt", json_string(""));
    json_set(out, "_in_reasoning_block", json_bool(0));
    json_set(out, "_stream_last_was_newline", json_bool(1));
    json_set(out, "_reasoning_box_opened", json_bool(0));
    json_set(out, "_reasoning_buf", json_string(""));
    json_set(out, "_reasoning_preview_buf", json_string(""));
    json_set(out, "_stream_table_buf", json_array());
    json_set(out, "_in_stream_table", json_bool(0));
    json_set(out, "_stream_box_live", json_bool(0));
    return out;
}

/* Python str.rfind(needle, start, end): last i with start<=i and i+len<=end. */
static int rfind_sub(const char *s, const char *needle, int start, int end) {
    size_t L = strlen(s), n = strlen(needle);
    if (n == 0) return end;
    if (end > (int)L) end = (int)L;
    for (int i = end - (int)n; i >= start; i--) {
        if (i < 0) break;
        if (!strncmp(s + i, needle, n)) return i;
    }
    return -1;
}

static int endswith_any(const char *s, const char *const *suffixes, size_t count) {
    size_t L = strlen(s);
    for (size_t i = 0; i < count; i++) {
        size_t n = strlen(suffixes[i]);
        if (n && n <= L && !strcmp(s + L - n, suffixes[i])) return 1;
    }
    return 0;
}

static char *lstrip_dup(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return dupstr(s);
}

json_t *w15_clistream_56f8e8_flush_reasoning_preview(const char *buf_in,
                                                     int force,
                                                     int columns) {
    char *buf = dupstr(buf_in ? buf_in : "");
    char *flush = NULL;
    json_t *out = json_object();
    if (!buf) { json_set(out, "buf", json_string("")); json_set(out, "flush", json_null()); return out; }
    size_t L = strlen(buf);
    if (L == 0) {
        json_set(out, "buf", json_string(""));
        json_set(out, "flush", json_null());
        free(buf);
        return out;
    }
    int target_width = columns - THINKING_PREFIX_LEN - 4;
    if (target_width < 40) target_width = 40;
    if (force) {
        flush = buf;
        buf = dupstr("");
    } else {
        int line_break = -1;
        for (int i = (int)L - 1; i >= 0; i--) if (buf[i] == '\n') { line_break = i; break; }
        int min_newline_flush = target_width / 3;
        if (min_newline_flush < 16) min_newline_flush = 16;
        static const char *const endings[] = {"\n\n", ".\n", "!\n", "?\n", ":\n"};
        int ends_ok = endswith_any(buf, endings, 5);
        if (line_break != -1 && (line_break >= min_newline_flush || ends_ok)) {
            flush = substr_dup(buf, 0, (size_t)line_break + 1);
            char *rest = substr_dup(buf, (size_t)line_break + 1, L);
            free(buf); buf = rest;
        } else if ((int)L >= target_width) {
            int search_start = target_width / 2;
            if (search_start < 20) search_start = 20;
            int upper = target_width + target_width / 3;
            int upper2 = target_width + 8;
            int search_end = upper > upper2 ? upper : upper2;
            if (search_end > (int)L) search_end = (int)L;
            static const char *const needles[] = {" ", "\t", ".", "!", "?", ",", ";", ":"};
            int cut = -1;
            for (size_t i = 0; i < 7; i++) {
                int c = rfind_sub(buf, needles[i], search_start, search_end);
                if (c > cut) cut = c;
            }
            if (cut != -1) {
                flush = substr_dup(buf, 0, (size_t)cut + 1);
                char *rest = substr_dup(buf, (size_t)cut + 1, L);
                free(buf); buf = rest;
            }
        }
    }
    char *remain = flush ? lstrip_dup(buf) : dupstr(buf);
    json_set(out, "buf", json_string(remain));
    json_set(out, "flush", flush ? json_string(flush) : json_null());
    free(remain); free(flush); free(buf);
    return out;
}

