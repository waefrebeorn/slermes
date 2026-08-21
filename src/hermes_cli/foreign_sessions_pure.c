#define _POSIX_C_SOURCE 200809L
/* foreign_sessions_pure.c - Faithful C11 port of the pure, deterministic
 * helpers in hermes_cli/foreign_sessions.py (import sessions from Claude Code
 * / Codex CLI).
 *
 * Ports (PoP):
 *   _WRAPPER_TAG_RE + _is_wrapper_text -> foreign_sessions_is_wrapper_text
 *   _flatten_blocks                    -> foreign_sessions_flatten_blocks
 *   _merge_turns                       -> foreign_sessions_merge_turns
 *   ForeignSession.label               -> foreign_sessions_session_label
 *
 * File discovery / JSONL parsing / cwd extraction are NOT ported (FS + JSONL
 * line iteration, not pure logic).
 *
 * _flatten_blocks takes a JSON string (the message "content") and a source
 * ("claude" | "codex"); returns malloc'd flattened text. _merge_turns takes a
 * JSON array of [role, text] pairs and returns a JSON array of
 * {"role","content"} objects. All strings are malloc'd - free() them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FS_TITLE_MAX 60

/* _WRAPPER_TAG_RE = ^<(user_instructions|environment_context|
 *   recommended_plugins|skills_instructions|permissions[_-]instructions|
 *   turn_context|command-name|command-message|local-command-stdout|
 *   system-reminder)\b  (case-insensitive)  */
/* PoP: foreign_sessions_is_wrapper_text @ hermes_cli/foreign_sessions.py:_is_wrapper_text */

int foreign_sessions_is_wrapper_text(const char *text) {
    if (!text) return 0;
    while (*text == ' ' || *text == '\t') text++;
    static const char *tags[] = {
        "user_instructions", "environment_context", "recommended_plugins",
        "skills_instructions", "permissions_instructions", "permissions-instructions",
        "turn_context", "command-name", "command-message",
        "local-command-stdout", "system-reminder", NULL
    };
    if (text[0] != '<') return 0;
    /* match <tag (case-insensitive) followed by end-of-word or another char */
    for (int t = 0; tags[t]; t++) {
        size_t tl = strlen(tags[t]);
        if (strncasecmp(text + 1, tags[t], tl) == 0) {
            char c = text[1 + tl];
            if (c == '>' || c == '\0' || c == ' ' || c == '\t' || c == '\n')
                return 1;
        }
    }
    return 0;
}

/* _flatten_blocks(content, source)  */
/* PoP: foreign_sessions_flatten_blocks @ hermes_cli/foreign_sessions.py:_flatten_blocks */

char *foreign_sessions_flatten_blocks(const char *content_json, const char *source) {
    json_t *content = json_parse(content_json, NULL);
    /* size parts generously */
    char *out = (char *)malloc(1);
    out[0] = '\0';
    size_t outcap = 1, outlen = 0;

    /* helper to append a stripped part with "\n\n" separator */
    #define APPEND_PART(p) do { \
        char *sp = strdup(p); \
        /* strip */ \
        char *q = sp; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++; \
        size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r')) q[--L]=0; \
        if (*q) { \
            size_t need = outlen + (outlen?2:0) + L + 1; \
            out = (char *)realloc(out, need); outcap = need; \
            if (outlen) { strcat(out, "\n\n"); outlen += 2; } \
            strcat(out, q); outlen += L; \
        } \
        free(sp); \
    } while (0)

    if (content) {
        if (content->type == JSON_STRING) {
            APPEND_PART(content->str_val ? content->str_val : "");
        } else if (content->type == JSON_ARRAY) {
            size_t n = json_len(content);
            for (size_t i = 0; i < n; i++) {
                json_t *block = json_array_get(content, i);
                if (!block) continue;
                if (block->type == JSON_STRING) {
                    APPEND_PART(block->str_val ? block->str_val : "");
                    continue;
                }
                if (block->type != JSON_OBJECT) continue;
                const char *btype = json_get_str(block, "type", "");
                if (strcmp(btype, "text") == 0 ||
                    strcmp(btype, "input_text") == 0 ||
                    strcmp(btype, "output_text") == 0) {
                    const char *t = json_get_str(block, "text", "");
                    if (t && *t) APPEND_PART(t);
                } else if (strcmp(btype, "tool_use") == 0) {
                    const char *name = json_get_str(block, "name", "");
                    char buf[64];
                    snprintf(buf, sizeof(buf), "[ran tool: %s]", name && *name ? name : "tool");
                    APPEND_PART(buf);
                } else if (strcmp(btype, "image") == 0) {
                    APPEND_PART("[image]");
                }
                /* tool_result, thinking, redacted_thinking, reasoning, unknown -> skip */
            }
        }
    }
    #undef APPEND_PART
    json_free(content);
    return out;
}

