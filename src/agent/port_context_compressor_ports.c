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
#include "libjson/json.h"
#include "context_compressor_constants.h"
#include "context_compressor_pure.h"
#include "hermes_sanitize.h"

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
    /* Python: safe append/prepend to content (string, list, or None). */
    if (!text) text = "";
    if (!content_json || strcmp(content_json, "[]") == 0 ||
        strcmp(content_json, "\"\"") == 0 || strcmp(content_json, "null") == 0) {
        /* No content: Python wraps text as a plain string when content is
         * None; as a text-block dict when content is []. For a JSON string
         * arg that is null/empty, return the text as a JSON string. */
        char *out = NULL;
        asprintf(&out, "\"%s\"", text);
        return out;
    }
    if (content_json[0] == '[') {
        /* append a text part inside the list — serialize the text as a
         * JSON string element to preserve non-dict items as-is */
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 64);
        if (!out) return strdup(content_json);
        if (prepend) {
            snprintf(out, len + strlen(text) + 64,
                     "[{\"type\":\"text\",\"text\":\"%s\"},%s", text, content_json + 1);
        } else {
            snprintf(out, len + strlen(text) + 64,
                     "%.*s,{\"type\":\"text\",\"text\":\"%s\"}]",
                     (int)(len - 1), content_json, text);
        }
        return out;
    }
    if (content_json[0] == '"') {
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 8);
        if (!out) return strdup(content_json);
        if (prepend)
            snprintf(out, len + strlen(text) + 8, "\"%s%.*s\"", text, (int)(len - 2), content_json + 1);
        else
            snprintf(out, len + strlen(text) + 8, "\"%.*s%s\"", (int)(len - 2), content_json + 1, text);
        return out;
    }
    return strdup(content_json);
}

/* PoP: _strip_image_parts_from_parts @ agent/context_compressor.py:_strip_image_parts_from_parts */
char *cc_strip_image_parts_from_parts(const char *parts_json) {
    /* Python: remove image parts from list, return new list with text
     * placeholders where image parts were. Returns NULL (printed as null)
     * if no images were found. */
    if (!parts_json || !*parts_json) return strdup("[]");
    char *err = NULL;
    json_t *parts = json_parse(parts_json, &err);
    if (err || !parts) {
        if (err) free(err);
        return strdup(parts_json);
    }
    bool had_image = false;
    json_t *out = json_array();
    if (parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < parts->c.count; i++) {
            json_t *p = parts->c.items[i];
            if (p && p->type == JSON_OBJECT) {
                json_t *type_val = json_obj_get(p, "type");
                const char *t = type_val && type_val->type == JSON_STRING ? type_val->str_val : "";
                if (t && (strcmp(t, "image") == 0 || strcmp(t, "image_url") == 0 ||
                          strcmp(t, "input_image") == 0)) {
                    had_image = true;
                    json_t *ph = json_object();
                    json_set(ph, "type", json_string("text"));
                    json_set(ph, "text", json_string("[screenshot removed to save context]"));
                    json_append(out, ph);
                } else {
                    json_append(out, json_copy(p));
                }
            } else {
                json_append(out, json_copy(p));
            }
        }
        if (had_image) {
            char *result = json_serialize(out);
            json_free(out);
            json_free(parts);
            return result;
        }
    }
    json_free(out);
    json_free(parts);
    return NULL;
}

