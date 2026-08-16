#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/tools/clarify_tool.c — C11 port of tools/clarify_tool.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_clarify_tool.c ── */
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





/* ── from src/tools/port_clarify_tool.c ── */
/*
 * port_clarify_tool.c — Faithful C11 port of tools/clarify_tool.py.
 *
 * Implements: _flatten_choice, _invoke_callback, _parse_multi_select_response,
 * clarify_tool, check_clarify_requirements.
 *
 * Uses the json_node_t* JSON API (json_parse/json_object_get/...), matching
 * the sibling tools/clarify.c gateway handler.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CHOICES 4

/* ── _flatten_choice ─────────────────────────────────────────────────────── */
/* PoP: clarify_flatten_choice @ tools/clarify_tool.py:_flatten_choice */
/*
 * Coerce a single choice into its user-facing display string. Supports bare
 * strings, dict-shaped choices ({"label":"..."} etc.), and lists. Returns a
 * malloc'd string; caller frees. NULL → "". */
char *clarify_flatten_choice(const json_node_t *c)
{
    if (!c) return strdup("");
    if (c->type == JSON_STRING) {
        /* strip surrounding whitespace */
        const char *s = c->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len-1])) len--;
        char *result = malloc(len + 1);
        if (!result) return NULL;
        memcpy(result, s, len); result[len] = '\0';
        return result;
    }
    if (c->type == JSON_NULL)
        return strdup("");
    if (c->type == JSON_OBJECT) {
        const char *keys[] = {"label", "description", "text", "title"};
        for (size_t k = 0; k < sizeof(keys)/sizeof(keys[0]); k++) {
            json_node_t *v = json_object_get(c, keys[k]);
            if (v && v->type == JSON_STRING && v->str_val && v->str_val[0] &&
                strspn(v->str_val, " \t\n\r") != strlen(v->str_val)) {
                /* non-empty after strip */
                const char *s = v->str_val;
                while (*s && isspace((unsigned char)*s)) s++;
                size_t len = strlen(s);
                while (len > 0 && isspace((unsigned char)s[len-1])) len--;
                char *result = malloc(len + 1);
                if (!result) return NULL;
                memcpy(result, s, len); result[len] = '\0';
                return result;
            }
        }
        return strdup("");
    }
    if (c->type == JSON_ARRAY) {
        /* " ".join(_flatten_choice(x) for x in c) */
        char *out = NULL; size_t outcap = 0, outlen = 0;
        for (size_t i = 0; i < json_array_count(c); i++) {
            char *part = clarify_flatten_choice(json_array_get(c, i));
            size_t plen = part ? strlen(part) : 0;
            if (outlen + plen + (i > 0 ? 1 : 0) + 1 > outcap) {
                outcap = (outlen + plen + 2) * 2;
                out = realloc(out, outcap);
            }
            if (i > 0 && plen > 0) { out[outlen++] = ' '; }
            if (plen > 0) { memcpy(out + outlen, part, plen); outlen += plen; }
            free(part);
        }
        if (!out) { out = malloc(1); out[0] = '\0'; }
        else out[outlen] = '\0';
        return out;
    }
    /* fallback: stringify (numbers, bools) */
    char buf[64];
    if (c->type == JSON_NUMBER)
        snprintf(buf, sizeof(buf), "%g", c->num_val);
    else
        snprintf(buf, sizeof(buf), "%s", c->type == JSON_NULL ? "null" : "null");
    return strdup(buf);
}

/* ── _invoke_callback ────────────────────────────────────────────────────── */
/* PoP: clarify_invoke_callback @ tools/clarify_tool.py:_invoke_callback */
/* C cannot introspect a function-pointer signature the way Python inspects a
 * callable, so the C callback has a fixed signature that always accepts
 * multi_select.  The Python signature-inspection fallback (2-arg form for
 * builtins without a signature) has no C analogue — all C callbacks are typed
 * at the call site.  We invoke with multi_select always. */
typedef char *(*clarify_callback_fn)(const char *question,
                                     const char *const *choices, int n_choices,
                                     bool multi_select, void *user_data);

char *clarify_invoke_callback(clarify_callback_fn callback, void *user_data,
                              const char *question,
                              const char *const *choices, int n_choices,
                              bool multi_select)
{
    if (!callback) return NULL;
    return callback(question, choices, n_choices, multi_select, user_data);
}

/* ── _parse_multi_select_response ────────────────────────────────────────── */
/* PoP: clarify_parse_multi_select_response @ tools/clarify_tool.py:_parse_multi_select_response */
/* Parse a multi-select response into a malloc'd JSON string array of cleaned
 * choice strings.  Handles: already-a-list, JSON array, comma-separated.
 * Returns a json_node_t* array (caller: json_free). */