/* _merge_turns(raw_turns_json) -> JSON array of {role, content}  */
/* PoP: foreign_sessions_merge_turns @ hermes_cli/foreign_sessions.py:_merge_turns */

char *foreign_sessions_merge_turns(const char *raw_turns_json) {
    json_t *raw = json_parse(raw_turns_json, NULL);
    /* Build output as JSON manually for clarity. */
    char *out = (char *)malloc(2);
    out[0] = '['; out[1] = '\0';
    size_t outlen = 1;

    if (raw && raw->type == JSON_ARRAY) {
        /* merged list as (role, text) pairs kept in temp arrays */
        char **mrole = NULL; char **mtext = NULL;
        size_t mcap = 0, mcount = 0;
        size_t n = json_len(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *pair = json_array_get(raw, i);
            if (!pair || pair->type != JSON_ARRAY) continue;
            json_t *rj = json_array_get(pair, 0);
            json_t *tj = json_array_get(pair, 1);
            if (!rj || !tj) continue;
            const char *role = (rj->type == JSON_STRING) ? rj->str_val : "";
            const char *text = (tj->type == JSON_STRING) ? tj->str_val : "";
            /* strip text */
            char *tx = strdup(text);
            char *q = tx; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') q++;
            size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r')) q[--L]=0;
            if (!*q) { free(tx); continue; }
            if (mcount && strcmp(mrole[mcount-1], role) == 0) {
                /* merge */
                size_t oldlen = strlen(mtext[mcount-1]);
                char *nt = (char *)realloc(mtext[mcount-1], oldlen + 2 + L + 1);
                strcat(nt, "\n\n"); strcat(nt, q);
                mtext[mcount-1] = nt;
            } else {
                if (mcount >= mcap) {
                    mcap = mcap ? mcap*2 : 8;
                    mrole = (char **)realloc(mrole, mcap*sizeof(char*));
                    mtext = (char **)realloc(mtext, mcap*sizeof(char*));
                }
                mrole[mcount] = strdup(role);
                mtext[mcount] = strdup(q);
                mcount++;
            }
            free(tx);
        }
        /* leading assistant -> prepend user stub */
        if (mcount && strcmp(mrole[0], "assistant") == 0) {
            /* shift */
            mrole = (char **)realloc(mrole, (mcount+1)*sizeof(char*));
            mtext = (char **)realloc(mtext, (mcount+1)*sizeof(char*));
            for (size_t k = mcount; k > 0; k--) { mrole[k]=mrole[k-1]; mtext[k]=mtext[k-1]; }
            mrole[0] = strdup("user");
            mtext[0] = strdup("(imported conversation begins with an assistant reply)");
            mcount++;
        }
        for (size_t i = 0; i < mcount; i++) {
            char buf[2048];
            snprintf(buf, sizeof(buf), "%s{\"role\":\"%s\",\"content\":",
                     (i? ",":""), mrole[i]);
            /* JSON-escape content */
            json_t *escnode = json_string(mtext[i]);
            char *esc = escnode ? json_serialize(escnode) : NULL;
            if (escnode) json_free(escnode);
            strcat(out, buf);
            if (esc) { strcat(out, esc); free(esc); }
            else strcat(out, "\"\"");
            strcat(out, "}");
            free(mrole[i]); free(mtext[i]);
        }
        free(mrole); free(mtext);
    }
    json_free(raw);
    out = (char *)realloc(out, outlen + 2);
    strcat(out, "]");
    return out;
}

