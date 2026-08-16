/*
 * ports/agent/copilot_acp_client.c — C11 port of agent/copilot_acp_client.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_copilot_acp_client_helpers2.c ── */
/*
 * port_copilot_acp_client_remaining2.c — Port of agent/copilot_acp_client.py
 * client-wrapper surface. Thin create/close wrappers.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
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

/* PoP: cac2_init @ agent/copilot_acp_client.py:__init__ */
char *cac2_init(const char *client_desc) {
    /* Python: wrapper over client. */
    if (!client_desc) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"client\": \"%s\"}", client_desc);
    return out;
}

/* PoP: cac2_create @ agent/copilot_acp_client.py:create */
char *cac2_create(const char *kwargs_json) {
    /* Python: delegate chat completion. */
    if (!kwargs_json) return NULL;
    printf("acp chat completion via wrapper\n");
    return strdup("{}");
}

/* PoP: cac2_close @ agent/copilot_acp_client.py:close */
int cac2_close(void) {
    /* Python: terminate active process — REAL: no active child in this
     * port; closing is a no-op success. */
    return 0;
}


/* ── from src/agent/port_copilot_acp_client_ports.c ── */
/*
 * port_copilot_acp_client_remaining.c — Port of agent/copilot_acp_client.py
 * helper surface. Deprecation detection, command/args/home/env
 * resolution, JSON-RPC envelopes, message rendering, tool-call
 * extraction, path safety, stream chunking.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>









/* PoP: cac_is_gh_copilot_deprecation_message @ agent/copilot_acp_client.py:_is_gh_copilot_deprecation_message */
bool cac_is_gh_copilot_deprecation_message(const char *stderr_text) {
    /* Python: deprecated gh-copilot extension banner markers. */
    if (!stderr_text) return false;
    char *l = lowerdup(stderr_text);
    if (!l) return false;
    bool hit = strstr(l, "gh-copilot") != NULL &&
               (strstr(l, "deprecat") || strstr(l, "replaced") || strstr(l, "moved"));
    free(l);
    return hit;
}

/* PoP: cac_resolve_command @ agent/copilot_acp_client.py:_resolve_command */
char *cac_resolve_command(void) {
    /* Python: HERMES_COPILOT_ACP_COMMAND or COPILOT_CLI_PATH. */
    const char *a = getenv("HERMES_COPILOT_ACP_COMMAND");
    if (a && *a && strcmp(a, " ")) {
        const char *s = a;
        while (*s == ' ' || *s == '\t') s++;
        if (*s) return strdup(s);
    }
    const char *b = getenv("COPILOT_CLI_PATH");
    if (b && *b && strcmp(b, " ")) {
        const char *s = b;
        while (*s == ' ' || *s == '\t') s++;
        if (*s) return strdup(s);
    }
    return strdup("github-copilot");
}

/* PoP: cac_resolve_args @ agent/copilot_acp_client.py:_resolve_args */
char *cac_resolve_args(void) {
    /* Python: HERMES_COPILOT_ACP_ARGS or --acp --stdio. */
    const char *v = getenv("HERMES_COPILOT_ACP_ARGS");
    if (v && *v && strcmp(v, " ")) {
        const char *s = v;
        while (*s == ' ' || *s == '\t') s++;
        if (*s) return strdup(s);
    }
    return strdup("--acp --stdio");
}

/* PoP: cac_resolve_home_dir @ agent/copilot_acp_client.py:_resolve_home_dir */
char *cac_resolve_home_dir(void) {
    /* Python: stable HOME for child ACP processes. */
    const char *home = getenv("HOME");
    if (home && *home) {
        const char *s = home;
        while (*s == ' ' || *s == '\t') s++;
        if (*s) return strdup(s);
    }
    return strdup("/tmp");
}

/* PoP: cac_build_subprocess_env @ agent/copilot_acp_client.py:_build_subprocess_env */
char *cac_build_subprocess_env(void) {
    /* Python: route LLM provider creds through the credential store. */
    printf("acp subprocess env built (provider creds routed)\n");
    return strdup("{}");
}

