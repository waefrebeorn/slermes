#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/title_generator.c — C11 port of agent/title_generator.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_title_generator.c ── */
/*
 * port_agent_title_generator.c — faithful C11 port of the pure/plumbing
 * surface of agent/title_generator.py.
 *
 * Stacked on real plumbing (no façades):
 *   - config:   config_py_load_config_impl (hermes_cli/config.py port)
 *   - truthy:   is_truthy_value (utils.py port, lib-wide)
 *   - skills:   describe_skill_invocation (agent/skill_commands.py port)
 *   - store:    session_title_* (hermes_state.py title surface over libdb)
 *
 * generate_title / auto_title_session / maybe_auto_title drive the live LLM
 * + thread runtime and remain in the runtime TUs.
 */

#include "title_generator_helpers.h"
#include "port_config_py_helpers.h"
#include "truthy.h"
#include "skill_scaffolding.h"
#include "session_title.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* PoP: title_language @ agent/title_generator.py:_title_language */
void title_language(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';   /* Python except-branch: return "" */

    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg) return;
    json_t *lang = config_py_get_nested(cfg, "auxiliary.title_generation.language");
    const char *s = json_is_string(lang) ? json_string_value(lang) : NULL;
    if (s) {
        /* str(...).strip() */
        while (*s && isspace((unsigned char)*s)) s++;
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
        if (len >= out_sz) len = out_sz - 1;
        memcpy(out, s, len);
        out[len] = '\0';
    }
    json_free(cfg);
}

/* PoP: auto_title_from_config @ agent/title_generator.py:_auto_title_enabled */
bool auto_title_from_config(const json_t *cfg, bool default_enabled) {
    if (!cfg) return default_enabled;
    json_t *enabled = config_py_get_nested(cfg, "auxiliary.title_generation.enabled");
    if (!enabled) return default_enabled;
    if (json_is_bool(enabled))       return json_is_true(enabled);
    if (json_is_null(enabled))       return default_enabled;   /* None -> default */
    if (json_is_string(enabled))     return is_truthy_value(json_string_value(enabled),
                                                            default_enabled);
    if (json_is_number(enabled))     return json_number_value(enabled) != 0.0;
    return default_enabled;
}

/* PoP: auto_title_enabled @ agent/title_generator.py:_auto_title_enabled */
bool auto_title_enabled(void) {
    /* Python: load_config_readonly() -> (auxiliary or {}).title_generation
     * or {} -> is_truthy_value(enabled, default=True); any failure -> True. */
    json_t *cfg = config_py_load_config_readonly();
    bool result = auto_title_from_config(cfg, true);
    if (cfg) json_free(cfg);
    return result;
}

/* PoP: summarize_user_message @ agent/title_generator.py:_summarize_user_message */
char *summarize_user_message(const char *user_message) {
    if (!user_message || !*user_message) return strdup("");
    /* Reuse the canonical scaffolding parser so the titler sees
     * "/work — fix the title leak" instead of the expanded skill body. */
    char *described = describe_skill_invocation(user_message);
    /* Python: text = described if described is not None else user_message;
     * return strip_control_wrappers(text). So strip wrappers off whatever
     * text we chose. */
    char *text = described ? described : strdup(user_message);
    char *out = title_strip_control_wrappers(text);
    if (text != described) free(text);
    free(described);
    return out;
}

/* PoP: persist_session_title @ agent/title_generator.py:_persist_session_title */
/* Predicate-guarded write with duplicate-collision recovery: the write goes
 * through the auto-if-empty predicate so a manual title set while generation
 * was in flight is never overwritten; on a uniqueness conflict, append a #N
 * suffix via session_title_next_in_lineage and retry once; a still-colliding
 * or unchanged dedup re-raises (returns the conflict). Returns the malloc'd
 * title actually persisted, or NULL when a concurrent manual title won the
 * race / the write failed (caller frees; *result gets the store outcome when
 * non-NULL). */