/* ForeignSession.label: [<name>] <title[:60]>  */
/* PoP: foreign_sessions_session_label @ hermes_cli/foreign_sessions.py:ForeignSession.label */

char *foreign_sessions_session_label(const char *source, const char *title_guess,
                                     const char *stem) {
    const char *name = "?";
    if (source && strcmp(source, "claude") == 0) name = "Claude Code";
    else if (source && strcmp(source, "codex") == 0) name = "Codex CLI";
    else if (source && *source) name = source;

    const char *t = (title_guess && *title_guess) ? title_guess : (stem ? stem : "");
    /* strip */
    char *ts = strdup(t);
    char *q = ts; while (*q==' '||*q=='\t') q++;
    size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t')) q[--L]=0;
    if (!*q) {
        /* fall back to stem */
        free(ts);
        ts = strdup(stem ? stem : "");
    }
    /* truncate to FS_TITLE_MAX */
    if (strlen(ts) > FS_TITLE_MAX) ts[FS_TITLE_MAX] = '\0';
    char *out = (char *)malloc(strlen(name) + strlen(ts) + 8);
    snprintf(out, strlen(name)+strlen(ts)+8, "[%s] %s", name, ts);
    free(ts);
    return out;
}

/* ── JSONL line parsing + session parsers ────────────────────────────────
 * The Python originals take a file path and iterate JSONL lines. We port the
 * pure logic: the caller passes the already-read JSONL *content*; file IO
 * stays in the thin Python wrappers / harness. _read_json_lines yields parsed
 * dict objects, skipping blank/unparseable lines (mirrors the Python
 * generator). */

/* Parse a JSONL string into a malloc'd JSON array of parsed line objects
 * (only dicts). Caller frees. Returns a (possibly empty) JSON array. */
static json_t *fs_read_json_lines(const char *jsonl) {
    json_t *arr = json_new_array();
    if (!jsonl) return arr;
    const char *p = jsonl;
    size_t len = strlen(jsonl);
    while (p < jsonl + len) {
        /* find end of line */
        const char *nl = p;
        while (nl < jsonl + len && *nl != '\n') nl++;
        /* line is [p, nl); strip trailing \r */
        size_t llen = (size_t)(nl - p);
        while (llen > 0 && (p[llen-1] == '\r' || p[llen-1] == ' ' || p[llen-1] == '\t')) llen--;
        while (*p == ' ' || *p == '\t') { p++; if (llen) llen--; }
        if (llen > 0) {
            char *line = (char *)malloc(llen + 1);
            memcpy(line, p, llen); line[llen] = '\0';
            json_t *obj = json_parse(line, NULL);
            free(line);
            if (obj && obj->type == JSON_OBJECT) json_array_append(arr, obj);
            else if (obj) json_free(obj);
        }
        p = (*nl == '\n') ? nl + 1 : nl;
    }
    return arr;
}

/* First user-role turn's first line, truncated to FS_TITLE_MAX*2. Mirrors
 * _first_user_line(turns) on the merged {role,content} array. Returns malloc'd
 * string (caller frees) or NULL. */
static char *fs_first_user_line(const json_t *merged) {
    if (!merged || merged->type != JSON_ARRAY) return NULL;
    size_t n = json_len(merged);
    for (size_t i = 0; i < n; i++) {
        json_t *o = json_array_get(merged, i);
        if (!o || o->type != JSON_OBJECT) continue;
        const char *role = json_get_str(o, "role", "");
        if (strcmp(role, "user") != 0) continue;
        const char *content = json_get_str(o, "content", "");
        /* first line */
        const char *nl = strchr(content, '\n');
        size_t flen = nl ? (size_t)(nl - content) : strlen(content);
        while (flen > 0 && (content[flen-1]==' '||content[flen-1]=='\t')) flen--;
        size_t cap = flen < FS_TITLE_MAX*2 ? flen : FS_TITLE_MAX*2;
        char *res = (char *)malloc(cap + 1);
        memcpy(res, content, cap); res[cap] = '\0';
        if (!*res) { free(res); continue; }
        return res;
    }
    return NULL;
}