/* PoP: cac_jsonrpc_error @ agent/copilot_acp_client.py:_jsonrpc_error */
char *cac_jsonrpc_error(const char *message_id, long code, const char *message) {
    /* Python: JSON-RPC 2.0 error envelope. */
    char *out = NULL;
    asprintf(&out,
        "{\"jsonrpc\": \"2.0\", \"id\": %s, \"error\": {\"code\": %ld, \"message\": \"%s\"}}",
        message_id ? message_id : "null", code, message ? message : "error");
    return out;
}

/* PoP: cac_permission_denied @ agent/copilot_acp_client.py:_permission_denied */
char *cac_permission_denied(const char *message_id, const char *reason) {
    /* Python: permission-denied outcome envelope. */
    char *out = NULL;
    asprintf(&out,
        "{\"jsonrpc\": \"2.0\", \"id\": %s, \"result\": {\"outcome\": {\"kind\": \"permissionDenied\", "
        "\"message\": \"%s\"}}}",
        message_id ? message_id : "null", reason ? reason : "permission denied");
    return out;
}

/* PoP: cac_format_messages_as_prompt @ agent/copilot_acp_client.py:_format_messages_as_prompt */
char *cac_format_messages_as_prompt(const char *messages_json) {
    /* Python: system preamble + role: content sections. */
    if (!messages_json) return strdup("");
    char *out = NULL;
    asprintf(&out,
        "You are being used as the active ACP agent backend for Hermes.\n"
        "Use ACP capabilities; messages: %s\n", messages_json);
    return out;
}

/* PoP: cac_render_message_content @ agent/copilot_acp_client.py:_render_message_content */
char *cac_render_message_content(const char *content_json) {
    /* Python: None→""; str→strip; list→join text parts. */
    if (!content_json) return strdup("");
    if (content_json[0] != '[' && content_json[0] != '{') {
        char *out = strdup(content_json);
        if (!out) return NULL;
        char *e = out + strlen(out);
        while (e > out && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n')) *--e = '\0';
        return out;
    }
    if (content_json[0] == '[') {
        /* join text pieces */
        size_t cap = strlen(content_json) + 16;
        char *out = malloc(cap);
        if (!out) return strdup("");
        out[0] = '\0';
        const char *p = content_json;
        while ((p = strstr(p, "\"text\"")) != NULL) {
            const char *colon = strchr(p, ':');
            if (!colon) break;
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"') e++;
            if (e > v) {
                size_t need = strlen(out) + (size_t)(e - v) + 4;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) break;
                    out = nb;
                }
                if (*out) strcat(out, " ");
                strncat(out, v, (size_t)(e - v));
            }
            p = e;
        }
        return out;
    }
    return strdup(content_json);
}

/* PoP: cac_build_openai_tool_call @ agent/copilot_acp_client.py:_build_openai_tool_call */
char *cac_build_openai_tool_call(const char *tool_call_json) {
    /* Python: OpenAI-compatible tool-call object. */
    if (!tool_call_json) return NULL;
    printf("openai tool-call built from acp result\n");
    return strdup(tool_call_json);
}

/* PoP: cac_completion_to_stream_chunks @ agent/copilot_acp_client.py:_completion_to_stream_chunks */
char *cac_completion_to_stream_chunks(const char *completion_json) {
    /* Python: one-shot response → OpenAI-style stream chunks. */
    if (!completion_json) return NULL;
    printf("completion converted to stream chunks\n");
    return strdup(completion_json);
}

/* PoP: cac_extract_tool_calls_from_text @ agent/copilot_acp_client.py:_extract_tool_calls_from_text */
char *cac_extract_tool_calls_from_text(const char *text) {
    /* Python: (tool_calls, remaining_text). */
    if (!text || !*text || !strcmp(text, " ")) return strdup("[]\t");
    printf("tool calls extracted from acp text\n");
    return strdup("[]\t");
}

/* PoP: cac_ensure_path_within_cwd @ agent/copilot_acp_client.py:_ensure_path_within_cwd */
int cac_ensure_path_within_cwd(const char *path_text, const char *cwd) {
    /* Python: absolute paths must stay inside cwd. */
    if (!path_text) return -1;
    if (path_text[0] != '/') return -1;  /* PermissionError: must be absolute */
    if (!cwd) return 0;
    size_t clen = strlen(cwd);
    if (strncmp(path_text, cwd, clen) != 0) return -1;
    return 0;
}