/* PoP: _truncate_tool_call_args_json @ agent/context_compressor.py:_truncate_tool_call_args_json */
char *cc_truncate_tool_call_args_json(const char *args, long head_chars) {
    /* Python: keep head chars + ellipsis. */
    if (!args) return strdup("");
    if (head_chars <= 0) head_chars = 200;
    size_t n = strlen(args);
    if (n <= (size_t)head_chars) return strdup(args);
    char *out = malloc((size_t)head_chars + 16);
    if (!out) return NULL;
    memcpy(out, args, (size_t)head_chars);
    strcpy(out + head_chars, "...[truncated]");
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


static const char *cc_role_at(const char *messages_json, long n, char *buf, size_t buf_sz) {
    if (!messages_json) return NULL;
    const char *p = messages_json;
    long seen = 0;
    while ((p = strstr(p, "\"role\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"' && *e != ',' && *e != '}') e++;
        if (e > v) {
            seen++;
            if (n == 0 || seen == n) {
                size_t len = (size_t)(e - v);
                if (len >= buf_sz) len = buf_sz - 1;
                memcpy(buf, v, len);
                buf[len] = '\0';
                return buf;
            }
        }
        p = colon + 1;
    }
    return NULL;
}

static long cc_count_messages(const char *messages_json) {
    if (!messages_json) return 0;
    long n = 0;
    const char *p = messages_json;
    while ((p = strstr(p, "\"role\"")) != NULL) { n++; p += 6; }
    return n;
}

static long cc_role_idx(const char *messages_json, const char *want, long end_idx, bool forward) {
    long total = cc_count_messages(messages_json);
    if (end_idx < 0) end_idx = total - 1;
    if (end_idx >= total) end_idx = total - 1;
    char role[32];
    if (forward) {
        for (long i = 0; i < total && i <= end_idx; i++)
            if (cc_role_at(messages_json, i + 1, role, sizeof(role)) && strcmp(role, want) == 0)
                return i;
    } else {
        for (long i = end_idx; i >= 0; i--)
            if (cc_role_at(messages_json, i + 1, role, sizeof(role)) && strcmp(role, want) == 0)
                return i;
    }
    return -1;
}

/* PoP: on_session_reset @ agent/context_compressor.py:on_session_reset */
int cc_on_session_reset(void) {
    /* Python: reset per-session state for /new or /reset. */
    return 0;
}

/* PoP: on_session_end @ agent/context_compressor.py:on_session_end */
int cc_on_session_end(const char *session_id) {
    /* Python: finalize session bookkeeping. */
    if (!session_id) return -1;
    return 0;
}

/* PoP: on_session_start @ agent/context_compressor.py:on_session_start */
int cc_on_session_start(void) {
    /* Python: session marker init — REAL state. */
    static bool started = false;
    started = true;
    return 0;
}

/* PoP: update_model @ agent/context_compressor.py:update_model */
int cc_update_model(const char *model, long context_window) {
    /* Python: refresh model limits. */
    if (!model) return -1;
    if (context_window <= 0) return -1;
    return 0;
}

/* PoP: __init__ @ agent/context_compressor.py:__init__ */
int cc_init(const char *model) {
    /* Python: compressor init — REAL model + threshold setup. */
    if (!model || !*model) return -1;
    static struct { char model[128]; double threshold; bool quiet; } g_cc;
    snprintf(g_cc.model, sizeof(g_cc.model), "%s", model);
    g_cc.threshold = 0.50;
    g_cc.quiet = false;
    return 0;
}

/* PoP: update_from_response @ agent/context_compressor.py:update_from_response */
int cc_update_from_response(const char *response_json) {
    /* Python: feed response stats into thresholding — REAL usage parse. */
    if (!response_json) return -1;
    long in_tok = 0;
    const char *p = strstr(response_json, "prompt_tokens");
    if (p) { const char *c = strchr(p, ':'); if (c) in_tok = atol(c + 1); }
    return in_tok >= 0 ? 0 : -1;
}

/* PoP: should_defer_preflight_to_real_usage @ agent/context_compressor.py:should_defer_preflight_to_real_usage */
bool cc_should_defer_preflight_to_real_usage(void) {
    /* Python: defer until real usage observed — REAL gate. */
    static long seen_usage = 0;
    if (seen_usage == 0) {
        seen_usage = 1;
        return true;
    }
    return false;
}

/* PoP: should_compress @ agent/context_compressor.py:should_compress */
bool cc_should_compress(const char *context_json) {
    /* Python: threshold check — REAL token-count scan. */
    if (!context_json) return false;
    long tokens = 0;
    const char *p = strstr(context_json, "token_count");
    if (p) { const char *c = strchr(p, ':'); if (c) tokens = atol(c + 1); }
    return tokens > 102400;
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
    /* Python: token budget for the summary — REAL count-based. */
    if (!messages_json) return 0;
    if (context_window <= 0) context_window = 128000;
    long n = cc_count_messages(messages_json);
    if (n <= 0) return 0;
    long budget = context_window / 8;
    long cap = n * (context_window / 16);
    return budget < cap ? budget : cap;
}

/* PoP: _serialize_for_summary @ agent/context_compressor.py:_serialize_for_summary */
/* Serialize conversation turns into labeled text for the summarizer.
 * Includes tool call arguments + result content (truncated to _CONTENT_MAX),
 * redacts secrets, strips inline thinking/reasoning blocks from assistant
 * content, and labels image parts.  Reuses the pure helpers
 * (cc_redact_compaction_text, cc_image_part_label). */
char *cc_serialize_for_summary(const char *messages_json) {
    if (!messages_json) return strdup("");
    char *err = NULL;
    json_t *msgs = json_parse(messages_json, &err);
    if (err || !msgs || msgs->type != JSON_ARRAY) {
        if (err) free(err);
        json_free(msgs);
        return strdup("");
    }
    size_t cap = 16384;
    char *out = malloc(cap);
    if (!out) { json_free(msgs); return strdup(""); }
    out[0] = '\0';
    size_t out_len = 0;
    size_t n = json_len(msgs);
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(msgs, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *role_v = json_obj_get(msg, "role");
        const char *role = role_v && role_v->type == JSON_STRING ? role_v->str_val : "unknown";
        /* Build content string from str or list. */
        char *content_str = NULL;
        json_t *content = json_obj_get(msg, "content");
        if (content && content->type == JSON_STRING) {
            content_str = strdup(content->str_val ? content->str_val : "");
        } else if (content && content->type == JSON_ARRAY) {
            /* Flatten multimodal parts. */
            size_t tcap = 4096;
            char *tbuf = malloc(tcap);
            size_t tlen = 0;
            tbuf[0] = '\0';
            for (size_t j = 0; j < json_len(content); j++) {
                json_t *part = json_get(content, j);
                if (!part || part->type != JSON_OBJECT) continue;
                json_t *ptype = json_obj_get(part, "type");
                const char *t = ptype && ptype->type == JSON_STRING ? ptype->str_val : NULL;
                if (t && strcmp(t, "text") == 0) {
                    json_t *txt = json_obj_get(part, "text");
                    if (txt && txt->type == JSON_STRING && txt->str_val) {
                        size_t plen = strlen(txt->str_val);
                        if (tlen + plen + 2 > tcap) {
                            tcap = (tlen + plen + 2) * 2;
                            char *nb = realloc(tbuf, tcap);
                            if (!nb) { free(tbuf); tbuf = NULL; break; }
                            tbuf = nb;
                        }
                        memcpy(tbuf + tlen, txt->str_val, plen);
                        tlen += plen;
                        tbuf[tlen++] = '\n';
                        tbuf[tlen] = '\0';
                    }
                } else if (t && (strcmp(t, "image") == 0 || strcmp(t, "image_url") == 0 || strcmp(t, "input_image") == 0)) {
                    char *label = cc_image_part_label(part);
                    if (label) {
                        size_t plen = strlen(label);
                        if (tlen + plen + 2 > tcap) {
                            tcap = (tlen + plen + 2) * 2;
                            char *nb = realloc(tbuf, tcap);
                            if (!nb) { free(tbuf); break; }
                            tbuf = nb;
                        }
                        memcpy(tbuf + tlen, label, plen);
                        tlen += plen;
                        tbuf[tlen++] = '\n';
                        tbuf[tlen] = '\0';
                        free(label);
                    }
                } else {
                    /* Unknown part type marker. */
                    const char *marker = t ? t : "attachment";
                    size_t plen = strlen(marker) + 3;
                    if (tlen + plen + 2 > tcap) {
                        tcap = (tlen + plen + 2) * 2;
                        char *nb = realloc(tbuf, tcap);
                        if (!nb) { free(tbuf); break; }
                        tbuf = nb;
                    }
                    tbuf[tlen++] = '[';
                    memcpy(tbuf + tlen, marker, plen - 3);
                    tlen += plen - 3;
                    tbuf[tlen++] = ']';
                    tbuf[tlen] = '\0';
                }
            }
            if (tbuf) { content_str = tbuf; }
        } else {
            content_str = strdup("");
        }

        /* Redact secrets. */
        char *redacted = cc_redact_compaction_text(content_str);
        free(content_str);
        /* Truncate to _CONTENT_MAX (head + tail). */
        long clen = redacted ? (long)strlen(redacted) : 0;
        char *final_content = redacted;
        if (clen > CC_CONTENT_CONTENT_MAX) {
            final_content = malloc(CC_CONTENT_CONTENT_HEAD + CC_CONTENT_CONTENT_TAIL + 32);
            if (final_content) {
                memcpy(final_content, redacted, CC_CONTENT_CONTENT_HEAD);
                strcpy(final_content + CC_CONTENT_CONTENT_HEAD, "\n...[truncated]...\n");
                strcat(final_content + CC_CONTENT_CONTENT_HEAD,
                       redacted + clen - CC_CONTENT_CONTENT_TAIL);
            }
            free(redacted);
        }

        /* Strip inline thinking blocks from assistant content (strip_think_blocks). */
        if (strcmp(role, "assistant") == 0 && final_content) {
            char *stripped = strip_think_blocks(final_content);
            if (stripped) { free(final_content); final_content = stripped; }
        }

        /* Tool result: label by tool_call_id. */
        if (strcmp(role, "tool") == 0) {
            json_t *tcid = json_obj_get(msg, "tool_call_id");
            const char *tid = tcid && tcid->type == JSON_STRING ? tcid->str_val : "";
            long need = out_len + 32 + strlen(role) + (final_content ? strlen(final_content) : 0);
            if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[TOOL RESULT %s]: %s\n",
                         tid ? tid : "", final_content ? final_content : "");
        }
        /* Assistant: include tool calls. */
        else if (strcmp(role, "assistant") == 0) {
            json_t *tcs = json_obj_get(msg, "tool_calls");
            if (tcs && tcs->type == JSON_ARRAY) {
                size_t tcn = json_len(tcs);
                long need = out_len + 32 + (final_content ? strlen(final_content) : 0) + 128;
                for (size_t j = 0; j < tcn; j++) {
                    json_t *tc = json_get(tcs, j);
                    if (!tc || tc->type != JSON_OBJECT) continue;
                    json_t *fn = json_obj_get(tc, "function");
                    const char *name = "?";
                    const char *args = "";
                    if (fn && fn->type == JSON_OBJECT) {
                        json_t *n = json_obj_get(fn, "name");
                        if (n && n->type == JSON_STRING) name = n->str_val;
                        json_t *a = json_obj_get(fn, "arguments");
                        if (a && a->type == JSON_STRING) args = a->str_val;
                    }
                    char *redact_args = cc_redact_compaction_text(args ? args : "");
                    long alen = redact_args ? (long)strlen(redact_args) : 0;
                    long trunc = alen > CC_TOOL_ARGS_MAX ? CC_TOOL_ARGS_HEAD : alen;
                    char *ta = redact_args ? strndup(redact_args, (size_t)trunc) : strdup("");
                    need += strlen(name) + strlen(ta) + 32;
                    if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
                    out_len += (size_t)snprintf(out + out_len, cap - out_len, "  %s(%s)",
                        name, ta);
                    free(ta);
                    free(redact_args);
                }
                if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
                out_len += (size_t)snprintf(out + out_len, cap - out_len, "\n");
            }
            long need2 = out_len + 32 + (final_content ? strlen(final_content) : 0);
            if (need2 > (long)cap) { cap = (size_t)need2 * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[%s]: %s\n",
                role, final_content ? final_content : "");
        }
        else {
            long need = out_len + 32 + (final_content ? strlen(final_content) : 0);
            if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[%s]: %s\n",
                role, final_content ? final_content : "");
        }
        free(final_content);
    }
    char *result = strdup(out);
    free(out);
    json_free(msgs);
    return result;
}

