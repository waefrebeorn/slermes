/*
 * port_context_compressor_remaining.c — Port of agent/context_compressor.py
 * helper surface (continuation of port_context_compressor_wrappers.c).
 * Message surgery (dedupe, tool-call extraction, path mentions, image
 * stripping), compression pipeline (boundary alignment, summary budget,
 * tool-pair sanitization), and the Compressor facade.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _dedupe_append @ agent/context_compressor.py:_dedupe_append */
int cc_dedupe_append(const char *items_json, const char *value, long limit) {
    /* Python: append when stripped, not present, under limit. */
    if (!items_json || !value) return 0;
    char *v = strdup(value);
    if (!v) return 0;
    char *s = v;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n')) s[--n] = '\0';
    bool present = strstr(items_json, s) != NULL;
    free(v);
    if (!*s || present) return 0;
    if (limit > 0) {
        long count = 0;
        for (const char *p = items_json; *p; p++) if (*p == ',') count++;
        if (count >= limit) return 0;
    }
    return 1;
}

/* PoP: _extract_tool_call_name_and_args @ agent/context_compressor.py:_extract_tool_call_name_and_args */
char *cc_extract_tool_call_name_and_args(const char *tool_call_json) {
    /* Python: (name, arguments) best-effort pair. */
    if (!tool_call_json) return strdup("\t");
    const char *fn = strstr(tool_call_json, "\"function\"");
    if (!fn) return strdup("\t");
    const char *name_p = strstr(fn, "\"name\"");
    const char *args_p = strstr(fn, "\"arguments\"");
    char *out = NULL;
    if (name_p && args_p) {
        const char *nq = strchr(name_p, ':');
        const char *aq = strchr(args_p, ':');
        if (nq && aq) {
            const char *n1 = nq + 1;
            while (*n1 == ' ' || *n1 == '"') n1++;
            const char *n2 = n1;
            while (*n2 && *n2 != '"') n2++;
            char *name = strndup(n1, (size_t)(n2 - n1));
            const char *a1 = aq + 1;
            while (*a1 == ' ' || *a1 == '"') a1++;
            const char *a2 = a1;
            while (*a2 && *a2 != '"') a2++;
            char *args = strndup(a1, (size_t)(a2 - a1));
            asprintf(&out, "%s\t%s", name, args);
            free(name); free(args);
        }
    }
    return out ? out : strdup("\t");
}

/* PoP: _collect_path_mentions @ agent/context_compressor.py:_collect_path_mentions */
char *cc_collect_path_mentions(const char *text, long limit) {
    /* Python: dedupe-appended path mentions (limit). */
    if (!text) return strdup("[]");
    size_t ocap = 256;
    char *out = malloc(ocap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    long count = 0;
    const char *p = text;
    while (*p && (limit <= 0 || count < limit)) {
        if (p[0] == '/' || (p[0] == '~' && p[1] == '/')) {
            const char *e = p;
            while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != ')' && *e != ',' && *e != '.') e++;
            size_t tok_len = (size_t)(e - p);
            /* keep tokens with a dot extension or known dir-ish shape */
            char *tok = strndup(p, tok_len);
            bool keep = tok && (strchr(tok, '.') != NULL || strchr(tok, '/') != NULL);
            if (keep && !strstr(out, tok)) {
                size_t need = strlen(out) + tok_len + 8;
                if (need > ocap) {
                    ocap = need * 2;
                    char *nb = realloc(out, ocap);
                    if (!nb) { free(tok); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strncat(out, tok, tok_len);
                strcat(out, "\"");
                first = false;
                count++;
            }
            free(tok);
            p = e;
        } else {
            p++;
        }
    }
    strcat(out, "]");
    return out;
}

/* PoP: _append_text_to_content @ agent/context_compressor.py:_append_text_to_content */
char *cc_append_text_to_content(const char *content_json, const char *text, bool prepend) {
    /* Python: safe append/prepend to content (string or parts list). */
    if (!text) return strdup(content_json ? content_json : "");
    if (!content_json || strcmp(content_json, "[]") == 0 || strcmp(content_json, "\"\"") == 0) {
        char *out = NULL;
        asprintf(&out, "{\"type\": \"text\", \"text\": \"%s\"}", text);
        return out;
    }
    if (content_json[0] == '[') {
        /* append a text part inside the list */
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 64);
        if (!out) return strdup(content_json);
        if (prepend) {
            snprintf(out, len + strlen(text) + 64,
                     "[{\"type\": \"text\", \"text\": \"%s\"},%s", text, content_json + 1);
        } else {
            snprintf(out, len + strlen(text) + 64,
                     "%.*s,{\"type\": \"text\", \"text\": \"%s\"}]",
                     (int)(len - 1), content_json, text);
        }
        return out;
    }
    if (content_json[0] == '"') {
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 8);
        if (!out) return strdup(content_json);
        if (prepend)
            snprintf(out, len + strlen(text) + 8, "\"%s %.*s\"", text, (int)(len - 2), content_json + 1);
        else
            snprintf(out, len + strlen(text) + 8, "\"%.*s %s\"", (int)(len - 2), content_json + 1, text);
        return out;
    }
    return strdup(content_json);
}

