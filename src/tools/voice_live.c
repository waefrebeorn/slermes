/*
 * voice_live.c — C home for upstream Hermes tools/voice_live.py.
 * PoP: w6_voicelive_605a89_voice_live_turn_note @ tools/voice_live.py:voice_live_turn_note
 * PoP: w6_voicelive_605a89_voice_chat_mode @ tools/voice_live.py:voice_chat_mode
 * PoP: w6_voicelive_605a89_live_instructions @ tools/voice_live.py:live_instructions
 */
#include "noc_w6_voicelive_605a89.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char GPT_LIVE_MODE[] = "gpt-live";
static const char CHAINED_MODE[] = "chained";

static const char VOICE_LIVE_TURN_NOTE[] =
    "[Note: this message is a delegation from a live spoken conversation. The text is a voice "
    "transcript (it may contain mis-hearings, hesitations and later corrections; use the latest "
    "intent). Your reply will be spoken aloud by a voice model that paraphrases it: answer in plain "
    "conversational sentences, keep it short (a few sentences unless the user asked for detail), no "
    "markdown, no lists, no code blocks, no URLs read out character by character. Do the work with "
    "your tools as usual; only the final facts need to be spoken. Do not claim an action succeeded "
    "before it actually did.]";

static const char LIVE_PERSONA[] =
    "You are Hermes, a calm and friendly voice assistant. Speak naturally at an unhurried pace. "
    "Be clear and direct, not overly cheerful. If the user is frustrated, acknowledge it briefly "
    "and focus on the next helpful step.\n\n"
    "Backchannel policy: Use moderate backchannels. Acknowledge naturally without competing with "
    "the main response.\n\n"
    "Interruption policy: Stop speaking when the user interrupts. Listen to what they say.\n\n"
    "Delegation policy:\n"
    "Backend tools:\n"
    "- Hermes agent: a full AI agent with tools — it can run commands, read and edit files, "
    "browse the web, search, remember things across sessions, schedule tasks, and reason "
    "carefully about anything. It is the one who actually does work and knows facts.\n\n"
    "Delegate to the backend when:\n"
    "- The user asks a question that needs facts, current information, or careful reasoning.\n"
    "- The user asks you to do, check, find, make, fix, run or remember anything.\n"
    "- A correction changes work already requested.\n\n"
    "Do not delegate to the backend when:\n"
    "- The user greets you, makes small talk, or asks you to repeat a result already provided.\n"
    "- You need a brief clarification to understand the request.\n\n"
    "Delegate before giving an answer that depends on backend work. Do not guess the result "
    "while waiting; say briefly that you are checking, then wait for the result.";

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace (fixtures are ASCII here). */
static char *py_strip(const char *s) {
    size_t n;
    char *out;
    if (!s) return strdup("");
    while (*s && is_space(*s)) s++;
    n = strlen(s);
    while (n && is_space(s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

char *w6_voicelive_605a89_voice_live_turn_note(const char *context) {
    char *c = py_strip(context);
    char *out;
    size_t n;
    if (!*c) { free(c); return strdup(VOICE_LIVE_TURN_NOTE); }
    n = strlen(VOICE_LIVE_TURN_NOTE) + strlen("\n[Recent spoken conversation, newest last:\n")
        + strlen(c) + strlen("]") + 1;
    out = (char *)malloc(n);
    snprintf(out, n, "%s\n[Recent spoken conversation, newest last:\n%s]",
             VOICE_LIVE_TURN_NOTE, c);
    free(c);
    return out;
}

/* Truthiness of a decoded JSON scalar, mirroring Python `raw or CHAINED_MODE`. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* Python str(value) for the scalar values used by voice_chat_mode. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    return strdup("");
}

const char *w6_voicelive_605a89_voice_chat_mode(const struct json_t *voice) {
    const json_t *raw = voice ? json_obj_get(voice, "voice_chat_mode") : NULL;
    char *mode, *p;
    if (!py_truthy(raw)) return CHAINED_MODE;      /* raw or "chained" */
    mode = py_str(raw);
    p = py_strip(mode);
    free(mode);
    for (size_t i = 0; p[i]; i++) {
        if (p[i] >= 'A' && p[i] <= 'Z') p[i] = (char)(p[i] - 'A' + 'a');
        if (p[i] == '_') p[i] = '-';
    }
    if (!strcmp(p, "gpt-live") || !strcmp(p, "gptlive") || !strcmp(p, "live")) {
        free(p);
        return GPT_LIVE_MODE;
    }
    free(p);
    return CHAINED_MODE;
}

char *w6_voicelive_605a89_live_instructions(const struct json_t *live) {
    const json_t *raw = live ? json_obj_get(live, "instructions") : NULL;
    char *extra;
    char *out;
    size_t n;
    if (!py_truthy(raw)) return strdup(LIVE_PERSONA);
    extra = py_strip(py_str(raw));
    n = strlen(LIVE_PERSONA) + strlen("\n\n") + strlen(extra) + 1;
    out = (char *)malloc(n);
    snprintf(out, n, "%s\n\n%s", LIVE_PERSONA, extra);
    free(extra);
    return out;
}