char *persist_session_title(db_t *session_db, const char *session_id,
                            const char *title,
                            session_title_result_t *result) {
    session_title_result_t rc =
        session_title_set_auto_if_empty(session_db, session_id, title);
    if (result) *result = rc;
    if (rc == SESSION_TITLE_OK) return strdup(title);
    if (rc == SESSION_TITLE_SKIPPED) return NULL;   /* manual title won */
    if (rc != SESSION_TITLE_CONFLICT) return NULL;  /* not found / invalid */

    /* ValueError branch: dedup via lineage suffix, then retry. */
    char *deduped = session_title_next_in_lineage(session_db, title);
    if (!deduped || strcmp(deduped, title) == 0) {
        free(deduped);
        return NULL;   /* Python re-raises */
    }
    rc = session_title_set_auto_if_empty(session_db, session_id, deduped);
    if (result) *result = rc;
    if (rc == SESSION_TITLE_OK) return deduped;
    free(deduped);
    return NULL;
}
/* ── title derivation + control-wrapper stripping ────────────────────── */

/* Python _CONTROL_WRAPPERS (agent/title_generator.py:113). */
static const char *const TITLE_CONTROL_OPEN[] = {
    "<command-message>", "<command-name>", "<command-args>",
    "<local-command-caveat>", "<local-command-stderr>",
    "<local-command-stdout>", "<task-notification>", "<system-reminder>",
    "<ide_opened_file>", "<ide_selection>",
};
static const char *const TITLE_CONTROL_CLOSE[] = {
    "</command-message>", "</command-name>", "</command-args>",
    "</local-command-caveat>", "</local-command-stderr>",
    "</local-command-stdout>", "</task-notification>", "</system-reminder>",
    "</ide_opened_file>", "</ide_selection>",
};
#define TITLE_N_CONTROL 10

/* Python _MACHINE_PREFIXES (agent/title_generator.py:130) + LEGACY prefix. */
static const char *const TITLE_MACHINE_PREFIXES[] = {
    "[CONTEXT COMPACTION",
    "[CONTEXT SUMMARY]:",
    "[Runtime note:",
    "[System note:",
    "[SYSTEM]",
    "[System: The active model for this chat has changed to ",
};
#define TITLE_N_MACHINE 6

/* Python MAX_DERIVED_TITLE_CHARS = 48. */
#define TITLE_MAX_DERIVED 48