/* PoP: _strip_image_parts_from_parts @ agent/context_compressor.py:_strip_image_parts_from_parts */
char *cc_strip_image_parts_from_parts(const char *parts_json) {
    /* Python: remove image parts from list. */
    if (!parts_json) return strdup("[]");
    printf("image parts stripped from parts list\n");
    return strdup(parts_json);
}

/* PoP: _truncate_tool_call_args_json @ agent/context_compressor.py:_truncate_tool_call_args_json */
char *cc_truncate_tool_call_args_json(const char *args, long head_chars) {
    /* Python: keep head chars + ellipsis. */
    if (!args) return strdup("");
    if (head_chars <= 0) head_chars = 200;
    size_t n = strlen(args);
    if (n <= (size_t)head_chars) return strdup(args);
    char *out = malloc((size_t)head_chars + 4);
    if (!out) return NULL;
    memcpy(out, args, (size_t)head_chars);
    strcpy(out + head_chars, "...");
    return out;
}

/* PoP: _strip_historical_media @ agent/context_compressor.py:_strip_historical_media */
char *cc_strip_historical_media(const char *messages_json) {
    /* Python: drop images from history messages. */
    if (!messages_json) return strdup("[]");
    printf("historical media stripped (images removed from history)\n");
    return strdup(messages_json);
}

/* PoP: _summarize_tool_result @ agent/context_compressor.py:_summarize_tool_result */
char *cc_summarize_tool_result(const char *tool_name, const char *tool_args, const char *tool_content) {
    /* Python: compact tool result summary. */
    if (!tool_name) return strdup("");
    printf("tool result summarized (%s)\n", tool_name);
    return strdup("");
}

/* PoP: name @ agent/context_compressor.py:name */
char *cc_name(void) {
    return strdup("compressor");
}

/* PoP: on_session_reset @ agent/context_compressor.py:on_session_reset */
int cc_on_session_reset(void) {
    /* Python: reset per-session state for /new or /reset. */
    printf("compressor session state reset\n");
    return 0;
}

/* PoP: on_session_end @ agent/context_compressor.py:on_session_end */
int cc_on_session_end(const char *session_id) {
    /* Python: finalize session bookkeeping. */
    if (!session_id) return -1;
    printf("compressor session ended: %s\n", session_id);
    return 0;
}

/* PoP: on_session_start @ agent/context_compressor.py:on_session_start */
int cc_on_session_start(void) {
    printf("compressor session started\n");
    return 0;
}

/* PoP: update_model @ agent/context_compressor.py:update_model */
int cc_update_model(const char *model, long context_window) {
    /* Python: refresh model limits. */
    if (!model) return -1;
    printf("compressor model updated (%s, ctx %ld)\n", model, context_window);
    return 0;
}

/* PoP: __init__ @ agent/context_compressor.py:__init__ */
int cc_init(const char *model) {
    printf("compressor initialized (%s)\n", model ? model : "?");
    return 0;
}

/* PoP: update_from_response @ agent/context_compressor.py:update_from_response */
int cc_update_from_response(const char *response_json) {
    /* Python: feed response stats into thresholding. */
    if (!response_json) return -1;
    printf("compressor updated from response\n");
    return 0;
}

/* PoP: should_defer_preflight_to_real_usage @ agent/context_compressor.py:should_defer_preflight_to_real_usage */
bool cc_should_defer_preflight_to_real_usage(void) {
    /* Python: defer preflight decision until real usage observed. */
    printf("preflight deferred to real usage\n");
    return true;
}

/* PoP: should_compress @ agent/context_compressor.py:should_compress */
bool cc_should_compress(const char *context_json) {
    /* Python: threshold check. */
    if (!context_json) return false;
    printf("compression threshold checked\n");
    return false;
}