/* parse_claude_session(content_jsonl) -> JSON object
 *   {turns:[{role,content}], cwd, title_guess, session_id}
 * Mirrors hermes_cli/foreign_sessions.py:parse_claude_session exactly (turn
 * logic, skip rules, wrapper-text filtering, merge). */
/* PoP: foreign_sessions_parse_claude_session @ hermes_cli/foreign_sessions.py:parse_claude_session */
char *foreign_sessions_parse_claude_session(const char *jsonl) {
    json_t *lines = fs_read_json_lines(jsonl);
    /* raw turns as JSON array of [role, text] */
    char *rawj = (char *)malloc(2); rawj[0]='['; rawj[1]='\0';
    size_t rawlen = 1;
    const char *cwd = NULL;
    const char *session_id = NULL;
    char *summary = NULL;

    size_t n = json_len(lines);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_array_get(lines, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        const char *otype = json_get_str(obj, "type", "");
        if (strcmp(otype, "summary") == 0) {
            const char *s = json_get_str(obj, "summary", "");
            if (s && *s) { char *st = strdup(s); char *q=st; while(*q==' '||*q=='\t')q++; size_t L=strlen(q); while(L>0&&(q[L-1]==' '||q[L-1]=='\t'||q[L-1]=='\n'||q[L-1]=='\r'))q[--L]=0; if(*q){ free(summary); summary=strdup(q);} free(st); }
            continue;
        }
        if (strcmp(otype, "user") != 0 && strcmp(otype, "assistant") != 0) continue;
        if (json_get_bool(obj, "isSidechain", 0) || json_get_bool(obj, "isMeta", 0)) continue;
        if (!cwd) { const char *c = json_get_str(obj, "cwd", ""); if (*c) cwd = c; }
        if (!session_id) { const char *sid = json_get_str(obj, "sessionId", ""); if (*sid) session_id = sid; }
        json_t *message = json_object_get(obj, "message");
        if (!message || message->type != JSON_OBJECT) continue;
        const char *role = json_get_str(message, "role", "");
        if (strcmp(role, "user") != 0 && strcmp(role, "assistant") != 0) continue;
        char *content_json = json_dumps(message, 0);
        char *text = NULL;
        { /* _flatten_blocks(message content, source=claude) */
            json_t *mc = json_object_get(message, "content");
            if (mc) { char *mcj = json_dumps(mc, 0); text = foreign_sessions_flatten_blocks(mcj, "claude"); free(mcj); }
            else text = strdup("");
        }
        free(content_json);
        if (!text || !*text) { free(text); continue; }
        if (strcmp(role, "user") == 0 && foreign_sessions_is_wrapper_text(text)) { free(text); continue; }
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s[\"%s\",%s", (rawlen>1?",":""), role, "\"\"]");
        /* escape text into a JSON string */
        json_t *escnode = json_string(text); char *esc = json_serialize(escnode); json_free(escnode);
        /* replace the trailing ]\"] placeholder: build [role, "text"] */
        char *pair = (char *)malloc(strlen(role) + strlen(esc) + 8);
        snprintf(pair, strlen(role)+strlen(esc)+8, "[\"%s\",%s]", role, esc);
        free(esc);
        size_t need = rawlen + strlen(pair) + 1;
        rawj = (char *)realloc(rawj, need);
        strcat(rawj, pair);
        rawlen += strlen(pair);
        free(pair); free(text);
    }
    rawj = (char *)realloc(rawj, rawlen + 2); strcat(rawj, "]");

    char *merged = foreign_sessions_merge_turns(rawj);
    free(rawj);
    json_t *merged_obj = json_parse(merged, NULL);
    free(merged);

    const char *title = summary ? summary : NULL;
    char *ful = fs_first_user_line(merged_obj);
    if (!title && ful) title = ful;

    /* build output object */
    json_t *out = json_new_object();
    json_object_set(out, "turns", merged_obj ? merged_obj : json_new_array());
    json_object_set(out, "cwd", json_string(cwd ? cwd : ""));
    json_object_set(out, "title_guess", json_string(title ? title : ""));
    json_object_set(out, "session_id", json_string(session_id ? session_id : ""));
    char *res = json_dumps(out, 0);
    json_free(out); if (merged_obj) json_free(merged_obj);
    free(summary); free(ful);
    json_free(lines);
    return res;
}