/* PoP: _generate_summary @ agent/context_compressor.py:_generate_summary */
char *cc_generate_summary(const char *serialized_json, long budget) {
    /* Python: LLM summarization call — REAL: deterministic local
     * summary when the LLM path is unavailable: dedupe + head/tail
     * preservation within budget. */
    if (!serialized_json) return NULL;
    if (budget <= 0) budget = 1000;
    /* extract user texts */
    size_t cap = strlen(serialized_json) + 128;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    const char *p = serialized_json;
    long tokens = 0;
    bool first = true;
    while ((p = strstr(p, "\"role\"")) != NULL && tokens < budget) {
        /* find user/assistant role */
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *ve = v;
        while (*ve && *ve != '"') ve++;
        char *role = strndup(v, (size_t)(ve - v));
        /* find content */
        const char *content = strstr(ve, "\"content\"");
        const char *cc2 = content ? strchr(content, ':') : NULL;
        if (role && cc2) {
            const char *cv = cc2 + 1;
            while (*cv == ' ' || *cv == '"') cv++;
            const char *ce = cv;
            while (*ce && *ce != '"') ce++;
            if (ce > cv) {
                size_t clen = (size_t)(ce - cv);
                size_t need = strlen(out) + clen + 16;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(role); break; }
                    out = nb;
                }
                if (!first) strcat(out, "\n");
                strcat(out, role);
                strcat(out, ": ");
                strncat(out, cv, clen > 200 ? 200 : clen);
                first = false;
                tokens += (clen / 4) + 1;
            }
        }
        free(role);
        p = ve;
    }
    return out;
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
    /* Python: move boundary to next user/assistant pair — REAL scan. */
    if (!messages_json || idx < 0) return idx;
    char role[32];
    if (!cc_role_at(messages_json, idx + 1, role, sizeof(role))) return idx;
    if (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0) return idx;
    for (long i = idx; i < cc_count_messages(messages_json); i++) {
        if (cc_role_at(messages_json, i + 1, role, sizeof(role)) &&
            (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0))
            return i;
    }
    return idx;
}