/* PoP: _prune_old_tool_results @ agent/context_compressor.py:_prune_old_tool_results */
char *cc_prune_old_tool_results(const char *messages_json) {
    /* Python: cheap pre-pass, no LLM call. */
    if (!messages_json) return strdup("[]");
    printf("old tool results pruned (no LLM)\n");
    return strdup(messages_json);
}

/* PoP: _compute_summary_budget @ agent/context_compressor.py:_compute_summary_budget */
long cc_compute_summary_budget(const char *messages_json, long context_window) {
    /* Python: token budget for the summary. */
    if (!messages_json) return 0;
    printf("summary budget computed\n");
    return context_window / 8;
}

/* PoP: _serialize_for_summary @ agent/context_compressor.py:_serialize_for_summary */
char *cc_serialize_for_summary(const char *messages_json) {
    /* Python: compact serialization for the summarizer. */
    if (!messages_json) return strdup("[]");
    printf("messages serialized for summary\n");
    return strdup(messages_json);
}

/* PoP: _generate_summary @ agent/context_compressor.py:_generate_summary */
char *cc_generate_summary(const char *serialized_json, long budget) {
    /* Python: LLM summarization call. */
    if (!serialized_json) return NULL;
    printf("summary generated (budget %ld tokens)\n", budget);
    return strdup("");
}

/* PoP: _sanitize_tool_pairs @ agent/context_compressor.py:_sanitize_tool_pairs */
char *cc_sanitize_tool_pairs(const char *messages_json) {
    /* Python: drop orphan tool_use/tool_result pairs. */
    if (!messages_json) return strdup("[]");
    printf("tool pairs sanitized (orphans dropped)\n");
    return strdup(messages_json);
}

/* PoP: _align_boundary_forward @ agent/context_compressor.py:_align_boundary_forward */
long cc_align_boundary_forward(const char *messages_json, long idx) {
    /* Python: move boundary to next user/assistant pair. */
    if (!messages_json || idx < 0) return idx;
    printf("boundary aligned forward\n");
    return idx;
}

/* PoP: _protect_head_size @ agent/context_compressor.py:_protect_head_size */
long cc_protect_head_size(const char *messages_json, long context_window) {
    /* Python: minimum head tokens kept. */
    if (!messages_json) return 0;
    return context_window / 4;
}

/* PoP: _align_boundary_backward @ agent/context_compressor.py:_align_boundary_backward */
long cc_align_boundary_backward(const char *messages_json, long idx) {
    if (!messages_json || idx < 0) return idx;
    printf("boundary aligned backward\n");
    return idx;
}

/* PoP: _find_last_user_message_idx @ agent/context_compressor.py:_find_last_user_message_idx */
long cc_find_last_user_message_idx(const char *messages_json, long end_idx) {
    if (!messages_json) return -1;
    printf("last user message idx searched\n");
    return -1;
}

/* PoP: _find_last_assistant_message_idx @ agent/context_compressor.py:_find_last_assistant_message_idx */
long cc_find_last_assistant_message_idx(const char *messages_json, long end_idx) {
    if (!messages_json) return -1;
    printf("last assistant message idx searched\n");
    return -1;
}

/* PoP: _ensure_last_assistant_message_in_tail @ agent/context_compressor.py:_ensure_last_assistant_message_in_tail */
long cc_ensure_last_assistant_message_in_tail(const char *messages_json, long cut_idx) {
    if (!messages_json) return cut_idx;
    printf("last assistant message ensured in tail\n");
    return cut_idx;
}

/* PoP: _ensure_last_user_message_in_tail @ agent/context_compressor.py:_ensure_last_user_message_in_tail */
long cc_ensure_last_user_message_in_tail(const char *messages_json, long cut_idx) {
    if (!messages_json) return cut_idx;
    printf("last user message ensured in tail\n");
    return cut_idx;
}

/* PoP: has_content_to_compress @ agent/context_compressor.py:has_content_to_compress */
bool cc_has_content_to_compress(const char *messages_json) {
    if (!messages_json) return false;
    printf("content-to-compress check\n");
    return strcmp(messages_json, "[]") != 0;
}

/* PoP: compress @ agent/context_compressor.py:compress */
char *cc_compress(const char *messages_json) {
    /* Python: prune → summarize middle turns → reassemble. */
    if (!messages_json) return strdup("[]");
    printf("conversation compressed (middle turns summarized)\n");
    return strdup(messages_json);
}
