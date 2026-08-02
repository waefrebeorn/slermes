/*
 * port_moa_slash.c — Faithful C11 ports of the MoA slash-command handlers:
 *   - hermes_cli/moa_slash.py: handle_moa_slash_command, handle_moa_slash_command_sync
 *   - gateway/moa_slash.py:    _handle_moa_command, register_moa_slash_handler
 *
 * These are thin wrappers over the real MoA engine (handle_mixture_of_agents)
 * plus a result formatter. Each function carries its exact PoP comment.
 */

#include "port_moa_slash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hermes_json.h"
#include "online_research.h"

/* ---- shared: parse "/moa <prompt> [mode]" -> (mode, prompt) ----
 * Returns malloc'd prompt; *out_mode malloc'd. Caller frees both.
 * Returns NULL when usage is malformed (and *out_usage points to a usage str). */
static char *moa_slash_parse(const char *text, char **out_mode) {
    static const char *USAGE =
        "Usage: /moa <prompt> [mode]\nModes: standard, devil_advocate, trepidation, token_maxx, math";
    char *mode = strdup("standard");
    if (out_mode) *out_mode = mode;

    if (!text) return strdup(USAGE);
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", text);
    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;
    if (p[0] != '/') { free(mode); return strdup(USAGE); }
    /* strip leading '/', then "moa" */
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "moa", 3) == 0) { p += 3; while (*p == ' ' || *p == '\t') p++; }
    if (p[0] == '\0') { free(mode); return strdup(USAGE); }

    /* tokenize simply */
    char *tokens[256]; int nt = 0;
    char *save = p;
    char *tok = strtok_r(save, " \t", &save);
    while (tok && nt < 256) { tokens[nt++] = tok; tok = strtok_r(NULL, " \t", &save); }

    static const char *VALID[] = {"standard","devil_advocate","devil","trepidation",
                                  "token_maxx","tokenmaxx","maxx","math", NULL};
    char *prompt = NULL;
    if (nt > 0) {
        char *last = tokens[nt-1];
        int valid = 0;
        for (int i = 0; VALID[i]; i++) if (strcasecmp(last, VALID[i]) == 0) { valid = 1; break; }
        if (valid) {
            free(mode);
            mode = strdup(last);
            if (out_mode) *out_mode = mode;
            /* prompt = join of tokens[:-1] */
            size_t cap = 1;
            for (int i = 0; i < nt-1; i++) cap += strlen(tokens[i]) + 1;
            prompt = (char *)malloc(cap);
            prompt[0] = '\0';
            for (int i = 0; i < nt-1; i++) {
                strcat(prompt, tokens[i]);
                if (i < nt-2) strcat(prompt, " ");
            }
            if (prompt[0] == '\0') { free(prompt); free(mode); return strdup("Usage: /moa <prompt> [mode]\nPrompt cannot be empty."); }
        } else {
            /* whole thing is the prompt */
            size_t cap = 1; for (int i = 0; i < nt; i++) cap += strlen(tokens[i]) + 1;
            prompt = (char *)malloc(cap); prompt[0] = '\0';
            for (int i = 0; i < nt; i++) { strcat(prompt, tokens[i]); if (i < nt-1) strcat(prompt, " "); }
        }
    } else {
        prompt = strdup("");
    }
    return prompt;
}

/* normalize mode alias -> canonical engine mode string */
static void moa_slash_normalize_mode(char *mode) {
    if (strcasecmp(mode, "devil") == 0) strcpy(mode, "devil_advocate");
    else if (strcasecmp(mode, "tokenmaxx") == 0) strcpy(mode, "token_maxx");
    else if (strcasecmp(mode, "maxx") == 0) strcpy(mode, "token_maxx");
}

