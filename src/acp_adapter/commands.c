/*
 * commands.c -- faithful C11 port of upstream Hermes acp_adapter/commands.py.
 *
 * PoP: w7_commands_7ff5d7_commands_table  @ acp_adapter/commands.py:SlashCommandsMixin._COMMANDS
 * PoP: w7_commands_7ff5d7_mid_turn_blocked @ acp_adapter/commands.py:_MID_TURN_BLOCKED_COMMANDS
 * PoP: w7_commands_7ff5d7_parse_slash      @ acp_adapter/commands.py:SlashCommandsMixin._handle_slash_command
 * PoP: w7_commands_7ff5d7_help_text        @ acp_adapter/commands.py:SlashCommandsMixin._cmd_help
 * PoP: w7_commands_7ff5d7_model_status     @ acp_adapter/commands.py:SlashCommandsMixin._cmd_model
 *
 * The ACP slash-command surface's transport, agent rebuild and session state
 * are not portable; the command registry, its parse contract and the two pure
 * text renderings are.
 */
#include "noc_w7_commands_7ff5d7.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name, *help, *desc, *hint; } cmd_spec_t;

/* Exact insertion order of SlashCommandsMixin._COMMANDS. */
static const cmd_spec_t COMMANDS[] = {
    { "help", "Show available commands", "List available commands", NULL },
    { "model", "Show or change current model", "Show current model and provider, or switch models",
      "model name to switch to" },
    { "tools", "List available tools", "List available tools with descriptions", NULL },
    { "context", "Show conversation context info", "Show conversation message counts by role", NULL },
    { "reset", "Clear conversation history", "Clear conversation history", NULL },
    { "compress", "Compress conversation context", "Compress conversation context", NULL },
    { "steer", "Inject guidance into the currently running agent turn",
      "Inject guidance into the currently running agent turn", "guidance for the active turn" },
    { "queue", "Queue a prompt to run after the current turn finishes",
      "Queue a prompt to run after the current turn finishes", "prompt to run next" },
    { "version", "Show Hermes version", "Show Hermes version", NULL },
};
#define NCOMMANDS (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

static const char *const BLOCKED[] = { "compress", "model", "reset" }; /* sorted(set(...)) */
#define NBLOCKED (sizeof(BLOCKED) / sizeof(BLOCKED[0]))

static int is_ws(char c) { return isspace((unsigned char)c) != 0; }

static int known_command(const char *cmd, size_t n) {
    for (size_t i = 0; i < NCOMMANDS; i++)
        if (strlen(COMMANDS[i].name) == n && !strncmp(COMMANDS[i].name, cmd, n)) return 1;
    return 0;
}

json_t *w7_commands_7ff5d7_commands_table(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < NCOMMANDS; i++) {
        json_t *row = json_array();
        json_append(row, json_string(COMMANDS[i].help));
        json_append(row, json_string(COMMANDS[i].desc));
        json_append(row, COMMANDS[i].hint ? json_string(COMMANDS[i].hint) : json_null());
        json_set(out, COMMANDS[i].name, row);
    }
    return out;
}

json_t *w7_commands_7ff5d7_mid_turn_blocked(void) {
    json_t *out = json_array();
    for (size_t i = 0; i < NBLOCKED; i++) json_append(out, json_string(BLOCKED[i]));
    return out;
}

json_t *w7_commands_7ff5d7_parse_slash(const char *text) {
    if (!text) text = "";
    const char *p = text;
    while (*p && is_ws(*p)) p++;

    char *cmd = NULL;
    char *args = NULL;
    if (!*p) {
        cmd = strdup("");
        args = strdup("");
    } else {
        const char *tok = p;
        while (*p && !is_ws(*p)) p++;
        size_t toklen = (size_t)(p - tok);
        while (toklen && *tok == '/') { tok++; toklen--; }
        cmd = (char *)malloc(toklen + 1);
        for (size_t i = 0; i < toklen; i++) cmd[i] = (char)tolower((unsigned char)tok[i]);
        cmd[toklen] = '\0';

        while (*p && is_ws(*p)) p++;
        size_t n = strlen(p);
        while (n && is_ws(p[n - 1])) n--; /* args = parts[1].strip() */
        args = (char *)malloc(n + 1);
        memcpy(args, p, n);
        args[n] = '\0';
    }

    json_t *out = json_object();
    json_set(out, "cmd", json_string(cmd));
    json_set(out, "args", json_string(args));
    json_set(out, "known", json_bool(known_command(cmd, strlen(cmd))));
    free(cmd);
    free(args);
    return out;
}

/* growable string buffer */
typedef struct { char *b; size_t len, cap; } sb_t;
static void sb_init(sb_t *s) {
    s->cap = 1024; s->len = 0;
    s->b = (char *)malloc(s->cap);
    if (s->b) s->b[0] = '\0';
}
static void sb_add(sb_t *s, const char *x, size_t n) {
    if (!s->b) return;
    if (s->len + n + 1 > s->cap) {
        size_t nc = s->cap * 2 + n + 1;
        char *nb = (char *)realloc(s->b, nc);
        if (!nb) return;
        s->b = nb; s->cap = nc;
    }
    memcpy(s->b + s->len, x, n);
    s->len += n;
    s->b[s->len] = '\0';
}
static void sb_adds(sb_t *s, const char *x) { sb_add(s, x, strlen(x)); }
static void sb_addc(sb_t *s, char c) { sb_add(s, &c, 1); }

char *w7_commands_7ff5d7_help_text(void) {
    sb_t s;
    sb_init(&s);
    sb_adds(&s, "Available commands:\n\n");
    for (size_t i = 0; i < NCOMMANDS; i++) {
        sb_adds(&s, "  /");
        sb_adds(&s, COMMANDS[i].name);
        size_t plen = strlen(COMMANDS[i].name);
        if (plen < 10) for (size_t k = plen; k < 10; k++) sb_addc(&s, ' ');
        sb_adds(&s, "  ");
        sb_adds(&s, COMMANDS[i].help);
        sb_addc(&s, '\n');
    }
    sb_adds(&s, "\nUnrecognized /commands are sent to the model as normal messages.");
    return s.b;
}

char *w7_commands_7ff5d7_model_status(const char *model, const char *fallback, const char *provider) {
    const char *m = (model && *model) ? model : ((fallback && *fallback) ? fallback : "unknown");
    const char *p = (provider && *provider) ? provider : "auto";
    size_t n = strlen(m) + strlen(p) + 64;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "Current model: %s\nProvider: %s", m, p);
    return out;
}