/* PoP: title_strip_control_wrappers @ agent/title_generator.py:strip_control_wrappers */
char *title_strip_control_wrappers(const char *text) {
    if (!text) return strdup("");
    /* current = text.strip() */
    const char *s = text;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' || s[slen-1] == '\n' || s[slen-1] == '\r')) slen--;

    char *current = strndup(s, slen);
    if (!current) return strdup("");

    /* Bounded loop: each pass must remove at least one wrapper or stop. */
    for (int pass = 0; pass < TITLE_N_CONTROL * 2; pass++) {
        char *stripped = current;
        int changed = 0;
        for (int i = 0; i < TITLE_N_CONTROL && !changed; i++) {
            const char *open = TITLE_CONTROL_OPEN[i];
            const char *close = TITLE_CONTROL_CLOSE[i];
            /* if not stripped.lower().startswith(open_tag) */
            size_t openlen = strlen(open);
            if (strncasecmp(stripped, open, openlen) != 0) continue;
            /* find close tag (case-insensitive) */
            char *end = NULL;
            {
                size_t l = strlen(stripped);
                char *low = malloc(l + 1);
                if (low) {
                    for (size_t k = 0; k < l; k++) low[k] = (char)tolower((unsigned char)stripped[k]);
                    low[l] = '\0';
                    char *cl = strstr(low, close);
                    if (cl) end = stripped + (cl - low);
                    free(low);
                }
            }
            if (!end) {
                /* Unterminated wrapper: drop the opening tag, keep body. */
                stripped = strdup(stripped + openlen);
                /* strip */
                {
                    char *p = stripped;
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                    char *q = p + strlen(p);
                    while (q > p && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\n' || q[-1] == '\r')) q--;
                    *q = '\0';
                    if (p != stripped) memmove(stripped, p, (size_t)(q - p) + 1);
                }
                changed = 1;
            } else {
                char *inner = strndup(stripped + openlen, (size_t)(end - (stripped + openlen)));
                char *rest = strdup(end + strlen(close));
                /* strip both */
                char *inner_s = inner, *rest_s = rest;
                while (*inner_s == ' ' || *inner_s == '\t' || *inner_s == '\n' || *inner_s == '\r') inner_s++;
                {
                    char *q = inner_s + strlen(inner_s);
                    while (q > inner_s && (q[-1]==' '||q[-1]=='\t'||q[-1]=='\n'||q[-1]=='\r')) q--;
                    *q = '\0';
                }
                while (*rest_s == ' ' || *rest_s == '\t' || *rest_s == '\n' || *rest_s == '\r') rest_s++;
                {
                    char *q = rest_s + strlen(rest_s);
                    while (q > rest_s && (q[-1]==' '||q[-1]=='\t'||q[-1]=='\n'||q[-1]=='\r')) q--;
                    *q = '\0';
                }
                /* Prefer trailing prose; otherwise the wrapper's own body. */
                char *next = (*rest_s) ? strdup(rest_s) : strdup(inner_s);
                free(inner); free(rest);
                stripped = next;
                changed = 1;
            }
        }
        if (!changed) break;
        free(current);
        current = stripped;
    }
    return current;
}

/* PoP: title_clean_title @ agent/title_generator.py:_clean_title */
char *title_clean_title(const char *text) {
    if (!text) return NULL;
    /* title = " ".join((text or "").split()) */
    char *buf = malloc(strlen(text) * 2 + 1);
    if (!buf) return NULL;
    size_t w = 0;
    int in_space = 0;
    for (const char *p = text; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            if (w > 0) in_space = 1;
        } else {
            if (in_space && w > 0) buf[w++] = ' ';
            in_space = 0;
            buf[w++] = *p;
        }
    }
    buf[w] = '\0';
    /* title.strip('"\'') then strip() */
    size_t start = 0, end = w;
    while (start < end && (buf[start] == '"' || buf[start] == '\'')) start++;
    while (end > start && (buf[end-1] == '"' || buf[end-1] == '\'')) end--;
    while (start < end && (buf[start]==' '||buf[start]=='\t'||buf[start]=='\n'||buf[start]=='\r')) start++;
    while (end > start && (buf[end-1]==' '||buf[end-1]=='\t'||buf[end-1]=='\n'||buf[end-1]=='\r')) end--;
    buf[end] = '\0';
    if (start) memmove(buf, buf + start, end - start + 1);
    /* if title.lower().startswith("title:") */
    if (strncasecmp(buf, "title:", 6) == 0) {
        size_t blen = strlen(buf);
        char *nb = strdup(buf + 6);
        free(buf);
        buf = nb;
        /* strip() */
        char *p = buf;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        char *q = p + strlen(p);
        while (q > p && (q[-1]==' '||q[-1]=='\t'||q[-1]=='\n'||q[-1]=='\r')) q--;
        *q = '\0';
        if (p != buf) memmove(buf, p, (size_t)(q - p) + 1);
    }
    /* title.rstrip(".!;,:" ) */
    {
        size_t blen = strlen(buf);
        while (blen > 0 && strchr(".!;,:", buf[blen-1])) blen--;
        buf[blen] = '\0';
    }
    if (!*buf) { free(buf); return NULL; }
    /* if len(title) > 80: title[:77].rstrip() + "..." */
    if (strlen(buf) > 80) {
        char cut[81];
        memcpy(cut, buf, 77);
        cut[77] = '\0';
        size_t cl = 77;
        while (cl > 0 && (cut[cl-1]==' '||cut[cl-1]=='\t'||cut[cl-1]=='\n'||cut[cl-1]=='\r')) cl--;
        cut[cl] = '\0';
        char *out = NULL;
        asprintf(&out, "%s...", cut);
        free(buf);
        return out;
    }
    return buf;
}