/* shared result formatter (matches Python handle_moa_slash_command output) */
static char *moa_slash_format(const char *result_json, const char *mode) {
    if (!result_json) return strdup("🎭 **MoA Result:**");
    json_t *parsed = json_parse(result_json, NULL);
    if (!parsed) {
        size_t n = strlen(result_json) + 32;
        char *out = (char *)malloc(n);
        snprintf(out, n, "🎭 **MoA Result**:\n%.4000s", result_json);
        return out;
    }
    json_t *success = json_obj_get(parsed, "success");
    if (!success || success->type != JSON_BOOL || success->bool_val == 0) {
        json_t *err = json_obj_get(parsed, "error");
        const char *emsg = (err && err->type == JSON_STRING) ? err->str_val : "Unknown error";
        size_t n = strlen(emsg) + 32;
        char *out = (char *)malloc(n);
        snprintf(out, n, "❌ **MoA Error**: %s", emsg);
        json_free(parsed);
        return out;
    }
    /* build output */
    char *parts[512]; int np = 0;
    size_t total = 0;
    #define ADD(...) do { \
        char tmp[8192]; snprintf(tmp, sizeof tmp, __VA_ARGS__); \
        parts[np] = strdup(tmp); total += strlen(parts[np]) + 1; np++; \
    } while (0)

    ADD("🎭 **Mixture of Agents** — Mode: `%s`", mode ? mode : "standard");
    json_t *health = json_obj_get(parsed, "provider_health");
    if (health) {
        char *hs = json_dumps(health, 0);
        ADD("📊 Provider health: %s", hs ? hs : "{}");
        free(hs);
    }
    ADD("");
    json_t *refs = json_obj_get(parsed, "reference_responses");
    if (refs && refs->type == JSON_ARRAY && refs->c.count > 0) {
        ADD("**📋 Reference Model Responses:**");
        int lim = (int)refs->c.count < 5 ? (int)refs->c.count : 5;
        for (int i = 0; i < lim; i++) {
            json_t *r = refs->c.items[i];
            const char *provider = "", *model = "", *content = "";
            if (r && r->type == JSON_OBJECT) {
                json_t *v;
                if ((v = json_obj_get(r, "provider")) && v->type == JSON_STRING) provider = v->str_val;
                if ((v = json_obj_get(r, "model")) && v->type == JSON_STRING) model = v->str_val;
                if ((v = json_obj_get(r, "content")) && v->type == JSON_STRING) content = v->str_val;
            }
            char prev[256];
            size_t cl = strlen(content);
            if (cl > 200) { memcpy(prev, content, 200); prev[200]='\0'; strcat(prev,"..."); }
            else strcpy(prev, content);
            ADD("  %d. **%s:%s** — %s", i+1, provider, model, prev);
        }
        if ((int)refs->c.count > 5) ADD("  ... and %d more", (int)refs->c.count - 5);
        ADD("");
    }
    json_t *agg = json_obj_get(parsed, "aggregator_response");
    if (agg && agg->type == JSON_STRING && agg->str_val[0]) {
        ADD("**🎯 Aggregated Result:**");
        ADD("%s", agg->str_val);
    }
    #undef ADD

    char *out = (char *)malloc(total + 1);
    out[0] = '\0';
    for (int i = 0; i < np; i++) { strcat(out, parts[i]); if (i < np-1) strcat(out, "\n"); free(parts[i]); }
    json_free(parsed);
    return out;
}

/* ============================================================
 * hermes_cli/moa_slash.py
 * ============================================================ */

/* PoP: moa_cli_handle_slash_command @ hermes_cli/moa_slash.py:handle_moa_slash_command */
char *moa_cli_handle_slash_command(const char *prompt, const char *mode) {
    char mbuf[64];
    snprintf(mbuf, sizeof mbuf, "%s", mode ? mode : "standard");
    moa_slash_normalize_mode(mbuf);
    char *result = NULL;
    if (strcasecmp(mbuf, "math") == 0) {
        result = moa_mixture_of_agents_math(prompt);
    } else {
        json_t *args = json_object();
        json_set(args, "user_prompt", json_string(prompt ? prompt : ""));
        json_set(args, "mode", json_string(mbuf));
        json_set(args, "use_online_research", json_bool(1));
        json_set(args, "research_intent", json_string("benchmark_update"));
        char *args_json = json_dumps(args, 0);
        json_free(args);
        result = handle_mixture_of_agents(args_json, NULL);
        free(args_json);
    }
    char *formatted = moa_slash_format(result, mbuf);
    free(result);
    return formatted;
}

/* PoP: moa_cli_handle_slash_command_sync @ hermes_cli/moa_slash.py:handle_moa_slash_command_sync */
char *moa_cli_handle_slash_command_sync(const char *prompt, const char *mode) {
    /* The Python sync wrapper is identical to the async one for our engine. */
    return moa_cli_handle_slash_command(prompt, mode);
}

/* ============================================================
 * gateway/moa_slash.py
 * ============================================================ */

/* PoP: moa_gateway_handle_command @ gateway/moa_slash.py:_handle_moa_command */
char *moa_gateway_handle_command(const char *text) {
    char *mode = NULL;
    char *prompt = moa_slash_parse(text, &mode);
    if (!prompt) { free(mode); return strdup("❌ MoA execution failed"); }
    /* if prompt looks like a usage string (starts with "Usage:"), return it */
    if (strncmp(prompt, "Usage:", 6) == 0) { char *u = strdup(prompt); free(prompt); free(mode); return u; }

    moa_slash_normalize_mode(mode);
    char *result = NULL;
    if (strcasecmp(mode, "math") == 0) {
        result = moa_mixture_of_agents_math(prompt);
    } else {
        json_t *args = json_object();
        json_set(args, "user_prompt", json_string(prompt));
        json_set(args, "mode", json_string(mode));
        json_set(args, "use_online_research", json_bool(1));
        json_set(args, "research_intent", json_string("benchmark_update"));
        char *args_json = json_dumps(args, 0);
        json_free(args);
        result = handle_mixture_of_agents(args_json, NULL);
        free(args_json);
    }
    char *formatted = moa_slash_format(result, mode);
    free(result);
    free(prompt);
    free(mode);
    return formatted; /* gateway layer wraps this text in EphemeralReply */
}

/* PoP: moa_gateway_register_slash_handler @ gateway/moa_slash.py:register_moa_slash_handler */
/* Python body is `pass` (comment: the handler is picked up by the gateway's
 * command dispatch via GatewaySlashCommandsMixin._handle_moa_command). The
 * C port's moa_gateway_handle_command is the same dispatch — faithful
 * abstract no-op. */
void moa_gateway_register_slash_handler(void) {
}