json_node_t *clarify_parse_multi_select_response(const json_node_t *raw_response)
{
    json_node_t *out = json_array();
    if (!out) return NULL;
    if (!raw_response) return out;

    if (raw_response->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_count(raw_response); i++) {
            json_node_t *el = json_array_get(raw_response, i);
            char *s = el ? clarify_flatten_choice(el) : NULL;
            if (s && s[0]) json_array_append(out, json_new_string(s));
            free(s);
        }
        return out;
    }

    /* string form */
    const char *raw = (raw_response->type == JSON_STRING) ? raw_response->str_val : NULL;
    if (!raw || !*raw) return out;
    /* try JSON array */
    if (raw[0] == '[') {
        char *err = NULL;
        json_node_t *parsed = json_parse(raw, &err);
        if (parsed && parsed->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_count(parsed); i++) {
                json_node_t *el = json_array_get(parsed, i);
                char *s = el ? clarify_flatten_choice(el) : NULL;
                if (s && s[0]) json_array_append(out, json_new_string(s));
                free(s);
            }
            json_free(parsed);
            return out;
        }
        if (parsed) json_free(parsed);
        if (err) free(err);
    }
    /* comma-separated */
    char buf[1024];
    strncpy(buf, raw, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    char *save = NULL;
    char *tok = strtok_r(buf, ",", &save);
    while (tok) {
        /* strip whitespace */
        while (*tok && isspace((unsigned char)*tok)) tok++;
        size_t len = strlen(tok);
        while (len > 0 && isspace((unsigned char)tok[len-1])) tok[--len] = '\0';
        if (len > 0) json_array_append(out, json_new_string(tok));
        tok = strtok_r(NULL, ",", &save);
    }
    return out;
}

/* ── clarify_tool ────────────────────────────────────────────────────────── */
/* PoP: clarify_tool @ tools/clarify_tool.py:clarify_tool
 *
 * Ask the user a question, optionally with multiple-choice options.
 * choices_json: JSON array (or NULL).  callback: platform callback (may be NULL
 *   when not in a callback-capable context — returns an error JSON).
 * Returns malloc'd JSON string (caller frees).
 *
 * faithful to Python return shape:
 *   {"question":..., "choices_offered":..., "user_response":...} */
char *clarify_tool(const char *question, json_node_t *choices_json,
                   bool multi_select,
                   clarify_callback_fn callback, void *user_data)
{
    /* Validate question */
    if (!question || !question[0] || strspn(question, " \t\n\r") == strlen(question))
        return strdup("{\"error\":\"Question text is required.\"}");
    char qbuf[1024];
    strncpy(qbuf, question, sizeof(qbuf)-1); qbuf[sizeof(qbuf)-1] = '\0';
    /* strip */
    char *qs = qbuf;
    while (*qs && isspace((unsigned char)*qs)) qs++;
    size_t qlen = strlen(qs);
    while (qlen > 0 && isspace((unsigned char)qs[qlen-1])) qs[--qlen] = '\0';

    /* Validate / trim choices */
    char *const *choice_strs = NULL; int n_choices = 0;
    char *owned[4] = {0};
    json_node_t *offered = NULL;
    char *err = NULL;

    if (choices_json && choices_json->type == JSON_ARRAY) {
        /* flatten each choice, drop empties, cap at MAX_CHOICES */
        size_t count = json_array_count(choices_json);
        if (count > MAX_CHOICES) count = MAX_CHOICES;
        for (size_t i = 0; i < count; i++) {
            json_node_t *c = json_array_get(choices_json, i);
            char *s = clarify_flatten_choice(c);
            if (s && s[0]) {
                if (n_choices < MAX_CHOICES) owned[n_choices++] = s;
                else free(s);
            } else free(s);
        }
        if (n_choices == 0) { n_choices = 0; } /* empty → open-ended */
        choice_strs = (char *const *)owned;
    }

    if (!callback)
        return strdup("{\"error\":\"Clarify tool is not available in this execution context.\"}");

    /* Invoke callback */
    char *raw_response = clarify_invoke_callback(callback, user_data, qs,
                                                   (const char *const *)choice_strs, n_choices,
                                                   multi_select && n_choices > 0);

    json_node_t *result = json_new_object();
    json_object_set(result, "question", json_new_string(qs));

    /* choices_offered */
    if (n_choices > 0) {
        offered = json_array();
        for (int i = 0; i < n_choices; i++)
            json_array_append(offered, json_new_string(choice_strs[i]));
        json_object_set(result, "choices_offered", offered);
    } else {
        json_object_set(result, "choices_offered", json_new_null());
    }

    json_node_t *user_response;
    if (multi_select && n_choices > 0) {
        /* parse into list */
        json_node_t *parsed = NULL;
        if (raw_response && raw_response[0] == '[') {
            parsed = json_parse(raw_response, &err);
        }
        if (parsed && parsed->type == JSON_ARRAY) {
            user_response = clarify_parse_multi_select_response(parsed);
        } else {
            /* wrap raw as single-element or parse as multi */
            json_node_t *tmp = json_new_string(raw_response ? raw_response : "(no input)");
            user_response = clarify_parse_multi_select_response(tmp);
            json_free(tmp);
        }
        if (parsed) { json_free(parsed); parsed = NULL; }
        if (err) free(err);
    } else {
        char resp_buf[4096] = "(no input)";
        if (raw_response) {
            strncpy(resp_buf, raw_response, sizeof(resp_buf)-1);
            resp_buf[sizeof(resp_buf)-1] = '\0';
            size_t len = strlen(resp_buf);
            while (len > 0 && (resp_buf[len-1] == '\n' || resp_buf[len-1] == '\r'))
                resp_buf[--len] = '\0';
        }
        user_response = json_new_string(resp_buf);
    }
    json_object_set(result, "user_response", user_response);

    char *json_out = json_serialize(result);
    json_free(result);
    for (int i = 0; i < n_choices; i++) free(owned[i]);
    free(raw_response);
    return json_out;
}

/* ── check_clarify_requirements ──────────────────────────────────────────── */
/* PoP: check_clarify_requirements @ tools/clarify_tool.py:check_clarify_requirements */
bool check_clarify_requirements(void)
{
    return true;
}