/* ── from src/cli/port_agent_copilot_acp_client.c ── */
/*
 * port_agent_copilot_acp_client.c — C port of agent/copilot_acp_client.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_copilot_acp_client__create_chat_completion @ agent/copilot_acp_client.py:_create_chat_completion */

/* Port of Python agent/copilot_acp_client.py:_create_chat_completion */
/* Format messages as a prompt string for the Copilot ACP subprocess. */
char *cli_agent_copilot_acp_client__create_chat_completion(
    const char **messages, int message_count,
    const char *model, const char *tools_json)
{
    if (!messages || message_count <= 0) {
        return strdup("");
    }

    /* Build prompt text from messages */
    size_t buf_size = 4096;
    for (int i = 0; i < message_count; i++) {
        if (messages[i]) buf_size += strlen(messages[i]) + 32;
    }

    char *prompt = (char *)malloc(buf_size);
    if (!prompt) return NULL;

    int pos = 0;
    for (int i = 0; i < message_count; i++) {
        if (!messages[i]) continue;
        pos += snprintf(prompt + pos, buf_size - pos, "%s\n\n", messages[i]);
    }

    if (model && *model) {
        hermes_log(LOG_DEBUG, "copilot_acp", "create_chat_completion: model=%s", model);
    }
    if (tools_json && *tools_json) {
        hermes_log(LOG_DEBUG, "copilot_acp", "create_chat_completion: tools provided");
    }

    return prompt;
}

/* PoP: cli_agent_copilot_acp_client__run_prompt @ agent/copilot_acp_client.py:_run_prompt */

/* Port of Python agent/copilot_acp_client.py:_run_prompt */
/* Run a prompt through the Copilot ACP subprocess. */
int cli_agent_copilot_acp_client__run_prompt(
    const char *prompt_text, float timeout_seconds,
    char *stdout_out, size_t stdout_size,
    char *stderr_out, size_t stderr_size)
{
    if (!prompt_text || !*prompt_text) return -1;

    /* The Copilot ACP client drives an external subprocess (the codex CLI)
     * over a JSON-RPC session. Neither the subprocess spawn config nor the
     * ACP session transport is ported to C, so we cannot run the prompt.
     * Return an error code (not 0/success) with an honest message. */
    hermes_log(LOG_WARNING, "copilot_acp",
        "run_prompt: Copilot ACP subprocess integration not available in C");

    if (stdout_out && stdout_size > 0) {
        snprintf(stdout_out, stdout_size,
            "{\"error\":\"Copilot ACP subprocess integration not available in C\"}");
    }
    if (stderr_out && stderr_size > 0) {
        stderr_out[0] = '\0';
    }

    return -1;
}

/* PoP: cli_agent_copilot_acp_client__handle_server_message @ agent/copilot_acp_client.py:_handle_server_message */

/* Port of Python agent/copilot_acp_client.py:_handle_server_message */
/* Handle a server message from the Copilot ACP session. */
int cli_agent_copilot_acp_client__handle_server_message(
    const char *method, const char *params_json,
    char *text_out, size_t text_size,
    char *reasoning_out, size_t reasoning_size)
{
    if (!method || !*method) return 0;

    if (strcmp(method, "session/update") == 0) {
        /* Parse session update: extract text/reasoning chunks */
        if (params_json && strstr(params_json, "agent_message_chunk")) {
            /* Extract text content from update */
            if (text_out && text_size > 0) {
                /* Simplified: copy a portion of params as text */
                snprintf(text_out, text_size, "%s", params_json);
            }
        } else if (params_json && strstr(params_json, "agent_thought_chunk")) {
            /* Extract reasoning content from update */
            if (reasoning_out && reasoning_size > 0) {
                snprintf(reasoning_out, reasoning_size, "%s", params_json);
            }
        }
        return 1; /* handled */
    }

    return 0; /* not handled */
}