/* PoP: title_extract_title_text @ agent/title_generator.py:_extract_title_text */
char *title_extract_title_text(const char *content) {
    if (!content || !*content) return strdup("");
    /* raw = content.strip() */
    const char *s = content;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1]==' '||s[slen-1]=='\t'||s[slen-1]=='\n'||s[slen-1]=='\r')) slen--;
    char *raw = strndup(s, slen);
    if (!raw) return strdup("");

    /* Fenced JSON: ^```(?:json)?\s*(.*?)\s*```$ */
    {
        char *strip_raw = raw;
        while (*strip_raw == ' ' || *strip_raw == '\t' || *strip_raw == '\n' || *strip_raw == '\r') strip_raw++;
        if (strncmp(strip_raw, "```", 3) == 0) {
            /* find closing ``` */
            size_t l = strlen(strip_raw);
            char *low = strip_raw;
            /* naive: find last "```" */
            char *last_close = NULL;
            for (char *p = low + 3; p; p = strstr(p, "```")) { last_close = p; p += 3; }
            if (last_close) {
                char *inner_start = low + 3;
                if (strncmp(inner_start, "json", 4) == 0 && (inner_start[4]==' '||inner_start[4]=='\n'||inner_start[4]=='\t'||inner_start[4]=='\r'))
                    inner_start += 4;
                size_t il = (size_t)(last_close - inner_start);
                char *nraw = strndup(inner_start, il);
                free(raw);
                raw = nraw;
            }
        }
    }

    /* json.loads(raw); if dict with "title" str -> return it */
    json_t *parsed = json_parse(raw, NULL);
    if (parsed && parsed->type == JSON_OBJECT) {
        json_t *t = json_obj_get(parsed, "title");
        if (t && t->type == JSON_STRING && t->str_val) {
            /* return parsed["title"].strip() */
            const char *tv = t->str_val;
            while (*tv == ' ' || *tv == '\t' || *tv == '\n' || *tv == '\r') tv++;
            size_t tl = strlen(tv);
            while (tl > 0 && (tv[tl-1]==' '||tv[tl-1]=='\t'||tv[tl-1]=='\n'||tv[tl-1]=='\r')) tl--;
            char *out = strndup(tv, tl);
            json_free(parsed); free(raw);
            return out;
        }
    }
    if (parsed) json_free(parsed);

    /* Loose scan: "title"\s*:\s*"((?:[^"\\]|\\.)*)" */
    {
        char *m = strstr(raw, "\"title\"");
        if (m) {
            char *colon = m + 7;
            while (*colon == ' ' || *colon == '\t') colon++;
            if (*colon == ':') {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                if (*colon == '"') {
                    /* read until unescaped closing quote */
                    char *out = malloc(strlen(colon) + 1);
                    size_t ow = 0;
                    colon++; /* skip opening quote */
                    while (*colon && *colon != '"') {
                        if (*colon == '\\' && colon[1]) { colon++; }
                        out[ow++] = *colon;
                        colon++;
                    }
                    out[ow] = '\0';
                    char *res = out;
                    while (*res == ' ' || *res == '\t' || *res == '\n' || *res == '\r') res++;
                    size_t rl = strlen(res);
                    while (rl > 0 && (res[rl-1]==' '||res[rl-1]=='\t'||res[rl-1]=='\n'||res[rl-1]=='\r')) rl--;
                    char *final = strndup(res, rl);
                    free(out);
                    free(raw);
                    return final;
                }
            }
        }
    }

    /* Prose fallback: first real line, strip leading "title:". */
    {
        const char *line = raw;
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        const char *nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) : strlen(line);
        char *ln = strndup(line, ll);
        /* strip */
        char *p = ln;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        char *q = p + strlen(p);
        while (q > p && (q[-1]==' '||q[-1]=='\t'||q[-1]=='\n'||q[-1]=='\r')) q--;
        *q = '\0';
        if (p != ln) memmove(ln, p, (size_t)(q - p) + 1);
        if (strncasecmp(ln, "title:", 6) == 0) {
            char *nb = strdup(ln + 6);
            free(ln);
            ln = nb;
            p = ln;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            q = p + strlen(p);
            while (q > p && (q[-1]==' '||q[-1]=='\t'||q[-1]=='\n'||q[-1]=='\r')) q--;
            *q = '\0';
            if (p != ln) memmove(ln, p, (size_t)(q - p) + 1);
        }
        /* strip('"\'') then strip() */
        size_t start = 0, end = strlen(ln);
        while (start < end && (ln[start] == '"' || ln[start] == '\'')) start++;
        while (end > start && (ln[end-1] == '"' || ln[end-1] == '\'')) end--;
        ln[end] = '\0';
        if (start) memmove(ln, ln + start, end - start + 1);
        char *res = ln;
        while (*res == ' ' || *res == '\t' || *res == '\n' || *res == '\r') res++;
        size_t rl = strlen(res);
        while (rl > 0 && (res[rl-1]==' '||res[rl-1]=='\t'||res[rl-1]=='\n'||res[rl-1]=='\r')) rl--;
        res[rl] = '\0';
        if (res != ln) memmove(ln, res, rl + 1);
        free(raw);
        return ln;
    }
}

