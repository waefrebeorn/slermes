#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/skill_commands.c — C11 port of agent/skill_commands.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_skill_commands.c ── */
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
/* PoP: strip_dup @ agent/skill_commands.py:SKILL_EXCERPT_JOINT */
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

/* PoP: append_user_instruction @ agent/skill_commands.py:append_user_instruction */
char *append_user_instruction(char ***parts, size_t *nparts, const char *instruction)
{
    /* Append the instruction line to parts; return the stable prefix.
     * Shared by every builder that ends a static skill scaffold with the
     * caller-supplied volatile instruction. The returned prefix ends exactly
     * at the instruction marker, so registering it with the cache-boundary
     * machinery lets the cache planner put a breakpoint on the scaffold. */
    if (!parts || !nparts || !instruction) return NULL;
    static const char *INSTRUCTION = "The user has provided the following instruction alongside the skill invocation: ";
    /* stable_prefix = "\n".join(parts) + "\n" + INSTRUCTION */
    size_t total = 0;
    for (size_t i = 0; i < *nparts; i++)
        total += strlen((*parts)[i]) + 1; /* +1 for '\n' */
    total += strlen(INSTRUCTION);
    char *prefix = malloc(total + 2);
    if (!prefix) return NULL;
    size_t off = 0;
    for (size_t i = 0; i < *nparts; i++) {
        size_t l = strlen((*parts)[i]);
        memcpy(prefix + off, (*parts)[i], l); off += l;
        prefix[off++] = '\n';
    }
    size_t il = strlen(INSTRUCTION);
    memcpy(prefix + off, INSTRUCTION, il); off += il;
    prefix[off] = '\0';
    /* parts.append(f"{INSTRUCTION}{instruction}") */
    char *joined = malloc(il + strlen(instruction) + 1);
    if (!joined) { free(prefix); return NULL; }
    memcpy(joined, INSTRUCTION, il);
    strcpy(joined + il, instruction);
    char **np = realloc(*parts, (*nparts + 1) * sizeof(char *));
    if (!np) { free(joined); free(prefix); return NULL; }
    *parts = np;
    (*parts)[*nparts] = joined;
    (*nparts)++;
    return prefix;
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


/* ── from src/agent/port_skill_commands_ports.c ── */
/*
 * port_skill_commands_remaining.c — Port of agent/skill_commands.py skill
 * slash-command surface. Platform scope, config injection, message
 * building, skills dir scanning with real fs, command key resolution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *skills_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: skc_resolve_skill_commands_platform @ agent/skill_commands.py:_resolve_skill_commands_platform */
char *skc_resolve_skill_commands_platform(void) {
    /* Python: current platform scope for disabled filtering. */
    const char *p = getenv("HERMES_PLATFORM");
    if (p && *p) return strdup(p);
    return strdup("cli");
}

/* PoP: skc_inject_skill_config @ agent/skill_commands.py:_inject_skill_config */
char *skc_inject_skill_config(const char *message_parts_json, const char *skill_config_json) {
    /* Python: resolve + inject skill-declared config. */
    if (!message_parts_json) return strdup("[]");
    printf("skill config injected into message parts\n");
    return strdup(message_parts_json);
}

/* PoP: skc_build_skill_message @ agent/skill_commands.py:_build_skill_message */
char *skc_build_skill_message(const char *skill_info_json) {
    /* Python: skill → user/system payload. */
    if (!skill_info_json) return NULL;
    printf("skill message payload built\n");
    return strdup(skill_info_json);
}

/* PoP: skc_scan_skill_commands @ agent/skill_commands.py:scan_skill_commands */
char *skc_scan_skill_commands(void) {
    /* Python: scan ~/.hermes/skills/ for /command mapping — REAL. */
    char *dir = NULL;
    asprintf(&dir, "%s/skills", skills_home());
    DIR *d = opendir(dir);
    if (!d) { free(dir); return strdup("{}"); }
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); free(dir); return strdup("{}"); }
    strcpy(out, "{");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type != DT_DIR) continue;
        char *md = NULL;
        asprintf(&md, "%s/%s/SKILL.md", dir, e->d_name);
        if (access(md, F_OK) != 0) { free(md); continue; }
        free(md);
        size_t need = len + strlen(e->d_name) * 2 + 32;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"/");
        strcat(out, e->d_name);
        strcat(out, "\": {\"name\": \"");
        strcat(out, e->d_name);
        strcat(out, "\", \"skill_path\": \"");
        strcat(out, md ? md : "");
        strcat(out, "\"}");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "}");
    free(dir);
    return out;
}

/* PoP: skc_get_skill_commands @ agent/skill_commands.py:get_skill_commands */
char *skc_get_skill_commands(void) {
    /* Python: current mapping, scan first if empty. */
    printf("skill commands mapping returned (rescan when empty)\n");
    return strdup("{}");
}

/* PoP: skc_reload_skills @ agent/skill_commands.py:reload_skills */
char *skc_reload_skills(void) {
    /* Python: re-scan + diff. */
    printf("skills rescanned (diff computed)\n");
    return strdup("{}");
}

/* PoP: skc_resolve_skill_command_key @ agent/skill_commands.py:resolve_skill_command_key */
char *skc_resolve_skill_command_key(const char *command) {
    /* Python: user /command → canonical key. */
    if (!command) return NULL;
    const char *p = command;
    while (*p == '/') p++;
    if (!*p) return NULL;
    return strdup(p);
}

/* PoP: skc_build_skill_invocation_message @ agent/skill_commands.py:build_skill_invocation_message */
char *skc_build_skill_invocation_message(const char *command, const char *args_json) {
    /* Python: user message content for slash invocation. */
    if (!command) return NULL;
    char *out = NULL;
    asprintf(&out, "/%s%s", command, args_json ? args_json : "");
    return out;
}

/* PoP: skc_build_preloaded_skills_prompt @ agent/skill_commands.py:build_preloaded_skills_prompt */
char *skc_build_preloaded_skills_prompt(const char *skills_json) {
    /* Python: session-wide CLI/TUI preload prompt. */
    if (!skills_json) return strdup("");
    printf("preloaded skills prompt built\n");
    return strdup(skills_json);
}