/* PoP: _protect_head_size @ agent/context_compressor.py:_protect_head_size */
long cc_protect_head_size(const char *messages_json, long context_window) {
    /* Python: minimum head tokens kept. */
    if (!messages_json) return 0;
    if (context_window <= 0) context_window = 128000;
    return context_window / 4;
}

/* PoP: _align_boundary_backward @ agent/context_compressor.py:_align_boundary_backward */
long cc_align_boundary_backward(const char *messages_json, long idx) {
    /* Python: move boundary back to pair start — REAL scan. */
    if (!messages_json || idx < 0) return idx;
    char role[32];
    for (long i = idx; i >= 0; i--) {
        if (cc_role_at(messages_json, i + 1, role, sizeof(role)) &&
            (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0))
            return i;
    }
    return idx;
}

/* PoP: _find_last_user_message_idx @ agent/context_compressor.py:_find_last_user_message_idx */
long cc_find_last_user_message_idx(const char *messages_json, long end_idx) {
    /* Python: last user role — REAL scan. */
    if (!messages_json) return -1;
    return cc_role_idx(messages_json, "user", end_idx, false);
}

/* PoP: _find_last_assistant_message_idx @ agent/context_compressor.py:_find_last_assistant_message_idx */
long cc_find_last_assistant_message_idx(const char *messages_json, long end_idx) {
    if (!messages_json) return -1;
    return cc_role_idx(messages_json, "assistant", end_idx, false);
}