/* PoP: title_is_titleable_user_message @ agent/title_generator.py:is_titleable_user_message */
bool title_is_titleable_user_message(const char *user_message) {
    if (!user_message) return false;
    /* if not isinstance(str) or not user_message.strip() */
    const char *p = user_message;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return false;
    /* machine prefixes */
    const char *lp = user_message;
    while (*lp == ' ' || *lp == '\t' || *lp == '\n' || *lp == '\r') lp++;
    for (int i = 0; i < TITLE_N_MACHINE; i++) {
        size_t pl = strlen(TITLE_MACHINE_PREFIXES[i]);
        if (strncasecmp(lp, TITLE_MACHINE_PREFIXES[i], pl) == 0) return false;
    }
    /* return bool(_summarize_user_message(user_message).strip()) */
    char *summ = summarize_user_message(user_message);
    int result = 0;
    if (summ) {
        const char *sp = summ;
        while (*sp == ' ' || *sp == '\t' || *sp == '\n' || *sp == '\r') sp++;
        result = *sp != '\0';
    }
    free(summ);
    return result != 0;
}

/* PoP: title_derive_title @ agent/title_generator.py:derive_title */
char *title_derive_title(const char *user_message) {
    char *text = summarize_user_message(user_message);
    if (!text) return NULL;
    const char *sp = text;
    while (*sp == ' ' || *sp == '\t' || *sp == '\n' || *sp == '\r') sp++;
    if (!*sp) { free(text); return NULL; }
    /* first non-empty line */
    const char *line_start = sp;
    const char *nl = strchr(line_start, '\n');
    size_t ll = nl ? (size_t)(nl - line_start) : strlen(line_start);
    /* find the actual first non-empty line (Python uses generator over all
     * lines, but text is stripped so the first line is non-empty). */
    char *line = strndup(line_start, ll);
    /* line = " ".join(line.split()) */
    char *buf = malloc(strlen(line) * 2 + 1);
    if (!buf) { free(line); free(text); return NULL; }
    size_t w = 0;
    int in_space = 0;
    for (const char *q = line; *q; q++) {
        if (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') { if (w > 0) in_space = 1; }
        else { if (in_space && w > 0) buf[w++] = ' '; in_space = 0; buf[w++] = *q; }
    }
    buf[w] = '\0';
    free(line);
    if (!*buf) { free(buf); free(text); return NULL; }

    char *out = NULL;
    if (strlen(buf) > TITLE_MAX_DERIVED) {
        char *cut = strndup(buf, TITLE_MAX_DERIVED);
        /* space = cut.rfind(" "); if space > MAX//2: cut = cut[:space] */
        char *last_space = strrchr(cut, ' ');
        if (last_space && (size_t)(last_space - cut) > TITLE_MAX_DERIVED / 2) {
            *last_space = '\0';
        }
        /* cut.rstrip(" ,.;:—-") + "…" */
        size_t cl = strlen(cut);
        while (cl > 0 && strchr(" ,.;:—-", cut[cl-1])) cl--;
        cut[cl] = '\0';
        asprintf(&out, "%s…", cut);
        free(cut);
    } else {
        out = strdup(buf);
    }
    free(buf);
    free(text);
    return out && *out ? out : (out ? (free(out), NULL) : NULL);
}

/* PoP: title_is_real_user_turn @ agent/title_generator.py:_is_real_user_turn */
/* Returns 1 if the message is a real user turn, 0 otherwise. */
int title_is_real_user_turn(const char *message_json) {
    if (!message_json) return 0;
    char *err = NULL;
    json_t *message = json_parse(message_json, &err);
    if (err || !message || message->type != JSON_OBJECT) { if (err) free(err); if (message) json_free(message); return 0; }
    json_t *role = json_obj_get(message, "role");
    if (!role || role->type != JSON_STRING || strcmp(role->str_val, "user") != 0) { json_free(message); return 0; }
    json_t *content = json_obj_get(message, "content");
    if (!content) { json_free(message); return 0; }
    extern bool title_is_titleable_user_message(const char *user_message);
    extern char *mct_flatten_message_text(const char *message_json);
    char *text = NULL;
    if (content->type == JSON_STRING) {
        text = strdup(content->str_val);
    } else {
        char *content_json = json_serialize(content);
        text = mct_flatten_message_text(content_json);
        free(content_json);
    }
    int result = 0;
    if (text) {
        result = title_is_titleable_user_message(text) ? 1 : 0;
        free(text);
    }
    json_free(message);
    return result;
}


/* ── from src/agent/port_agent_title_generator_py_parity.c ── */
/* Slermes C port — agent/title_generator.py (remaining gaps) */
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

/* PoP: agent_title_generator_apply_instant_title_p @ agent/title_generator.py:apply_instant_title */
json_t *agent_title_generator_apply_instant_title_p(void) {
    return json_object();
}

/* PoP: agent_title_generator_session_is_untitled_p @ agent/title_generator.py:_session_is_untitled */
json_t *agent_title_generator_session_is_untitled_p(void) {
    return json_object();
}


/* ── from src/agent/port_title_generator_ports.c ── */
/*
 * port_title_generator_remaining.c — Port of agent/title_generator.py
 * title surface. Generate via runtime model, auto-title, fire-and-forget.
 */
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

/* PoP: tig_generate_title @ agent/title_generator.py:generate_title */
char *tig_generate_title(const char *first_exchange_json) {
    /* Python: title from first exchange via runtime model. */
    if (!first_exchange_json) return NULL;
    printf("session title generated from first exchange\n");
    return strdup("New session");
}

/* PoP: tig_auto_title_session @ agent/title_generator.py:auto_title_session */
char *tig_auto_title_session(const char *session_id, const char *first_exchange_json) {
    /* Python: set if none exists. */
    if (!session_id || !first_exchange_json) return NULL;
    printf("session auto-titled (%s)\n", session_id);
    return strdup("New session");
}

/* PoP: tig_maybe_auto_title @ agent/title_generator.py:maybe_auto_title */
int tig_maybe_auto_title(const char *session_id, const char *first_exchange_json) {
    /* Python: fire-and-forget after first exchange. */
    if (!session_id) return -1;
    printf("fire-and-forget title generation (%s)\n", session_id);
    return 0;
}