/* parse_codex_session(content_jsonl) -> JSON object. Mirrors
 * hermes_cli/foreign_sessions.py:parse_codex_session. */
/* PoP: foreign_sessions_parse_codex_session @ hermes_cli/foreign_sessions.py:parse_codex_session */
char *foreign_sessions_parse_codex_session(const char *jsonl) {
    json_t *lines = fs_read_json_lines(jsonl);
    char *rawj = (char *)malloc(2); rawj[0]='['; rawj[1]='\0';
    size_t rawlen = 1;
    const char *cwd = NULL;
    const char *session_id = NULL;

    size_t n = json_len(lines);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_array_get(lines, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        const char *otype = json_get_str(obj, "type", "");
        json_t *payload = json_object_get(obj, "payload");
        if (!payload || payload->type != JSON_OBJECT) continue;
        if (strcmp(otype, "session_meta") == 0) {
            const char *c = json_get_str(payload, "cwd", ""); if (*c) cwd = c;
            const char *sid = json_get_str(payload, "session_id", "");
            const char *pid = json_get_str(payload, "id", "");
            if (!*sid) sid = pid;
            if (*sid) session_id = sid;
            continue;
        }
        if (strcmp(otype, "response_item") != 0) continue;
        const char *ptype = json_get_str(payload, "type", "");
        if (strcmp(ptype, "message") == 0) {
            const char *role = json_get_str(payload, "role", "");
            if (strcmp(role, "user") != 0 && strcmp(role, "assistant") != 0) continue;
            json_t *content = json_object_get(payload, "content");
            char *text = NULL;
            if (content) { char *mcj = json_dumps(content, 0); text = foreign_sessions_flatten_blocks(mcj, "codex"); free(mcj); }
            else text = strdup("");
            if (!text || !*text) { free(text); continue; }
            if (strcmp(role, "user") == 0 && foreign_sessions_is_wrapper_text(text)) { free(text); continue; }
            json_t *escnode = json_string(text); char *esc = json_serialize(escnode); json_free(escnode);
            char *pair = (char *)malloc(strlen(role) + strlen(esc) + 8);
            snprintf(pair, strlen(role)+strlen(esc)+8, "[\"%s\",%s]", role, esc);
            free(esc);
            size_t need = rawlen + strlen(pair) + 1;
            rawj = (char *)realloc(rawj, need); strcat(rawj, pair); rawlen += strlen(pair);
            free(pair); free(text);
        } else if (strcmp(ptype, "custom_tool_call") == 0 ||
                   strcmp(ptype, "function_call") == 0 ||
                   strcmp(ptype, "local_shell_call") == 0) {
            const char *name = json_get_str(payload, "name", "");
            if (!*name) name = json_get_str(payload, "tool", "");
            if (!*name) name = "tool";
            char buf[128];
            snprintf(buf, sizeof(buf), "[ran tool: %s]", name);
            json_t *escnode = json_string(buf); char *esc = json_serialize(escnode); json_free(escnode);
            char *pair = (char *)malloc(strlen(esc) + 16);
            snprintf(pair, strlen(esc)+16, "[\"assistant\",%s]", esc);
            free(esc);
            size_t need = rawlen + strlen(pair) + 1;
            rawj = (char *)realloc(rawj, need); strcat(rawj, pair); rawlen += strlen(pair);
            free(pair);
        }
    }
    rawj = (char *)realloc(rawj, rawlen + 2); strcat(rawj, "]");

    char *merged = foreign_sessions_merge_turns(rawj);
    free(rawj);
    json_t *merged_obj = json_parse(merged, NULL);
    free(merged);
    char *ful = fs_first_user_line(merged_obj);

    json_t *out = json_new_object();
    json_object_set(out, "turns", merged_obj ? merged_obj : json_new_array());
    json_object_set(out, "cwd", json_string(cwd ? cwd : ""));
    json_object_set(out, "title_guess", json_string(ful ? ful : ""));
    json_object_set(out, "session_id", json_string(session_id ? session_id : ""));
    char *res = json_dumps(out, 0);
    json_free(out); if (merged_obj) json_free(merged_obj);
    free(ful);
    json_free(lines);
    return res;
}
