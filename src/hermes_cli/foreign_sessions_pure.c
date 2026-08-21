#define _POSIX_C_SOURCE 200809L
/* foreign_sessions_pure.c — Faithful C11 port of the pure, deterministic
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
 * {"role","content"} objects. All strings are malloc'd — free() them.
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
 *   system-reminder)\b  (case-insensitive) */
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

/* _flatten_blocks(content, source) */
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

/* _merge_turns(raw_turns_json) -> JSON array of {role, content} */
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

/* ForeignSession.label: [<name>] <title[:60]> */
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
