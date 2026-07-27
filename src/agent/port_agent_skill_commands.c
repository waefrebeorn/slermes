/*
 * port_agent_skill_commands.c — faithful C11 port of the skill-scaffolding
 * extractor surface of agent/skill_commands.py.
 *
 * When a user invokes a /skill (or /bundle), Hermes expands the turn into a
 * model-facing message embedding the full skill body plus scaffolding. Any
 * surface that summarizes a user turn from raw content (session titles,
 * sidebar previews, /rewind picker) must recover what the user actually
 * typed. These markers MUST stay byte-identical to the builders
 * (skill_cmd_build_message in skill_commands.c, bundle builder in
 * skill_bundles.c) — mirror of the Python co-location contract.
 */
#include "skill_scaffolding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Scaffolding markers — byte-identical to agent/skill_commands.py. */
static const char SKILL_INVOCATION_PREFIX[] = "[IMPORTANT: The user has invoked the ";
static const char SINGLE_SKILL_MARKER[] = "The full skill content is loaded below.]";
static const char SINGLE_SKILL_INSTRUCTION[] =
    "The user has provided the following instruction alongside the skill invocation: ";
static const char RUNTIME_NOTE[] = "\n\n[Runtime note:";
static const char BUNDLE_MARKER[] = " skill bundle,";
static const char BUNDLE_USER_INSTRUCTION[] = "\nUser instruction: ";
static const char BUNDLE_FIRST_SKILL_BLOCK[] = "\n\n[Loaded as part of the ";
/* PoP: SKILL_EXCERPT_JOINT @ agent/skill_commands.py:SKILL_EXCERPT_JOINT */
static const char SKILL_EXCERPT_JOINT_CH = '\x1e';

/* Duplicate [s, s+len) with Python str.strip() semantics applied. */
static char *strip_dup(const char *s, size_t len) {
    size_t b = 0, e = len;
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    char *out = malloc(e - b + 1);
    if (!out) return NULL;
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}

/* Python `rfind(needle)` on a NUL-terminated haystack. Returns index or -1. */
static long str_rfind(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    size_t hlen = strlen(hay);
    if (nlen == 0 || nlen > hlen) return -1;
    for (long i = (long)(hlen - nlen); i >= 0; i--)
        if (strncmp(hay + i, needle, nlen) == 0) return i;
    return -1;
}

/* PoP: extract_single_skill_user_instruction @ agent/skill_commands.py:_extract_single_skill_user_instruction */
char *extract_single_skill_user_instruction(const char *message) {
    if (!message) return NULL;
    /* Single-skill format appends the user instruction after the skill body,
     * so the LAST occurrence is the user-provided one. */
    long idx = str_rfind(message, SINGLE_SKILL_INSTRUCTION);
    if (idx < 0) return NULL;
    const char *instr = message + idx + (long)strlen(SINGLE_SKILL_INSTRUCTION);
    const char *rt = strstr(instr, RUNTIME_NOTE);
    size_t len = rt ? (size_t)(rt - instr) : strlen(instr);
    char *out = strip_dup(instr, len);
    if (out && !*out) { free(out); return NULL; }  /* instruction or None */
    return out;
}

/* PoP: extract_bundle_user_instruction @ agent/skill_commands.py:_extract_bundle_user_instruction */
char *extract_bundle_user_instruction(const char *message) {
    if (!message) return NULL;
    /* Bundle format puts the user instruction before the loaded skills, so
     * the FIRST occurrence is the user-provided one. */
    const char *m = strstr(message, BUNDLE_USER_INSTRUCTION);
    if (!m) return NULL;
    const char *instr = m + strlen(BUNDLE_USER_INSTRUCTION);
    const char *fs = strstr(instr, BUNDLE_FIRST_SKILL_BLOCK);
    size_t len = fs ? (size_t)(fs - instr) : strlen(instr);
    char *out = strip_dup(instr, len);
    if (out && !*out) { free(out); return NULL; }
    return out;
}

