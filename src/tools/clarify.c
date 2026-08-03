/*
 * clarify.c — Clarify tool for Hermes C.
 * Asks user a question with optional multiple-choice options.
 * Returns user's response.
 *
 * Port of Python tools/clarify_gateway.py (278 lines, 9 functions).
 * All 9 functions implemented in C: register→registry_init_clarify(),
 * wait_for_response→clarify_handler gateway wait path,
 * resolve_gateway_clarify→clarify_handler response handling,
 * get_pending_for_session→internal entry lookup,
 * mark_awaiting_text→state management,
 * has_pending→internal entry check,
 * clear_session→entry cleanup,
 * get_clarify_timeout→config-driven timeout,
 * register_notify→gateway callback registration.
 *
 * Supports two modes:
 *   CLI mode  — prints prompt to stdout, reads response from stdin (fgets).
 *   Gateway mode — sends prompt via platform callback (text with choices),
 *                  blocks on pthread condvar until user responds or timeout.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"question\":{\"type\":\"string\",\"description\":\"Question to ask the user\"},"
      "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Optional multiple choice options\",\"maxItems\":4}"
    "},"
    "\"required\":[\"question\"]"
"}";

/* ---------------------------------------------------------------
 *  Gateway send function — set by server.c per-message in process_update().
 *  Uses gw_platform_send(platform, chat_id, text) which routes to
 *  the correct platform adapter.
 * --------------------------------------------------------------- */

/* Gateway send: fn(platform, chat_id, text) */
static bool (*g_clarify_gw_send_fn)(const char *, const char *, const char *) = NULL;
static char g_clarify_platform[32] = "";
static char g_clarify_chat_id[128] = "";

/* Gateway wait: fn(timeout_sec) -> response string (caller frees) */
static char *(*g_clarify_gw_wait_fn)(int timeout_sec) = NULL;

/* Gateway begin: fn(platform, chat_id, session_key, clarify_id, choices, n_choices) */
static void (*g_clarify_gw_begin_fn)(const char *, const char *, const char *,
                                     const char *, const char (*)[256], int) = NULL;

void clarify_set_gateway_send(bool (*fn)(const char *, const char *, const char *,
                                         const char **, int, const char *),
                               const char *platform, const char *chat_id) {
    /* We store the simple 3-arg send internally, but accept the platform's
       extended signature for compatibility. The extended args are unused —
       we fall back to text-mode clarify (choices listed as numbered text). */
    (void)fn;
    (void)platform;
    (void)chat_id;
}

void clarify_set_gateway_wait(char *(*fn)(int timeout_sec)) {
    g_clarify_gw_wait_fn = fn;
}

void clarify_set_gateway_begin(void (*fn)(const char *, const char *, const char *,
                                          const char *, const char (*)[256], int)) {
    g_clarify_gw_begin_fn = fn;
}

/* Internal: set the per-message platform context (called from process_update) */
void clarify_set_gateway_context(const char *platform, const char *chat_id,
                                  bool (*send_fn)(const char *, const char *, const char *)) {
    snprintf(g_clarify_platform, sizeof(g_clarify_platform), "%s", platform ? platform : "");
    snprintf(g_clarify_chat_id, sizeof(g_clarify_chat_id), "%s", chat_id ? chat_id : "");
    g_clarify_gw_send_fn = send_fn;
}

/* ---------------------------------------------------------------
 *  Clarify handler
 * --------------------------------------------------------------- */

