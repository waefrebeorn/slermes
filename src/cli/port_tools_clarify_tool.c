/*
 * port_tools_clarify_tool.c — C port of tools/clarify_tool.py
 */

#include "hermes_logger.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* PoP: cli_tools_clarify_tool_check_clarify_requirements @ tools/clarify_tool.py:check_clarify_requirements */
/* Clarify tool has no external requirements — always available. */
bool cli_tools_clarify_tool_check_clarify_requirements(void) {
    return true;
}

#define MAX_CHOICES 4

/* PoP: cli_tools_clarify_tool_clarify_tool @ tools/clarify_tool.py:clarify_tool */

/* Port of Python tools/clarify_tool.py:clarify_tool */
/* Ask the user a question, optionally with multiple-choice options. */
char *cli_tools_clarify_tool_clarify_tool(
    const char *question,
    const char **choices, int choice_count)
{
    if (!question || !*question) {
        return strdup("{\"error\":\"Question text is required.\"}");
    }

    /* Validate and trim choices */
    if (choices && choice_count > MAX_CHOICES) {
        return strdup("{\"error\":\"Too many choices (max 4).\"}");
    }

    /* Build JSON response with question and choices */
    size_t buf_size = 4096;
    char *json = (char *)malloc(buf_size);
    if (!json) return NULL;

    int pos = 0;
    pos += snprintf(json + pos, buf_size - pos,
        "{\"question\":\"%s\"", question);

    if (choices && choice_count > 0) {
        pos += snprintf(json + pos, buf_size - pos, ",\"choices\":[");
        for (int i = 0; i < choice_count; i++) {
            if (i > 0) pos += snprintf(json + pos, buf_size - pos, ",");
            pos += snprintf(json + pos, buf_size - pos, "\"%s\"",
                choices[i] ? choices[i] : "");
        }
        pos += snprintf(json + pos, buf_size - pos, "]");
    }

    pos += snprintf(json + pos, buf_size - pos, "}");

    return json;
}

/* --- multi-select response parsing -------------------------------------- */

/* Strip leading/trailing whitespace from a heap string (in place). */
static void pcs_trim(char *s)
{
    if (!s) return;
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

/* str() helper for a JSON node: string → its value; others → serialized text. */
static char *pcs_strval(const json_t *node)
{
    if (!node) return strdup("");
    if (node->type == JSON_STRING) return strdup(node->str_val ? node->str_val : "");
    char *s = json_serialize(node);
    return s ? s : strdup("");
}

/* PoP: pcs_parse_multi_select_response @ tools/clarify_tool.py:_parse_multi_select_response */
/* Parse a multi-select response into a list of cleaned choice strings.
 * Handles: already-a-JSON-array, JSON array text, comma-separated text.
 * Returns a malloc'd array of malloc'd strings; *out_count is set.
 * Caller frees via: for(i)free(out[i]); free(out). */
char **pcs_parse_multi_select_response(const char *raw_response, size_t *out_count)
{
    if (out_count) *out_count = 0;
    /* 1. Try interpreting raw_response as a JSON array. */
    if (raw_response && raw_response[0] == '[') {
        char *err = NULL;
        json_t *parsed = json_parse(raw_response, &err);
        free(err);
        if (parsed && parsed->type == JSON_ARRAY) {
            size_t n = json_len(parsed);
            char **out = n ? malloc(n * sizeof(*out)) : NULL;
            size_t k = 0;
            for (size_t i = 0; i < n; i++) {
                char *s = pcs_strval(json_get(parsed, i));
                pcs_trim(s);
                if (*s) out[k++] = s;   /* drop empties */
                else free(s);
            }
            json_free(parsed);
            if (out_count) *out_count = k;
            /* shrink to exact count */
            if (k == 0) { free(out); return NULL; }
            char **shrank = realloc(out, k * sizeof(*out));
            return shrank ? shrank : out;
        }
        if (parsed) json_free(parsed);
    }
    /* 2. Fall back to comma-separated split. */
    size_t count = 0;
    size_t cap = 8;
    char **out = malloc(cap * sizeof(*out));
    if (!out) { if (out_count) *out_count = 0; return NULL; }
    if (!raw_response) { if (out_count) *out_count = 0; return out; }
    /* split on commas */
    const char *p = raw_response;
    while (*p) {
        /* gather one segment */
        const char *seg = p;
        size_t slen = 0;
        const char *c = p;
        while (*c && *c != ',') { c++; slen++; }
        char *piece = strndup(seg, slen);
        pcs_trim(piece);
        if (*piece) {
            if (count >= cap) { cap *= 2; out = realloc(out, cap * sizeof(*out)); }
            out[count++] = piece;
        } else {
            free(piece);
        }
        p = (*c == '\0') ? c : c + 1;
        if (*c == '\0') break;
    }
    if (out_count) *out_count = count;
    return out;
}