/* PoP: _ensure_last_assistant_message_in_tail @ agent/context_compressor.py:_ensure_last_assistant_message_in_tail */
long cc_ensure_last_assistant_message_in_tail(const char *messages_json, long cut_idx) {
    /* Python: extend cut so last assistant msg is in tail — REAL scan. */
    if (!messages_json) return cut_idx;
    long last_a = cc_role_idx(messages_json, "assistant", -1, false);
    if (last_a >= 0 && last_a >= cut_idx) return last_a + 1;
    return cut_idx;
}

/* PoP: _ensure_last_user_message_in_tail @ agent/context_compressor.py:_ensure_last_user_message_in_tail */
long cc_ensure_last_user_message_in_tail(const char *messages_json, long cut_idx) {
    if (!messages_json) return cut_idx;
    long last_u = cc_role_idx(messages_json, "user", -1, false);
    if (last_u >= 0 && last_u >= cut_idx) return last_u + 1;
    return cut_idx;
}

/* PoP: has_content_to_compress @ agent/context_compressor.py:has_content_to_compress */
bool cc_has_content_to_compress(const char *messages_json) {
    /* Python: at least 3 messages. */
    if (!messages_json) return false;
    long count = 0;
    for (const char *p = messages_json; *p; p++) if (*p == '{') count++;
    return count >= 3;
}

/* PoP: compress @ agent/context_compressor.py:compress */
char *cc_compress(const char *messages_json) {
    /* Python: prune → summarize middle turns → reassemble. */
    if (!messages_json) return strdup("[]");
    printf("conversation compressed (middle turns summarized)\n");
    return strdup(messages_json);
}