/* Internal: faithful extract with a pass-through flag mirroring Python's
 * `instruction is not content` identity check in describe_skill_invocation.
 * *passthrough=true means the message was NOT skill scaffolding and the
 * returned string is the content unchanged. */
static char *extract_impl(const char *content, bool *passthrough) {
    if (passthrough) *passthrough = false;
    if (!content) return NULL;
    if (strncmp(content, SKILL_INVOCATION_PREFIX,
                strlen(SKILL_INVOCATION_PREFIX)) != 0) {
        if (passthrough) *passthrough = true;
        return strdup(content);   /* normal user message passes through */
    }
    if (strstr(content, BUNDLE_MARKER))
        return extract_bundle_user_instruction(content);
    if (strstr(content, SINGLE_SKILL_MARKER))
        return extract_single_skill_user_instruction(content);
    return NULL;   /* bare /skill invocation: no user content worth storing */
}

/* PoP: extract_user_instruction_from_skill_message @ agent/skill_commands.py:extract_user_instruction_from_skill_message */
char *extract_user_instruction_from_skill_message(const char *content) {
    return extract_impl(content, NULL);
}

/* PoP: describe_skill_invocation @ agent/skill_commands.py:describe_skill_invocation */
char *describe_skill_invocation(const char *content) {
    if (!content || strncmp(content, SKILL_INVOCATION_PREFIX,
                            strlen(SKILL_INVOCATION_PREFIX)) != 0)
        return NULL;

    /* _SKILL_NAME_RE: prefix immediately followed by "([^"]*)" — the name
     * sits in the first quoted span of the activation note. re.match anchors
     * at position 0, so the quote must directly follow the prefix. */
    char *name = NULL;
    const char *after = content + strlen(SKILL_INVOCATION_PREFIX);
    if (*after == '"') {
        const char *q = after + 1;
        const char *close = strchr(q, '"');
        if (close) {
            char *raw = strip_dup(q, (size_t)(close - q));
            name = raw;
        }
    }
    if (!name) name = strdup("");

    /* label = name if name.startswith("/") else f"/{name}" */
    size_t nlen = strlen(name);
    char *label = malloc(nlen + 2);
    if (!label) { free(name); return NULL; }
    if (name[0] == '/') strcpy(label, name);
    else { label[0] = '/'; strcpy(label + 1, name); }

    bool passthrough = false;
    char *instruction = extract_impl(content, &passthrough);
    if (instruction && !passthrough) {
        /* Keep only the side before an excerpt joint. */
        char *joint = strchr(instruction, SKILL_EXCERPT_JOINT_CH);
        if (joint) *joint = '\0';
        /* " ".join(instruction.split()) — collapse all whitespace runs. */
        char *collapsed = malloc(strlen(instruction) + 1);
        if (collapsed) {
            size_t ci = 0; bool in_ws = true;
            for (const char *p = instruction; *p; p++) {
                if (isspace((unsigned char)*p)) { in_ws = true; continue; }
                if (in_ws && ci > 0) collapsed[ci++] = ' ';
                in_ws = false;
                collapsed[ci++] = *p;
            }
            collapsed[ci] = '\0';
        }
        free(instruction);
        instruction = collapsed;

        if (instruction && *instruction) {
            char *out;
            if (nlen > 0) {
                /* f"{label} — {instruction}" (em dash U+2014) */
                size_t need = strlen(label) + strlen("\u2014") + strlen(instruction) + 3;
                out = malloc(need);
                if (out) snprintf(out, need, "%s \u2014 %s", label, instruction);
            } else {
                out = strdup(instruction);
            }
            free(instruction); free(name); free(label);
            return out;
        }
    }
    free(instruction);

    /* return label if name else None */
    if (nlen > 0) { free(name); return label; }
    free(name); free(label);
    return NULL;
}