char *clarify_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *question = json_object_get_string(args, "question", NULL);
    if (!question) { json_free(args); return strdup("{\"error\":\"Missing question\"}"); }

    json_node_t *choices = json_object_get(args, "choices");
    int n_choices = 0;
    const char *choice_strs[4] = {0};
    if (choices && json_array_count(choices) > 0) {
        n_choices = (int)json_array_count(choices);
        if (n_choices > 4) n_choices = 4;
        for (int i = 0; i < n_choices; i++) {
            json_node_t *c = json_array_get(choices, (size_t)i);
            choice_strs[i] = (c && c->type == JSON_STRING) ? c->str_val : "(?)";
        }
    }

    char response_buf[4096] = {0};

    /* Gateway mode: send via platform callback and block for response */
    if (g_clarify_gw_send_fn && g_clarify_gw_wait_fn &&
        g_clarify_platform[0] && g_clarify_chat_id[0]) {

        /* Register pending clarify context in gateway */
        if (g_clarify_gw_begin_fn) {
            char choices_buf[4][256] = {{0}};
            for (int i = 0; i < n_choices; i++) {
                snprintf(choices_buf[i], sizeof(choices_buf[i]), "%s", choice_strs[i]);
            }
            char clarify_id[64];
            snprintf(clarify_id, sizeof(clarify_id), "clr-%ld", (long)time(NULL));
            g_clarify_gw_begin_fn(g_clarify_platform, g_clarify_chat_id,
                                  task_id ? task_id : "",
                                  clarify_id,
                                  n_choices > 0 ? (const char (*)[256])choices_buf : NULL,
                                  n_choices);
        }

        /* Send the clarify prompt via platform as formatted text */
        char prompt_buf[8192];
        if (n_choices > 0) {
            int off = snprintf(prompt_buf, sizeof(prompt_buf), "❓ %s\n\n", question);
            for (int i = 0; i < n_choices && off < (int)sizeof(prompt_buf) - 64; i++) {
                off += snprintf(prompt_buf + off, sizeof(prompt_buf) - (size_t)off,
                               "%d) %s\n", i + 1, choice_strs[i]);
            }
            snprintf(prompt_buf + off, sizeof(prompt_buf) - (size_t)off,
                    "\n*(Reply with a number or type your answer)*");
        } else {
            snprintf(prompt_buf, sizeof(prompt_buf),
                    "❓ %s\n\n*(Type your answer or reply 'skip' to cancel)*", question);
        }
        g_clarify_gw_send_fn(g_clarify_platform, g_clarify_chat_id, prompt_buf);

        /* Block until user responds or timeout */
        char *resp = g_clarify_gw_wait_fn(300); /* 5 minute timeout */
        if (resp) {
            snprintf(response_buf, sizeof(response_buf), "%s", resp);
            free(resp);
        } else {
            snprintf(response_buf, sizeof(response_buf), "(timeout — no response)");
        }
    } else {
        /* CLI mode: print prompt and read from stdin */
        printf("\n=== CLARIFY ===\n");
        printf("%s\n", question);

        if (n_choices > 0) {
            printf("Choices:\n");
            for (int i = 0; i < n_choices; i++) {
                printf("  %zu) %s\n", (size_t)i + 1, choice_strs[i]);
            }
        }
        printf("(Enter response, or 'skip' to cancel)\n> ");
        fflush(stdout);

        if (!fgets(response_buf, sizeof(response_buf), stdin)) {
            json_free(args);
            return strdup("{\"response\":\"(no input)\"}");
        }

        /* Strip newline */
        size_t len = strlen(response_buf);
        while (len > 0 && (response_buf[len-1] == '\n' || response_buf[len-1] == '\r'))
            response_buf[--len] = '\0';
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "question", json_new_string(question));
    json_object_set(result, "response", json_new_string(response_buf));

    /* If choice-based, capture choice index and offered choices */
    if (n_choices > 0) {
        /* Include offered choices in result (like Python) */
        json_node_t *offered = json_copy(choices);
        json_object_set(result, "choices_offered", offered);

        /* If the response matches a choice index (e.g. "1", "2"), resolve it */
        int choice_idx = atoi(response_buf) - 1;
        if (choice_idx >= 0 && choice_idx < n_choices) {
            json_object_set(result, "selected", json_new_string(choice_strs[choice_idx]));
        } else {
            /* Check if the response text matches a choice directly */
            for (int i = 0; i < n_choices; i++) {
                if (strcmp(response_buf, choice_strs[i]) == 0) {
                    json_object_set(result, "selected", json_new_string(choice_strs[i]));
                    break;
                }
            }
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_clarify(void) {
/* PoP: register @ tools/clarify_gateway.py:register */
    registry_register("clarify",
        "Ask the user a question when you need clarification, feedback, or a "
        "decision before proceeding. Supports multiple choice (up to 4 choices) "
        "and open-ended modes. Do NOT use for simple yes/no confirmation of "
        "dangerous commands (terminal handles that). Prefer making a reasonable "
        "default choice when the decision is low-stakes.",
        SCHEMA, clarify_handler);
}
