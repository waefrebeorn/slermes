/*
 * review_engine.c — Port of agent/review_engine.py (C11).
 *
 * Shared engine for the /review command: snapshot the recent conversation,
 * collect the parent's loaded skills, compose the reviewer subagent's
 * (goal, context) pair, and render the human-facing dispatch note.
 *
 * String returns are malloc'd (free()). JSON in/out uses libjson; callers
 * own returned json_t* nodes unless noted.
 */
#define _POSIX_C_SOURCE 200809L
#include "review_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* DEFAULT_CONTEXT_MESSAGES — how many recent chat messages the reviewer gets. */
#define REVIEW_ENGINE_DEFAULT_CONTEXT_MESSAGES 10
/* _MESSAGE_CHAR_CAP — per-message excerpt cap. */
#define REVIEW_ENGINE_MESSAGE_CHAR_CAP 12000

static char *rev_strdup(const char *s) {
    return s ? strdup(s) : NULL;
}

/* _message_text: extract display text from a conversation message dict.
 * Handles plain-string content and OpenAI-style multimodal content lists
 * (text parts joined with \n; non-text parts noted as "[type]"). */
static char *review_engine_message_text(const json_t *message) {
    if (!message || !json_is_object(message)) return strdup("");
    const json_t *content = json_object_get(message, "content");
    if (!content) return strdup("");
    if (json_is_string(content)) {
        const char *s = json_string_value(content);
        return rev_strdup(s ? s : "");
    }
    if (json_is_array(content)) {
        size_t n = json_array_size(content);
        /* Join text parts with "\n". */
        size_t cap = 256;
        char *out = malloc(cap);
        if (!out) return NULL;
        size_t len = 0;
        out[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            const json_t *part = json_array_get(content, i);
            if (!part || !json_is_object(part)) continue;
            const char *type = json_string_value(json_object_get(part, "type"));
            char piece[1024];
            if (type && strcmp(type, "text") == 0) {
                const char *txt = json_string_value(json_object_get(part, "text"));
                snprintf(piece, sizeof(piece), "%s", txt ? txt : "");
            } else {
                snprintf(piece, sizeof(piece), "[%s]",
                         type ? type : "attachment");
            }
            if (!piece[0]) continue;
            size_t plen = strlen(piece);
            int need_nl = (len > 0);
            while (len + plen + need_nl + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            if (need_nl) out[len++] = '\n';
            memcpy(out + len, piece, plen + 1);
            len += plen;
        }
        return out;
    }
    return strdup("");
}

/* PoP: review_engine_snapshot_recent_messages @ agent/review_engine.py:snapshot_recent_messages */
json_t *review_engine_snapshot_recent_messages(const json_t *messages, int limit) {
    /* Last `limit` user/assistant messages as [{"role","text"}...], oldest
     * first. System messages and tool results excluded; empty-text messages
     * skipped; over-cap text truncated with the Python marker. */
    if (limit <= 0) limit = REVIEW_ENGINE_DEFAULT_CONTEXT_MESSAGES;
    json_t *picked = json_array();   /* collected newest-first, reversed at end */
    if (!picked) return NULL;
    if (!messages || !json_is_array(messages)) return picked;

    size_t n = json_array_size(messages);
    for (size_t idx = n; idx-- > 0;) {   /* reversed(list(messages)) */
        const json_t *message = json_array_get(messages, idx);
        if (!message || !json_is_object(message)) continue;
        const char *role = json_string_value(json_object_get(message, "role"));
        if (!role || (strcmp(role, "user") != 0 && strcmp(role, "assistant") != 0))
            continue;
        char *text = review_engine_message_text(message);
        if (!text) break;
        /* strip() parity: trim surrounding whitespace, skip empty result. */
        char *b = text;
        while (*b && isspace((unsigned char)*b)) b++;
        char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        size_t tlen = (size_t)(e - b);
        if (tlen == 0) { free(text); continue; }

        json_t *item = json_object();
        if (!item) { free(text); break; }
        bool truncated = false;
        if ((int)tlen > REVIEW_ENGINE_MESSAGE_CHAR_CAP) {
            tlen = REVIEW_ENGINE_MESSAGE_CHAR_CAP;
            truncated = true;
        }
        char *final_text = malloc(tlen + sizeof("\n[... truncated ...]"));
        if (!final_text) { json_free(item); free(text); break; }
        memcpy(final_text, b, tlen);
        final_text[tlen] = '\0';
        if (truncated) strcat(final_text, "\n[... truncated ...]");
        json_set(item, "role", json_string(role));
        json_set(item, "text", json_string(final_text));
        free(final_text);
        free(text);
        json_array_append(picked, item);   /* steals item */
        if ((int)json_array_size(picked) >= limit) break;
    }
    /* out.reverse() — oldest first. */
    json_t *out = json_array();
    if (!out) { json_free(picked); return NULL; }
    for (size_t i = json_array_size(picked); i-- > 0;)
        json_array_append(out, json_copy(json_array_get(picked, i)));
    json_free(picked);
    return out;
}

static void rev_add_name(json_t *names, const char *name) {
    /* _add: cleaned, deduped append. */
    if (!names || !json_is_array(names)) return;
    if (!name) return;
    const char *b = name;
    while (*b && (*b == ' ' || *b == '\t')) b++;
    const char *e = b + strlen(b);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t')) e--;
    if (e == b) return;
    size_t len = (size_t)(e - b);
    char cleaned[512];
    if (len >= sizeof(cleaned)) return;
    memcpy(cleaned, b, len); cleaned[len] = '\0';
    size_t n = json_array_size(names);
    for (size_t i = 0; i < n; i++) {
        const char *have = json_string_value(json_array_get(names, i));
        if (have && strcmp(have, cleaned) == 0) return;   /* seen */
    }
    json_array_append(names, json_string(cleaned));
}

/* PoP: review_engine_collect_parent_loaded_skills @ agent/review_engine.py:collect_parent_loaded_skills */
json_t *review_engine_collect_parent_loaded_skills(const char *ephemeral_system_prompt,
                                                   const json_t *messages,
                                                   int limit) {
    /* Names of skills the parent operated under: launch-preloaded skills
     * (marker in ephemeral_system_prompt) then skill_view tool-call loads
     * from history; deduped; capped at limit (default 8). */
    if (limit <= 0) limit = 8;
    json_t *names = json_array();
    if (!names) return NULL;

    /* Preloaded marker: r'with the "([^"]+)" skill\s+preloaded' */
    if (ephemeral_system_prompt) {
        const char *p = ephemeral_system_prompt;
        const char *needle = "with the \"";
        size_t needle_len = strlen(needle);
        while ((p = strstr(p, needle)) != NULL) {
            const char *start = p + needle_len;
            const char *close = strchr(start, '"');
            if (!close) break;
            /* Python requires the literal " skill" + whitespace + "preloaded"
             * tail after the closing quote. */
            const char *tail = close + 1;
            if (strncmp(tail, " skill", 6) != 0) { p = close; continue; }
            tail += 6;
            while (*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == '\r') tail++;
            if (strncmp(tail, "preloaded", 9) == 0) {
                char name[512];
                size_t len = (size_t)(close - start);
                if (len < sizeof(name)) {
                    memcpy(name, start, len); name[len] = '\0';
                    rev_add_name(names, name);
                }
            }
            p = close;
        }
    }

    /* History loads: assistant tool_calls entries calling skill_view without
     * a file_path argument (whole-skill loads seed the reviewer). */
    if (messages && json_is_array(messages)) {
        size_t n = json_array_size(messages);
        for (size_t i = 0; i < n; i++) {
            const json_t *message = json_array_get(messages, i);
            if (!message || !json_is_object(message)) continue;
            const char *role = json_string_value(json_object_get(message, "role"));
            if (!role || strcmp(role, "assistant") != 0) continue;
            const json_t *calls = json_object_get(message, "tool_calls");
            if (!calls || !json_is_array(calls)) continue;
            size_t cn = json_array_size(calls);
            for (size_t k = 0; k < cn; k++) {
                const json_t *call = json_array_get(calls, k);
                if (!call || !json_is_object(call)) continue;
                const json_t *fn = json_object_get(call, "function");
                if (!fn || !json_is_object(fn)) continue;
                const char *fname = json_string_value(json_object_get(fn, "name"));
                if (!fname || strcmp(fname, "skill_view") != 0) continue;
                const char *args_txt = json_string_value(json_object_get(fn, "arguments"));
                json_t *args = json_parse(args_txt ? args_txt : "{}", NULL);
                if (!args || !json_is_object(args)) { if (args) json_free(args); continue; }
                /* No file_path => whole-skill load. */
                if (json_object_get(args, "file_path") == NULL) {
                    const char *sname = json_string_value(json_object_get(args, "name"));
                    rev_add_name(names, sname ? sname : "");
                }
                json_free(args);
                if ((int)json_array_size(names) >= limit) goto capped;
            }
        }
    }
capped:
    /* names[:limit] */
    while ((int)json_array_size(names) > limit)
        json_array_remove(names, json_array_size(names) - 1);
    return names;
}

/* build_review_task goal string — byte-identical to the Python constant. */
static const char *REVIEW_GOAL =
    "Act as an independent senior reviewer. Thoroughly review the work "
    "presented in the conversation excerpt provided in your context: "
    "investigate any code, pull request, branch, commit, documentation, "
    "design, or other artifact it references (open the PR, read the "
    "diff, run the code or tests where feasible) rather than judging "
    "from the excerpt alone. Produce a full, structured review: what "
    "the work does, whether it is correct and complete, concrete "
    "defects or risks found (with file/line references where possible), "
    "what was verified vs. only read, and a clear final verdict with "
    "recommended next steps.";

static int rev_append_line(char **buf, size_t *cap, size_t *len, const char *line) {
    size_t ll = strlen(line);
    while (*len + ll + 2 > *cap) {
        *cap *= 2;
        char *tmp = realloc(*buf, *cap);
        if (!tmp) return -1;
        *buf = tmp;
    }
    memcpy(*buf + *len, line, ll);
    *len += ll;
    (*buf)[*len] = '\n';   /* Python "\n".join adds separators; we append
                              lines with trailing newline and trim at end */
    (*len) += 1;
    return 0;
}

/* PoP: review_engine_build_review_task @ agent/review_engine.py:build_review_task */
bool review_engine_build_review_task(const json_t *snapshot,
                                     const char *user_prompt,
                                     const json_t *loaded_skills,
                                     char **out_goal,
                                     char **out_context) {
    /* Compose the reviewer subagent's (goal, context) pair. */
    if (!out_goal || !out_context) return false;
    *out_goal = rev_strdup(REVIEW_GOAL);
    if (!*out_goal) return false;

    size_t cap = 4096, len = 0;
    char *ctx = malloc(cap);
    if (!ctx) { free(*out_goal); *out_goal = NULL; return false; }
    ctx[0] = '\0';

    int fail = 0;
    fail |= rev_append_line(&ctx, &cap, &len,
        "You were spawned by the /review command. The following is an "
        "excerpt of the most recent conversation between the user and "
        "their primary agent. It is your starting evidence — the work to "
        "review is referenced in it.");
    fail |= rev_append_line(&ctx, &cap, &len, "");
    fail |= rev_append_line(&ctx, &cap, &len, "--- Recent conversation (oldest first) ---");
    if (snapshot && json_is_array(snapshot)) {
        size_t n = json_array_size(snapshot);
        for (size_t i = 0; i < n && !fail; i++) {
            const json_t *m = json_array_get(snapshot, i);
            const char *role = m ? json_string_value(json_object_get(m, "role")) : NULL;
            const char *text = m ? json_string_value(json_object_get(m, "text")) : NULL;
            const char *label =
                (role && strcmp(role, "user") == 0) ? "USER" : "PRIMARY AGENT";
            char header[32];
            snprintf(header, sizeof(header), "[%s]", label);
            fail |= rev_append_line(&ctx, &cap, &len, header);
            fail |= rev_append_line(&ctx, &cap, &len, text ? text : "");
            fail |= rev_append_line(&ctx, &cap, &len, "");
        }
    }
    fail |= rev_append_line(&ctx, &cap, &len, "--- End of conversation excerpt ---");
    if (loaded_skills && json_is_array(loaded_skills) &&
        json_array_size(loaded_skills) > 0) {
        /* skill_list = ", ".join(loaded_skills) */
        size_t total = 0;
        size_t sn = json_array_size(loaded_skills);
        for (size_t i = 0; i < sn; i++) {
            const char *s = json_string_value(json_array_get(loaded_skills, i));
            total += (s ? strlen(s) : 0) + 2;
        }
        char *skill_list = malloc(total + 1);
        if (skill_list) {
            skill_list[0] = '\0';
            for (size_t i = 0; i < sn; i++) {
                const char *s = json_string_value(json_array_get(loaded_skills, i));
                if (!s) continue;
                if (i) strcat(skill_list, ", ");
                strcat(skill_list, s);
            }
            char line[1024];
            snprintf(line, sizeof(line),
                     "The primary agent was operating under these loaded skills: "
                     "%.400s. Before reviewing, load each with "
                     "skill_view(name=...) and treat their conventions, invariants, "
                     "and review standards as binding for your assessment — the work "
                     "was produced under them and must be judged against them.",
                     skill_list);
            fail |= rev_append_line(&ctx, &cap, &len, "");
            fail |= rev_append_line(&ctx, &cap, &len, line);
            free(skill_list);
        }
    }
    if (user_prompt) {
        const char *b = user_prompt;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        if (e > b) {
            fail |= rev_append_line(&ctx, &cap, &len, "");
            fail |= rev_append_line(&ctx, &cap, &len, "Additional review instructions from the user:");
            char *stripped = strndup(b, (size_t)(e - b));
            if (stripped) {
                fail |= rev_append_line(&ctx, &cap, &len, stripped);
                free(stripped);
            }
        }
    }
    fail |= rev_append_line(&ctx, &cap, &len, "");
    fail |= rev_append_line(&ctx, &cap, &len,
        "Your review is delivered back into that conversation, addressed to "
        "the primary agent and its user. Be direct and specific; do not "
        "soften findings.");

    if (fail) { free(ctx); free(*out_goal); *out_goal = NULL; return false; }
    if (len > 0 && ctx[len - 1] == '\n') ctx[len - 1] = '\0';   /* join() has no trailing sep */
    *out_context = ctx;
    return true;
}

/* PoP: review_engine_format_dispatch_note @ agent/review_engine.py:format_dispatch_note */
char *review_engine_format_dispatch_note(const json_t *result, const char *user_prompt) {
    /* Human-facing one-liner for a successful dispatch. */
    if (!result || !json_is_object(result)) return rev_strdup("");
    const char *model = json_string_value(json_object_get(result, "review_model"));
    const char *status = json_string_value(json_object_get(result, "status"));

    char model_note[256] = "";
    if (model && model[0]) snprintf(model_note, sizeof(model_note), " on %s", model);

    char focus_note[512] = "";
    if (user_prompt) {
        const char *b = user_prompt;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        if (e > b)
            snprintf(focus_note, sizeof(focus_note), " (focus: %.*s)",
                     (int)(e - b), b);
    }

    char *out = malloc(2048);
    if (!out) return NULL;
    if (status && strcmp(status, "dispatched") == 0) {
        snprintf(out, 2048,
                 "⚖ Review subagent dispatched%s%s — it is investigating the "
                 "last %d messages in the background and its full review will "
                 "re-enter this conversation when it finishes.",
                 model_note, focus_note, REVIEW_ENGINE_DEFAULT_CONTEXT_MESSAGES);
    } else {
        /* Synchronous fallback: results JSON, [:4000]. */
        const json_t *res = json_object_get(result, "results");
        if (!res) res = result;
        char *dump = json_serialize(res);
        snprintf(out, 2048,
                 "⚖ Review completed synchronously%s%s — results:\n%.4000s",
                 model_note, focus_note, dump ? dump : "{}");
        free(dump);
    }
    return out;
}
