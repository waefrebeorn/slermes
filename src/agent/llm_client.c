/*
 * llm_client.c — LLM API client for Hermes C.
 * Sends chat completions requests via HTTP, parses JSON response.
 * Supports: chat completions, tool calls, streaming (optional), reasoning.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
#include "provider_metadata.h"
#include "error_classifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "hermes_url_safety.h"
#include "hermes_auth.h"
#include <sys/stat.h>
#include "hermes_redact.h"
#include "context_compressor_pure.h"  /* cc_reinject_pruned_skill_markers, context_compressor__extract_pruned_skill_names */

/* ================================================================
 *  Time helpers
 * ================================================================ */

/* P95: Monotonic time in seconds (for stream diagnostic timing) */
static double mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Forward: populate upstream diagnostic headers from HTTP response */
static void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers);
/* Port of Python agent/agent_runtime_helpers.py:extract_api_error_context(). */
/* Name parity: Python calls this extract_api_error_context() */
static time_t extract_api_error_context(const http_resp_t *resp);
/* Port of Python agent/agent_runtime_helpers.py:dump_api_request_debug(). */
bool dump_api_request_debug(const char *request_body, const char *session_id,
                             const char *reason, const char *provider,
                             const char *model, const char *url,
                             int http_status, const char *error_body);
/* Port of Python agent/agent_runtime_helpers.py:try_recover_primary_transport(). */
llm_response_t *try_recover_primary_transport(llm_config_t *cfg,
    const message_t **messages, size_t message_count,
    json_node_t *tools_json,
    llm_token_cb_t stream_cb, void *stream_data);

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* Port of Python setup_save_env: update a KEY=VALUE in .env file.
 * Reads existing .env, removes any line starting with KEY=, appends KEY=VALUE.
 * Works with SLERMES_HOME/.env, HERMES_HOME/.env, or ~/.slermes/.env. */
static bool _update_env_value(const char *key, const char *value) {
    if (!key || !value || !*value) return false;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;
    char env_path[4096];
    if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
        snprintf(env_path, sizeof(env_path), "%s/.env", home);
    else
        snprintf(env_path, sizeof(env_path), "%s/.slermes/.env", home);
    /* Read existing, filter out KEY= lines */
    FILE *f = fopen(env_path, "r");
    char tmp[4096], buf[8192] = {0};
    size_t pos = 0;
    if (f) {
        while (fgets(tmp, sizeof(tmp), f)) {
            size_t klen = strlen(key);
            if (strncmp(tmp, key, klen) == 0 && tmp[klen] == '=') continue;
            size_t tlen = strlen(tmp);
            if (pos + tlen < sizeof(buf)) { memcpy(buf + pos, tmp, tlen); pos += tlen; }
        }
        fclose(f);
    }
    size_t vlen = strlen(key) + 1 + strlen(value) + 1;
    if (pos + vlen < sizeof(buf))
        snprintf(buf + pos, sizeof(buf) - pos, "%s=%s\n", key, value);
    f = fopen(env_path, "w");
    if (!f) return false;
    fwrite(buf, 1, strlen(buf), f);
    fclose(f);
    return true;
}

/* Port of Python agent/token_refresh.py:nous_auto_refresh().
 * Attempt to refresh an expired Nous Portal OAuth token using NOUS_REFRESH_TOKEN
 * from the environment. On success, updates .env and process env with new token,
 * and updates cfg->api_key. Returns true if refresh succeeded. */
static bool nous_auto_refresh(llm_config_t *cfg) {
    const char *rt = getenv("NOUS_REFRESH_TOKEN");
    if (!rt || !rt[0]) return false;
    fprintf(stderr, "[llm] Nous Portal 401 — refreshing OAuth token...\n");
    oauth_token_t *new_tok = oauth_refresh_token(
        NOUS_OAUTH_TOKEN_ENDPOINT, DEFAULT_NOUS_CLIENT_ID, rt, 30);
    if (!new_tok || !new_tok->access_token) {
        if (new_tok) oauth_token_free(new_tok);
        fprintf(stderr, "[llm] Token refresh failed: %s\n", oauth_last_error());
        return false;
    }
    /* Update .env */
    _update_env_value("NOUS_API_KEY", new_tok->access_token);
    /* Update process environment */
    setenv("NOUS_API_KEY", new_tok->access_token, 1);
    /* Update config for immediate use */
    snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", new_tok->access_token);
    fprintf(stderr, "[llm] ✅ Token refreshed successfully\n");
    oauth_token_free(new_tok);
    return true;
}

/* Build the messages array JSON from message list */
static json_node_t *build_messages_json(const message_t **msgs, size_t count) {
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < count; i++) {
        json_node_t *msg = json_new_object();
        const char *role_str;
        switch (msgs[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        if (msgs[i]->content)
            json_object_set(msg, "content", json_new_string(msgs[i]->content));

        /* Tool call result */
        if (msgs[i]->role == MSG_TOOL && msgs[i]->tool_call_id)
            json_object_set(msg, "tool_call_id", json_new_string(msgs[i]->tool_call_id));

        /* Tool calls from assistant */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->tool_calls_count > 0) {
            json_node_t *tc_arr = json_new_array();
            for (int j = 0; j < msgs[i]->tool_calls_count; j++) {
                json_node_t *tc = json_new_object();
                json_object_set(tc, "id", json_new_string(msgs[i]->tool_calls[j].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_node_t *fn = json_new_object();
                json_object_set(fn, "name", json_new_string(msgs[i]->tool_calls[j].name));
                json_object_set(fn, "arguments", json_new_string(msgs[i]->tool_calls[j].arguments));
                json_object_set(tc, "function", fn);
                json_array_append(tc_arr, tc);
            }
            json_object_set(msg, "tool_calls", tc_arr);
        }

        /* Reasoning content echo-back (B32: DeepSeek/Kimi/MiMo thinking mode)
         * Some providers (DeepSeek V4, Kimi/coding, Xiaomi MiMo) require
         * reasoning_content on every assistant message for multi-turn thinking.
         * Mirror of Python agent/conversation_loop.py reasoning_content copy. */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->reasoning)
            json_object_set(msg, "reasoning_content", json_new_string(msgs[i]->reasoning));

        json_array_append(arr, msg);
    }
    return arr;
}

/* ================================================================
 *  Token counting + context management
 * ================================================================ */

/* Count approximate tokens in a message list.
 * Stops counting after max_tokens is exceeded. */
size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!msgs || !msgs[i]) continue;
        /* Role tokens (~1 token each) */
        total += 1;
        /* Content tokens */
        total += llm_estimate_tokens(msgs[i]->content);
        /* Tool call overhead */
        if (msgs[i]->tool_calls_count > 0)
            total += (size_t)msgs[i]->tool_calls_count * 10;
        /* Tool result overhead */
        if (msgs[i]->tool_call_id)
            total += 2;
        if (total > max_tokens) break;
    }
    return total;
}

/* Truncate context to fit within max_tokens while keeping
 * system message (index 0) and most recent messages. */
void llm_truncate_context(agent_state_t *state, size_t max_tokens) {
    if (!state || state->message_count < 2) return;

    size_t current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current <= max_tokens) return;

    /* Need to drop messages. Keep system + most recent. */
    size_t keep = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t target = keep + 4; /* Keep system + 4 most recent turns */
    if (state->message_count <= target) return;

    /* Remove from index keep to (message_count - target + keep) */
    size_t remove_count = state->message_count - target;
    for (size_t i = 0; i < remove_count; i++)
        message_free(state->messages[keep + i]);
    memmove(&state->messages[keep], &state->messages[keep + remove_count],
            (state->message_count - keep - remove_count) * sizeof(message_t *));
    state->message_count -= remove_count;

    /* Re-check after truncation */
    current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current > max_tokens && state->message_count > keep + 2) {
        /* Still over budget — drop more aggressively */
        llm_truncate_context(state, max_tokens);
    }
}

/* ================================================================
 *  Context Compression — Tool Result Pruning
 * ================================================================ */

/* Simple FNV-1a hash for content deduplication */
/* Port of Python: _dedupe_append (hash logic) */
static uint32_t compress_content_hash(const char *content) {
    uint32_t h = 2166136261u;
    if (!content) return 0;
    while (*content) {
        h ^= (unsigned char)*content++;
        h *= 16777619u;
    }
    return h;
}

/* Port of Python: _summarize_tool_result */
/* Create an informative 1-line summary of a tool call + result.
 * Matches Python context_compressor._summarize_tool_result() format. */
static void compress_summarize_tool(const char *tool_name, const char *tool_args,
                                     const char *tool_content,
                                     char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    /* Parse args JSON if present */
    /* Count lines in content */
    int line_count = 1;
    size_t content_len = 0;
    if (tool_content) {
        content_len = strlen(tool_content);
        for (const char *p = tool_content; *p; p++) {
            if (*p == '\n') line_count++;
        }
        if (line_count < 0) line_count = 0;
    }

    if (!tool_name) tool_name = "unknown";

    if (strcmp(tool_name, "terminal") == 0) {
        /* Extract command from args, exit_code from content */
        char cmd[256] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *c = json_get_str(jargs, "command", "");
                if (c[0]) {
                    size_t cl = strlen(c);
                    if (cl > 80) { memcpy(cmd, c, 77); cmd[77] = '\0'; strcat(cmd, "..."); }
                    else { memcpy(cmd, c, cl); cmd[cl] = '\0'; }
                }
                json_free(jargs);
            }
        }
        /* Extract exit_code from content via simple search */
        const char *exit_code = "?";
        char exit_buf[16] = "?";
        if (tool_content) {
            const char *exit_key = "\"exit_code\"";
            const char *found = strstr(tool_content, exit_key);
            if (found) {
                found += strlen(exit_key);
                while (*found == ' ' || *found == '\t' || *found == ':') found++;
                if (*found >= '0' && *found <= '9') {
                    int i = 0;
                    while (*found >= '0' && *found <= '9' && i < 14) {
                        exit_buf[i++] = *found++;
                    }
                    exit_buf[i] = '\0';
                    exit_code = exit_buf;
                }
            }
        }
        snprintf(out, out_sz, "[terminal] ran `%s` -> exit %s, %d lines output",
                 cmd[0] ? cmd : "?", exit_code, line_count);

    } else if (strcmp(tool_name, "read_file") == 0) {
        const char *path = "?";
        int offset = 1;
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                offset = (int)json_get_num(jargs, "offset", 1);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[read_file] read %s from line %d (%zu chars)",
                 path, offset, content_len);

    } else if (strcmp(tool_name, "write_file") == 0) {
        const char *path = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[write_file] wrote to %s (%d lines)",
                 path, line_count);

    } else if (strcmp(tool_name, "search_files") == 0) {
        const char *pattern = "?", *path = ".", *target = "content";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                pattern = json_get_str(jargs, "pattern", "?");
                path = json_get_str(jargs, "path", ".");
                target = json_get_str(jargs, "target", "content");
                json_free(jargs);
            }
        }
        /* Extract match_count from content */
        const char *count = "?";
        char count_buf[16] = "?";
        if (tool_content) {
            const char *key = "\"total_count\"";
            const char *found = strstr(tool_content, key);
            if (found) {
                found += strlen(key);
                while (*found == ' ' || *found == '\t' || *found == ':') found++;
                if (*found >= '0' && *found <= '9') {
                    int i = 0;
                    while (*found >= '0' && *found <= '9' && i < 14) {
                        count_buf[i++] = *found++;
                    }
                    count_buf[i] = '\0';
                    count = count_buf;
                }
            }
        }
        snprintf(out, out_sz, "[search_files] %s search for '%s' in %s -> %s matches",
                 target, pattern, path, count);

    } else if (strcmp(tool_name, "patch") == 0) {
        const char *path = "?";
        const char *mode = "replace";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                path = json_get_str(jargs, "path", "?");
                mode = json_get_str(jargs, "mode", "replace");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[patch] %s in %s (%zu chars result)", mode, path, content_len);

    } else if (strcmp(tool_name, "web_search") == 0) {
        const char *query = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                query = json_get_str(jargs, "query", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[web_search] query='%s' (%zu chars result)", query, content_len);

    } else if (strcmp(tool_name, "web_extract") == 0) {
        const char *url = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                json_node_t *urls = json_obj_get(jargs, "urls");
                if (urls && urls->type == JSON_ARRAY) {
                    int n = (int)json_len(urls);
                    if (n > 0) {
                        url = json_get_str(json_get(urls, 0), "", "?");
                        if (n > 1) {
                            char extra[32];
                            snprintf(extra, sizeof(extra), " (+%d more)", n - 1);
                            /* Append to url if space permits — use a temp buffer */
                            char url_buf[256];
                            snprintf(url_buf, sizeof(url_buf), "%s%s", url, extra);
                            /* Store in persistent buffer — use static (single-threaded ok) */
                            static char url_static[256];
                            strncpy(url_static, url_buf, sizeof(url_static) - 1);
                            url_static[sizeof(url_static) - 1] = '\0';
                            url = url_static;
                        }
                    }
                }
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[web_extract] %s (%zu chars)", url, content_len);

    } else if (strcmp(tool_name, "execute_code") == 0) {
        char code_preview[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *code = json_get_str(jargs, "code", "");
                if (code[0]) {
                    size_t cl = strlen(code);
                    size_t pl = cl > 60 ? 57 : cl;
                    int newline = 0;
                    for (size_t ii = 0; ii < pl && code[ii]; ii++) {
                        if (code[ii] == '\n' && !newline) { code_preview[ii] = ' '; newline = 1; }
                        else { code_preview[ii] = code[ii]; }
                    }
                    code_preview[pl] = '\0';
                    if (cl > 60) memcpy(code_preview + pl, "...", 4);
                }
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[execute_code] `%s` (%d lines output)",
                 code_preview[0] ? code_preview : "?", line_count);

    } else if (strcmp(tool_name, "delegate_task") == 0) {
        char goal[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *g = json_get_str(jargs, "goal", "");
                size_t gl = strlen(g);
                size_t cp = gl > 60 ? 57 : gl;
                memcpy(goal, g, cp);
                goal[cp] = '\0';
                if (gl > 60) memcpy(goal + cp, "...", 4);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[delegate_task] '%s' (%zu chars result)",
                 goal[0] ? goal : "?", content_len);

    } else if (strcmp(tool_name, "memory") == 0) {
        const char *action = "?", *target = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                action = json_get_str(jargs, "action", "?");
                target = json_get_str(jargs, "target", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[memory] %s on %s", action, target);

    } else if (strcmp(tool_name, "todo") == 0) {
        snprintf(out, out_sz, "[todo] updated task list");

    } else if (strcmp(tool_name, "clarify") == 0) {
        snprintf(out, out_sz, "[clarify] asked user a question");

    } else if (strcmp(tool_name, "vision_analyze") == 0) {
        char question[80] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                const char *q = json_get_str(jargs, "question", "");
                size_t ql = strlen(q);
                size_t cp = ql > 50 ? 47 : ql;
                memcpy(question, q, cp);
                question[cp] = '\0';
                if (ql > 50) memcpy(question + cp, "...", 4);
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[vision_analyze] '%s' (%zu chars)", question, content_len);

    } else if (strcmp(tool_name, "skill_view") == 0 ||
               strcmp(tool_name, "skills_list") == 0 ||
               strcmp(tool_name, "skill_manage") == 0) {
        const char *name = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                name = json_get_str(jargs, "name", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[%s] name=%s (%zu chars)", tool_name, name, content_len);

    } else if (strcmp(tool_name, "text_to_speech") == 0) {
        snprintf(out, out_sz, "[text_to_speech] generated audio (%zu chars)", content_len);

    } else if (strcmp(tool_name, "cronjob") == 0) {
        const char *action = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                action = json_get_str(jargs, "action", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[cronjob] %s", action);

    } else if (strcmp(tool_name, "process") == 0) {
        const char *action = "?";
        const char *sid = "?";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs) {
                action = json_get_str(jargs, "action", "?");
                sid = json_get_str(jargs, "session_id", "?");
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[process] %s session=%s", action, sid);

    } else {
        /* Generic fallback — include first 2 args like Python version */
        char first_arg[128] = "";
        if (tool_args && tool_args[0]) {
            char *err = NULL;
            json_node_t *jargs = json_parse(tool_args, &err);
            free(err);
            if (jargs && jargs->type == JSON_OBJECT) {
                int count = 0;
                /* Iterate object keys — use json_obj_get for first 2 keys */
                /* Simple approach: try common keys */
                const char *keys[] = {"path", "pattern", "query", "url", "ref",
                                      "command", "goal", "name", "action", "code",
                                      "mode", "target", "session_id", NULL};
                for (int k = 0; keys[k] && count < 2; k++) {
                    const char *v = json_get_str(jargs, keys[k], NULL);
                    if (v && *v) {
                        char sv[42];
                        size_t vl = strlen(v);
                        if (vl > 40) { memcpy(sv, v, 37); sv[37] = '\0'; strcat(sv, "..."); }
                        else { memcpy(sv, v, vl); sv[vl] = '\0'; }
                        int written = snprintf(first_arg + strlen(first_arg),
                                               sizeof(first_arg) - strlen(first_arg),
                                               " %s=%s", keys[k], sv);
                        if (written > 0) count++;
                    }
                }
                json_free(jargs);
            } else {
                json_free(jargs);
            }
        }
        snprintf(out, out_sz, "[%s]%s (%zu chars result)", tool_name, first_arg, content_len);
    }
}

/* Port of Python: _prune_old_tool_results */
/* Prune old tool results before LLM compression.
 * Deduplicates identical tool results, replaces old ones with 1-line summaries,
 * and truncates large tool_call arguments.
 *
 * Operates on state->messages IN PLACE (modifies content field of tool/assistant messages).
 * Does NOT add/remove messages. */
/* AG26: Port of Python agent/context_compressor.py:_sanitize_tool_pairs() */
/* AG26: Port of Python agent/context_compressor.py:_truncate_tool_call_args_json() */
static void compress_prune_tool_results(agent_state_t *state, size_t sys_keep) {
    if (!state || state->message_count <= sys_keep + 2) return;

    /* Build tool_call_id -> (tool_name, arguments) index from assistant messages */
    #define MAX_TOOL_INDEX 64   /* tool calls per message batch (was 256: 256×4.3KB ≈ 1.1MB .bss) */
    /* args is a pointer into the message's own arguments buffer (the
     * messages stay alive for the whole function) — was char[4096] per
     * entry: 64×4.2KB ≈ 263KB .bss → 64×200B ≈ 13KB. */
    static struct { char id[64]; char name[128]; const char *args; } tool_index[MAX_TOOL_INDEX];
    int idx_count = 0;

    for (size_t i = sys_keep; i < state->message_count && idx_count < MAX_TOOL_INDEX; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_ASSISTANT) continue;
        for (int j = 0; j < msg->tool_calls_count && idx_count < MAX_TOOL_INDEX; j++) {
            memcpy(tool_index[idx_count].id, msg->tool_calls[j].id, sizeof(tool_index[idx_count].id) - 1);
            tool_index[idx_count].id[sizeof(tool_index[idx_count].id) - 1] = '\0';
            memcpy(tool_index[idx_count].name, msg->tool_calls[j].name, sizeof(tool_index[idx_count].name) - 1);
            tool_index[idx_count].name[sizeof(tool_index[idx_count].name) - 1] = '\0';
            tool_index[idx_count].args = msg->tool_calls[j].arguments;
            idx_count++;
        }
    }

    if (idx_count == 0) return;

    /* Pass 1: Deduplicate identical tool results by content hash */
    #define MAX_HASH_MAP 256
    static struct { uint32_t hash; size_t index; } hash_map[MAX_HASH_MAP];
    int hash_count = 0;

    /* Walk backward so older duplicates get replaced */
    for (size_t i = state->message_count; i > sys_keep; i--) {
        size_t idx = i - 1;
        message_t *msg = state->messages[idx];
        if (!msg || msg->role != MSG_TOOL || !msg->content) continue;
        size_t clen = strlen(msg->content);
        if (clen < 200) continue;

        uint32_t h = compress_content_hash(msg->content);
        int found = -1;
        for (int hi = 0; hi < hash_count; hi++) {
            if (hash_map[hi].hash == h) { found = hi; break; }
        }
        if (found >= 0) {
            /* Older duplicate — replace with back-reference */
            const char *dup_text = "[Duplicate tool output — same content as a more recent call]";
            free(msg->content);
            msg->content = strdup(dup_text);
        } else if (hash_count < MAX_HASH_MAP) {
            hash_map[hash_count].hash = h;
            hash_map[hash_count].index = idx;
            hash_count++;
        }
    }

    /* Pass 2: Replace old tool results with 1-line summaries */
    /* Determine prune boundary — protect the most recent tool results within tail budget */
    size_t tail_budget_tokens = 2048; /* ~20% of 10K, similar to llm_compress_context */
    size_t tail_tokens = 0;
    int tail_count = 0;
    size_t prune_boundary = state->message_count;

    for (size_t i = state->message_count; i > sys_keep; i--) {
        size_t idx = i - 1;
        message_t *msg = state->messages[idx];
        if (!msg) continue;
        size_t mt = (size_t)context_message_tokens(msg);
        if (tail_tokens + mt > tail_budget_tokens && tail_count >= 3) {
            prune_boundary = idx;
            break;
        }
        tail_tokens += mt;
        tail_count++;
    }
    if (tail_count < 3) tail_count = 3;
    /* Ensure we don't prune past the protected tail */
    size_t actual_boundary = state->message_count;
    if (state->message_count > tail_count + sys_keep)
        actual_boundary = state->message_count - tail_count - sys_keep;
    else
        actual_boundary = sys_keep;
    if (actual_boundary > prune_boundary) actual_boundary = prune_boundary;

    for (size_t i = sys_keep; i < actual_boundary; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_TOOL || !msg->content) continue;

        /* Skip already-deduplicated or short content */
        size_t clen = strlen(msg->content);
        if (clen < 200) continue;
        if (strstr(msg->content, "[Duplicate tool output")) continue;
        if (strstr(msg->content, "[old tool output truncated]")) continue;

        /* Look up tool name from index */
        const char *tool_name = "unknown";
        const char *tool_args = "";
        for (int hi = 0; hi < idx_count; hi++) {
            if (msg->tool_call_id && strcmp(tool_index[hi].id, msg->tool_call_id) == 0) {
                tool_name = tool_index[hi].name;
                tool_args = tool_index[hi].args;
                break;
            }
        }

        char summary[512];
        compress_summarize_tool(tool_name, tool_args, msg->content, summary, sizeof(summary));
        free(msg->content);
        msg->content = strdup(summary);
    }

    /* Pass 3: Truncate large tool_call arguments in assistant messages outside protected tail */
    for (size_t i = sys_keep; i < actual_boundary; i++) {
        message_t *msg = state->messages[i];
        if (!msg || msg->role != MSG_ASSISTANT || msg->tool_calls_count == 0) continue;
        for (int j = 0; j < msg->tool_calls_count; j++) {
            size_t alen = strlen(msg->tool_calls[j].arguments);
            if (alen > 500) {
                /* Truncate arguments to 200 chars + ellipsis */
                char truncated[4160]; /* 4096 max + room */
                memcpy(truncated, msg->tool_calls[j].arguments, 200);
                truncated[200] = '\0';
                strcat(truncated, "...[truncated]");
                memcpy(msg->tool_calls[j].arguments, truncated, sizeof(msg->tool_calls[j].arguments) - 1);
                msg->tool_calls[j].arguments[sizeof(msg->tool_calls[j].arguments) - 1] = '\0';
            }
        }
    }
}

/* ================================================================
 *  Smart Context Compression
 * ================================================================ */

/* Summarization prompt for context compression */
#define COMPRESS_PROMPT \
    "Summarize the following conversation segment, preserving key " \
    "information: what was discussed, what tools/files were used, " \
    "what decisions were made, and any important findings. " \
    "Be concise but complete. Output only the summary:\n\n"

/* Port of Python: _serialize_for_summary */
/* AG26: Port of Python agent/context_compressor.py:_generate_summary() */
/* Compress middle messages by summarizing them via LLM.
 * Returns a malloc'd summary string, or NULL on failure.
 * Does NOT modify state.
 * Matches Python context_compressor._serialize_for_summary() format. */
static char *llm_compress_messages(const message_t **msgs, size_t count,
                                    llm_config_t *llm_cfg) {
    if (!msgs || count == 0 || !llm_cfg) return NULL;

    /* Content truncation limits matching Python _serialize_for_summary */
    #define SERIALIZE_CONTENT_MAX   6000    /* max chars per message body */
    #define SERIALIZE_CONTENT_HEAD  4000    /* chars kept from start */
    #define SERIALIZE_CONTENT_TAIL  1500    /* chars kept from end */
    #define SERIALIZE_TOOL_ARGS_MAX 1500    /* tool call argument chars */
    #define SERIALIZE_TOOL_ARGS_HEAD 1200   /* kept from start of tool args */

    /* Build conversation segment text with Python-style formatting */
    size_t total = 0;
    /* Estimate size: role labels ~80 bytes per message + content + tool calls */
    for (size_t i = 0; i < count; i++) {
        if (!msgs[i]) continue;
        total += 80; /* overhead per message (label, separator, newlines) */
        if (msgs[i]->content) {
            size_t clen = strlen(msgs[i]->content);
            if (clen > SERIALIZE_CONTENT_MAX)
                total += SERIALIZE_CONTENT_HEAD + SERIALIZE_CONTENT_TAIL + 30;
            else
                total += clen;
        }
        /* Tool calls in assistant messages */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->tool_calls_count > 0) {
            for (int j = 0; j < msgs[i]->tool_calls_count; j++) {
                total += strlen(msgs[i]->tool_calls[j].name) + 8;
                size_t alen = strlen(msgs[i]->tool_calls[j].arguments);
                if (alen > SERIALIZE_TOOL_ARGS_MAX)
                    total += SERIALIZE_TOOL_ARGS_HEAD + 5;
                else
                    total += alen;
            }
        }
    }
    total += strlen(COMPRESS_PROMPT) + 1;
    if (total > 100000) total = 100000; /* safety cap */

    char *text = (char *)malloc(total);
    if (!text) return NULL;
    text[0] = '\0';

    /* Helper: truncate content with head/middle/tail pattern */
    #define TRUNCATE_BUF_SIZE (SERIALIZE_CONTENT_HEAD + SERIALIZE_CONTENT_TAIL + 40)
    char trunc_buf[TRUNCATE_BUF_SIZE];

    strcat(text, COMPRESS_PROMPT);
    for (size_t i = 0; i < count; i++) {
        if (!msgs[i]) continue;
        size_t cur = strlen(text);
        size_t remain = total > cur ? total - cur : 0;
        if (remain < 20) break;

        /* Redact sensitive data before serialization (matches Python redact_sensitive_text) */
        const char *raw_content = msgs[i]->content ? msgs[i]->content : "";
        char *redacted = hermes_redact(raw_content);
        const char *content = redacted ? redacted : raw_content;
        size_t clen = strlen(content);

        if (msgs[i]->role == MSG_TOOL) {
            /* Tool result: [TOOL RESULT id]: content */
            const char *tid = msgs[i]->tool_call_id ? msgs[i]->tool_call_id : "";
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[TOOL RESULT %s]: %s\n", tid, trunc_buf);
            } else {
                snprintf(text + cur, remain, "[TOOL RESULT %s]: %s\n", tid, content);
            }

        } else if (msgs[i]->role == MSG_ASSISTANT) {
            /* Assistant: include tool call names + args */
            char tc_block[4096] = "";
            if (msgs[i]->tool_calls_count > 0) {
                size_t tc_pos = 0;
                tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                   "\n[Tool calls:\n");
                for (int j = 0; j < msgs[i]->tool_calls_count && tc_pos < sizeof(tc_block) - 100; j++) {
                    const char *tname = msgs[i]->tool_calls[j].name;
                    const char *targs = msgs[i]->tool_calls[j].arguments;
                    size_t alen = strlen(targs);
                    if (alen > SERIALIZE_TOOL_ARGS_MAX) {
                        char args_trunc[SERIALIZE_TOOL_ARGS_HEAD + 10];
                        size_t head_cp = SERIALIZE_TOOL_ARGS_HEAD < alen ? SERIALIZE_TOOL_ARGS_HEAD : alen;
                        memcpy(args_trunc, targs, head_cp);
                        args_trunc[head_cp] = '\0';
                        strcat(args_trunc, "...");
                        tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                           "  %s(%s)\n", tname, args_trunc);
                    } else {
                        tc_pos += snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos,
                                           "  %s(%s)\n", tname, targs);
                    }
                }
                snprintf(tc_block + tc_pos, sizeof(tc_block) - tc_pos, "]");
            }

            /* Content + tool calls block */
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[ASSISTANT]: %s%s\n",
                         trunc_buf, tc_block);
            } else {
                snprintf(text + cur, remain, "[ASSISTANT]: %s%s\n",
                         content, tc_block);
            }

        } else {
            /* User and other roles: [ROLE]: content */
            const char *role_label = "USER";
            if (msgs[i]->role == MSG_SYSTEM) role_label = "SYSTEM";
            if (clen > SERIALIZE_CONTENT_MAX) {
                size_t head_cp = SERIALIZE_CONTENT_HEAD < clen ? SERIALIZE_CONTENT_HEAD : clen;
                size_t tail_start = clen > SERIALIZE_CONTENT_TAIL ? clen - SERIALIZE_CONTENT_TAIL : 0;
                snprintf(trunc_buf, sizeof(trunc_buf),
                         "%.*s\n...[truncated]...\n%s",
                         (int)head_cp, content,
                         tail_start > 0 ? content + tail_start : "");
                snprintf(text + cur, remain, "[%s]: %s\n", role_label, trunc_buf);
            } else {
                snprintf(text + cur, remain, "[%s]: %s\n", role_label, content);
            }
        }
        /* Free redacted copy if allocated */
        if (redacted) free(redacted);
    }

    /* Create a temporary config for the summarization call.
     * Copy runtime llm config (state->llm) which has the correct
     * provider, model, base_url, api_key from CLI/config resolution.
     * Do NOT clear base_url/api_key — compression needs them to reach the API. */
    llm_config_t compress_cfg;
    memcpy(&compress_cfg, llm_cfg, sizeof(compress_cfg));

    /* Build summarization messages */
    message_t *sys = message_new(MSG_SYSTEM,
        "You are a summarization agent creating a context checkpoint. "
        "Treat the conversation turns below as source material for a "
        "compact record of prior work. "
        "Produce only the structured summary; do not add a greeting, "
        "preamble, or prefix. "
        "NEVER include API keys, tokens, passwords, secrets, credentials, "
        "or connection strings in the summary -- replace any that appear "
        "with [REDACTED].\n\n"
        "Use this exact structure:\n"
        "## Active Task\n"
        "[The user's most recent request verbatim -- pick up exactly here]\n\n"
        "## Completed Actions\n"
        "[Numbered list: ACTION target -- outcome]\n\n"
        "## Active State\n"
        "[Working directory, modified files, test status, running processes]\n\n"
        "## Key Decisions\n"
        "[Technical decisions and WHY they were made]\n\n"
        "## Blocked\n"
        "[Any blockers, errors, or issues not yet resolved]\n\n"
        "## Remaining Work\n"
        "[What remains -- framed as context, not instructions]");
    message_t *user = message_new(MSG_USER, text);
    const message_t *compress_msgs[2] = {sys, user};

    /* B01: Scaled summary budget — proportional to content size, capped at 5% of 128K = 6400 */
    /* Matches Python _compute_summary_budget: content_tokens * 0.20, min 2000, max 5% of context */
    size_t content_tokens = (strlen(text) + 3) / 4;  /* rough estimate */
    int scaled_budget = (int)(content_tokens * 0.20);
    int max_budget = 6400;  /* 5% of 128K context */
    if (max_budget > 12000) max_budget = 12000;  /* Python _SUMMARY_TOKENS_CEILING */
    if (scaled_budget < 2000) scaled_budget = 2000;  /* Python _MIN_SUMMARY_TOKENS */
    if (scaled_budget > max_budget) scaled_budget = max_budget;
    compress_cfg.max_tokens = scaled_budget;

    llm_response_t *resp = llm_chat_completion(&compress_cfg,
                                                compress_msgs, 2, NULL);
    char *summary = NULL;
    if (resp && resp->content) {
        summary = strdup(resp->content);
    }
    /* Ghost-skill defense (#32106): the summarizer routinely paraphrases
     * [SKILL_PRUNED: ...] reload markers into vague prose, erasing the
     * instruction. Restore any marker the model dropped (collected from the
     * turns being summarized) before the summary is inserted. Mirrors
     * Python _reinject_pruned_skill_markers in _generate_summary. */
    if (summary) {
        char *names[64];
        int ncount = 0;
        context_compressor__extract_pruned_skill_names(text, names, &ncount, 64);
        if (ncount > 0) {
            char *reinjected = NULL;
            cc_reinject_pruned_skill_markers(summary,
                                             (const char **)names, ncount,
                                             &reinjected);
            for (int i = 0; i < ncount; i++) free(names[i]);
            if (reinjected) {
                free(summary);
                summary = reinjected;
            }
        }
    }
    llm_response_free(resp);
    message_free(sys);
    message_free(user);
    free(text);
    return summary;
}

/* Port of Python agent/conversation_compression.py: compress_context, _compression_lock_holder, check_compression_model_feasibility, replay_compression_warning, try_shrink_image_parts_in_messages — consolidated in llm_compress_context */
/* AG26: Port of Python agent/conversation_compression.py:_compression_lock_holder() */
/* AG26: Port of Python agent/conversation_compression.py:check_compression_model_feasibility() */
/* AG26: Port of Python agent/conversation_compression.py:replay_compression_warning() */
/* AG26: Port of Python agent/conversation_compression.py:try_shrink_image_parts_in_messages() */
/* Attempt to compress context so it fits within max_tokens budget.
 * Returns summary string on success (caller must insert), NULL if compression
 * is disabled or fails (caller should fall back to dropping). */
char *llm_compress_context(agent_state_t *state, size_t max_tokens,
                            bool enabled) {
    if (!state || !enabled || state->message_count < 3) return NULL;

    /* Phase 1: Prune old tool results before counting tokens.
     * This deduplicates identical outputs, replaces large tool results with
     * 1-line summaries, and truncates oversized tool_call arguments.
     * Matches Python context_compressor._prune_old_tool_results(). */
    size_t keep = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    if (keep >= state->message_count) return NULL;
    compress_prune_tool_results(state, keep);

    size_t current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current <= max_tokens) return NULL;

    /* B02: Token-budget tail — walk backwards to determine how many
     * messages to preserve. Budget = 20% of max_tokens (summary_target_ratio). */
    /* AG26: Port of Python agent/context_compressor.py:_compute_summary_budget() */
    /* AG26: Port of Python agent/context_compressor.py:_find_last_user_message_idx() */
    /* AG26: Port of Python agent/context_compressor.py:_find_last_assistant_message_idx() */
    /* AG26: Port of Python agent/context_compressor.py:_ensure_last_user_message_in_tail() */
    /* AG26: Port of Python agent/context_compressor.py:_ensure_last_assistant_message_in_tail() */
    size_t tail_budget = max_tokens / 5; /* 20% */
    if (tail_budget < 1024) tail_budget = 1024; /* floor: at least 1K tokens */
    size_t tail_tokens = 0;
    int tail_count = 0;
    for (int i = (int)state->message_count - 1; i >= (int)keep; i--) {
        if (!state->messages[i]) continue;
        size_t mt = (size_t)context_message_tokens(state->messages[i]);
        if (tail_tokens + mt > tail_budget && tail_count >= 2) break;
        tail_tokens += mt;
        tail_count++;
    }
    if (tail_count < 2) tail_count = 2;

    size_t compress_count = (tail_count < (int)(state->message_count - keep))
        ? state->message_count - keep - tail_count : 0;
    if (compress_count < 2 || compress_count > 20) return NULL;

    /* Boundary alignment: don't split tool_call/tool_result pairs.
     * If the first tail message is a tool result, exclude it from the tail
     * and include it in the compress region instead. This prevents the
     * compressed summary from referencing a tool_call whose result is
     * still in the tail, or a tool result with no preceding call. */
    /* AG26: Port of Python agent/context_compressor.py:_align_boundary_forward() */
    /* AG26: Port of Python agent/context_compressor.py:_align_boundary_backward() */
    /* AG26: Port of Python agent/context_compressor.py:_protect_head_size() */
    size_t compress_end = keep + compress_count;
    if (compress_end < state->message_count &&
        state->messages[compress_end] &&
        state->messages[compress_end]->role == MSG_TOOL) {
        /* Move the orphan tool result into the compress region */
        compress_count++;
        if (compress_count > 20) compress_count = 20;
    } else if (compress_end > keep && compress_end - 1 < state->message_count &&
               compress_end - 1 >= keep &&
               state->messages[compress_end - 1] &&
               state->messages[compress_end - 1]->role == MSG_ASSISTANT &&
               state->messages[compress_end - 1]->tool_calls_count > 0) {
        /* The last compress message is an assistant with pending tool calls.
         * Move it into the tail so the tool result has its call visible. */
        if (tail_count > 0) {
            compress_count--;
        }
    }
    if (compress_count < 2) return NULL;

    return llm_compress_messages(
        (const message_t **)&state->messages[keep],
        compress_count, &state->llm);
}

/* PoP: llm_conversation_history_after_compression @ agent/conversation_compression.py:conversation_history_after_compression */
/* Return the correct flush baseline after a compression boundary.
 * Legacy compression rotates to a fresh child session, so callers must clear
 * conversation_history to NULL and let the next persistence call write the
 * whole compacted list. In-place compaction already soft-archived the previous
 * rows and inserted `messages` as the new live transcript under the same
 * session id — returning them again would double-append. A shallow copy is
 * intentional: it captures the current compacted dict identities as history
 * while later same-turn appends stay new. Returns a json_copy of messages when
 * last_compaction_in_place is set, else NULL. */
json_t *llm_conversation_history_after_compression(
    bool last_compaction_in_place, const json_t *messages)
{
    if (!last_compaction_in_place) return NULL;
    if (!messages) return NULL;
    return json_copy(messages);
}

/* ================================================================
 *  LLM API call
 * ================================================================ */

llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_node_t *tools_json)
{
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    /* AL07: Responses API mode — use codex_responses provider ops */
    if (prov && cfg->api_mode[0] && strcmp(cfg->api_mode, "codex_responses") == 0) {
        prov->ops = &PROVIDER_OPS_CODEX;
    }

    /* P91: Pass system cache state to provider */
    if (prov) prov->system_cached = cfg->system_cached;

    /* Copy LLM request params from config to provider */
    if (prov) {
        prov->config.max_tokens = cfg->max_tokens;
        prov->config.temperature = cfg->temperature;
        prov->config.top_p = cfg->top_p;
        prov->config.stop_count = cfg->stop_count;
        memcpy(prov->config.stop_sequences, cfg->stop_sequences,
               sizeof(prov->config.stop_sequences));
        prov->config.presence_penalty = cfg->presence_penalty;
        prov->config.frequency_penalty = cfg->frequency_penalty;
        prov->config.seed = cfg->seed;
        prov->config.logprobs = cfg->logprobs;
        prov->config.top_logprobs = cfg->top_logprobs;
        memcpy(prov->config.user, cfg->user, sizeof(prov->config.user));
        memcpy(prov->config.service_tier, cfg->service_tier,
               sizeof(prov->config.service_tier));
        memcpy(prov->config.reasoning_effort, cfg->reasoning_effort,
               sizeof(prov->config.reasoning_effort));
        memcpy(prov->config.response_format, cfg->response_format,
               sizeof(prov->config.response_format));
        memcpy(prov->config.metadata, cfg->metadata,
               sizeof(prov->config.metadata));
        memcpy(prov->config.tool_choice, cfg->tool_choice,
               sizeof(prov->config.tool_choice));
        prov->config.parallel_tool_calls = cfg->parallel_tool_calls;
        prov->config.top_k = cfg->top_k;
        prov->config.candidate_count = cfg->candidate_count;
        prov->config.json_mode = cfg->json_mode;
        prov->config.response_format_strict = cfg->response_format_strict;
        memcpy(prov->config.safety_settings, cfg->safety_settings,
               sizeof(prov->config.safety_settings));
        prov->config.max_tool_calls = cfg->max_tool_calls;
        prov->config.n = cfg->n;
    }

    if (prov && prov->ops) {
        int nous_retry = 0;
        while (1) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        /* P158: Verify URL is trusted for this provider's API key */
        const char *effective_key = cfg->api_key;
        if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
            effective_key = "";
        }
        char *headers = ops->build_headers(prov, effective_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, false);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            break;
        }
        http_client_t *client = http_client_new_with_retry(30, 3, 1000);
        http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                                   headers, body, strlen(body));

        /* P95: Save body copy for debug dump before freeing url/headers/body */
        char *saved_body = NULL;
        char *saved_url = NULL;
        if (getenv("SLERMES_DEBUG") && http_resp && url) {
            saved_body = xstrdup(body);
            saved_url = xstrdup(url);
        }
        free(url); free(headers); free(body);

        /* P95: Capture upstream diagnostic headers from response */
        if (http_resp)
            populate_stream_diag_headers(resp, http_resp->headers);

        /* Classify HTTP error responses for logging and retry decisions */
        if (http_resp && http_resp->status >= 400) {
            classified_error_t err;
            classify_api_error(http_resp->status, http_resp->body,
                           cfg->provider, cfg->model,
                           0, 0, &err);
            char err_buf[256];
            error_format(&err, err_buf, sizeof(err_buf));
            /* P95: Diagnostic output -- SLERMES_DEBUG only */
            if (getenv("SLERMES_DEBUG")) {
                if (resp->diag.upstream_headers[0])
                    fprintf(stderr, "[llm] %s [upstream: %s]\n",
                            err_buf, resp->diag.upstream_headers);
                else
                    fprintf(stderr, "[llm] %s\n", err_buf);
                /* Also dump request body for 4xx debugging */
                if (saved_body && http_resp->status >= 400)
                    dump_api_request_debug(saved_body, "unknown", "api_error",
                                           cfg->provider, cfg->model, saved_url,
                                           http_resp->status, http_resp->body);
            }
            if (err.should_compress)
                resp->compress_hint = true;
            if (err.should_rotate_credential)
                resp->credential_expired = true;

            /* Auto-refresh Nous OAuth token on 401 (once per call) */
            if (http_resp->status == 401 && nous_retry == 0 &&
                (strcasecmp(cfg->provider, "nous") == 0 ||
                 strcasecmp(cfg->provider, "nous-research") == 0)) {
                if (nous_auto_refresh(cfg)) {
                    /* Update provider's API key too */
                    snprintf(prov->api_key, sizeof(prov->api_key), "%s", cfg->api_key);
                    /* Clean up and retry */
                    http_response_free(http_resp);
                    http_client_free(client);
                    free(saved_body); free(saved_url);
                    nous_retry++;
                    continue;
                }
            }
        }

        /* Report HTTP result to credential pool if configured */
        if (cfg->cred_pool && http_resp) {
            credential_pool_t *pool = (credential_pool_t *)cfg->cred_pool;
            time_t rl_reset = extract_api_error_context(http_resp);
            credential_pool_report(pool, 0, http_resp->status,
                                   resp->output_tokens + resp->input_tokens,
                                   -1, rl_reset);
        }

        free(saved_body); free(saved_url);

        if (!http_resp || http_resp->status < 0) {
            resp->content = strdup("HTTP request failed");
            if (http_resp) http_response_free(http_resp);
            http_client_free(client);
            break;
        }
        provider_response_t *presp = ops->parse_response(prov, http_resp->body);
        http_response_free(http_resp); http_client_free(client);
        if (presp) {
            resp->content = presp->content; presp->content = NULL;
            resp->reasoning = presp->reasoning; presp->reasoning = NULL;
            resp->input_tokens = presp->input_tokens;
            resp->output_tokens = presp->output_tokens;
            resp->tool_calls_count = presp->tool_calls_count;
            for (int i = 0; i < presp->tool_calls_count && i < 64; i++)
                resp->tool_calls[i] = presp->tool_calls[i];
            ops->free_response(presp);
        }
        break; /* success */
        } /* while (1) retry loop */
        provider_free(prov);
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));

    /* Messages */
    json_node_t *msgs = build_messages_json(messages, message_count);
    json_object_set(req, "messages", msgs);

    /* Add tools if provided */
    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(req, "tools", json_copy(tools_json));

    /* Serialize */
    char *body = json_serialize(req);

    /* Determine URL */
    char url[512];
    const char *base = cfg->base_url;
    if (base && strlen(base) > 0) {
        /* Check if base already ends with /chat/completions */
        if (strstr(base, "/chat/completions"))
            snprintf(url, sizeof(url), "%s", base);
        else
            snprintf(url, sizeof(url), "%s/chat/completions", base);
    } else {
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");
    }

    /* Build auth header */
    char auth_header[2048];
    const char *effective_key = cfg->api_key;
    if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
        effective_key = "";
    }
    if (effective_key[0]) {
        snprintf(auth_header, sizeof(auth_header),
                "Authorization: Bearer %s\r\nContent-TContent-Type: application/json",
                 effective_key);
    } else {
        snprintf(auth_header, sizeof(auth_header),
                 "Content-Type: application/json");
    }

    /* Make HTTP request */
    http_client_t *client = http_client_new_with_retry(30, 3, 1000);
    http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                               auth_header, body, strlen(body));

    json_free(req);
    free(body);

    if (!http_resp ||        http_resp->status < 0) {
        resp->content = xstrdup("HTTP request failed");
        if (http_resp) http_response_free(http_resp);
        http_client_free(client);
        return resp;
    }

    /* Parse JSON response */
    char *json_err = NULL;
    json_node_t *root = json_parse(http_resp->body, &json_err);
    http_response_free(http_resp);

    if (!root) {
        resp->content = (char *)malloc(256);
        snprintf(resp->content, 256, "JSON parse error: %s", json_err ? json_err : "unknown");
        free(json_err);
        http_client_free(client);
        return resp;
    }

    /* Extract usage if present */
    json_node_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
    }

    /* Extract choices */
    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *message = json_object_get(choice, "message");

        if (message) {
            resp->content = xstrdup(json_object_get_string(message, "content", ""));

            /* Check for reasoning */
            json_node_t *reasoning = json_object_get(message, "reasoning");
            if (reasoning && reasoning->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning->str_val);

            /* Check for tool calls */
            json_node_t *tool_calls = json_object_get(message, "tool_calls");
            if (tool_calls && json_array_count(tool_calls) > 0) {
                resp->tool_calls_count = (int)json_array_count(tool_calls);
                if (resp->tool_calls_count > 64)
                    resp->tool_calls_count = 64;
                for (int i = 0; i < resp->tool_calls_count; i++) {
                    json_node_t *tc = json_array_get(tool_calls, (size_t)i);
                    const char *id = json_object_get_string(tc, "id", "");
                    snprintf(resp->tool_calls[i].id, sizeof(resp->tool_calls[i].id), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", "");
                        const char *args = json_object_get_string(fn, "arguments", "{}");
                        snprintf(resp->tool_calls[i].name, sizeof(resp->tool_calls[i].name), "%s", name);
                        snprintf(resp->tool_calls[i].arguments, sizeof(resp->tool_calls[i].arguments), "%s", args);
                    }
                }
            }

            /* Reasoning content field (some providers use this instead of "reasoning") */
            json_node_t *reasoning_content = json_object_get(message, "reasoning_content");
            if (!resp->reasoning && reasoning_content && reasoning_content->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning_content->str_val);

            /* L07: xAI encrypted reasoning content — serialize the encrypted_content array */
            json_node_t *enc_content = json_object_get(message, "encrypted_content");
            if (enc_content && json_array_count(enc_content) > 0) {
                char *ser = json_serialize(enc_content);
                if (ser) { resp->encrypted_content = ser; }
            }
        }
    }

    json_free(root);
    http_client_free(client);
    return resp;
}

/* Port of Python agent/agent_runtime_helpers.py:try_recover_primary_transport().
 * Give the primary provider one more attempt after retries are exhausted,
 * using a fresh connection. Skip for aggregator providers (OpenRouter,
 * Nous) which manage their own server-side retries.
 * Returns malloc'd response or NULL on failure. Caller must free. */
llm_response_t *try_recover_primary_transport(
    llm_config_t *cfg,
    const message_t **messages,
    size_t message_count,
    json_node_t *tools_json,
    llm_token_cb_t stream_cb,
    void *stream_data)
{
    /* Skip for aggregator providers — they manage server-side retries */
    const char *prov = cfg->provider;
    if (prov[0] && (strcasecmp(prov, "openrouter") == 0 ||
                    strcasecmp(prov, "nous") == 0 ||
                    strcasecmp(prov, "nous-research") == 0))
        return NULL;

    fprintf(stderr, "[recover] Primary transport recovery — one more attempt on %s/%s\n",
            cfg->provider, cfg->model);

    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);

    if (stream_cb) {
        return llm_chat_completion_stream(cfg, messages, message_count,
                                           tools_json, stream_cb, stream_data);
    }
    return llm_chat_completion(cfg, messages, message_count, tools_json);
}

/* ================================================================
 *  Streaming: per-chunk callback context
 * ================================================================ */

typedef struct {
    llm_response_t  *resp;      /* accumulated response */
    llm_token_cb_t   token_cb;  /* token callback */
    void            *userdata;  /* callback userdata */
    const provider_ops_t *prov_ops;  /* provider ops for per-chunk parsing (optional) */
    provider_t      *prov;      /* provider instance (optional) */
    /* Track tool_calls across chunks (streaming builds them incrementally) */
    int              max_tc_idx; /* highest tool_call index seen */
    char             tc_id[64][64];  /* id per index */
    char             tc_name[64][128]; /* name per index */
    char             tc_args[64][4096]; /* args buffer per index */
    bool             streaming_done; /* set when streaming is complete */
    /* P95: Stream diagnostic timing */
    double           req_start_time;  /* monotonic time of request send */
    size_t           token_count;     /* running token count */
    bool             first_token_flag; /* first content token tracked */
} stream_ctx_t;

/* Provider-aware stream chunk handler.
 * Uses provider's parse_stream_chunk for text deltas, falls back
 * to OpenAI-style delta parsing for tool calls. */
static int on_provider_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    if (!ctx->prov_ops || !ctx->prov) {
        /* No provider — skip */
        return 0;
    }

    /* Try provider's parse_stream_chunk first */
    provider_response_t *delta = ctx->prov_ops->parse_stream_chunk(ctx->prov, data);
    if (delta) {
        /* Text content from provider */
        if (delta->content && delta->content[0]) {
            /* P95: Track first token timing */
            if (!ctx->first_token_flag) {
                ctx->first_token_flag = true;
                ctx->resp->diag.first_token_time = mono_time();
                ctx->resp->diag.first_token_received = true;
                ctx->resp->diag.time_to_first_token =
                    ctx->resp->diag.first_token_time - ctx->req_start_time;
            }
            ctx->token_count++;
            size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
            size_t add = strlen(delta->content);
            char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
            if (newc) {
                ctx->resp->content = newc;
                memcpy(ctx->resp->content + cur, delta->content, add + 1);
            }
            if (ctx->token_cb) {
                int cb_ret = ctx->token_cb(delta->content, ctx->userdata);
                if (cb_ret != 0) {
                    /* Caller requested abort (e.g. user interrupt) — stop streaming */
                    ctx->prov_ops->free_response(delta);
                    return 1;
                }
            }
        }

        /* B22: finish_reason from provider delta */
        if (delta->finish_reason[0])
            snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason),
                     "%s", delta->finish_reason);

        /* Token counts from delta (Anthropic sends in message_delta) */
        if (delta->input_tokens > 0)
            ctx->resp->input_tokens = delta->input_tokens;
        if (delta->output_tokens > 0)
            ctx->resp->output_tokens = delta->output_tokens;

        /* Accumulate reasoning content across streaming chunks */
        if (delta->reasoning && delta->reasoning[0]) {
            size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
            size_t add = strlen(delta->reasoning);
            char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
            if (newr) {
                ctx->resp->reasoning = newr;
                memcpy(ctx->resp->reasoning + cur, delta->reasoning, add + 1);
            }
        }

        /* Tool calls from provider (Anthropic sends complete tool_use blocks,
         * OpenAI sends deltas — handled below) */
        if (delta->tool_calls_count > 0) {
            for (int i = 0; i < delta->tool_calls_count && i < 64; i++) {
                int idx = ctx->max_tc_idx;
                if (idx < 64) {
                    snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s",
                             delta->tool_calls[i].id);
                    snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s",
                             delta->tool_calls[i].name);
                    snprintf(ctx->tc_args[idx], sizeof(ctx->tc_args[idx]), "%s",
                             delta->tool_calls[i].arguments);
                    ctx->max_tc_idx = idx + 1;
                }
            }
        }

        ctx->prov_ops->free_response(delta);
    }

    /* Fallback: also try OpenAI-style delta parsing for tool calls.
     * This handles the case where parse_stream_chunk returns empty
     * but the chunk has tool_call deltas (OpenAI format).
     * HTTP layer already strips "data: " prefix — handle both cases. */
    const char *json_str = data;
    if (strncmp(data, "data: ", 6) == 0)
        json_str = data + 6;
    if (strncmp(json_str, "[DONE]", 6) == 0) {
            ctx->streaming_done = true;
            return 0;
        }

    char *err = NULL;
    json_node_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 0; }

    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *delta = json_object_get(choice, "delta");

        /* OpenAI tool call deltas */
        if (delta) {
            json_node_t *tc_delta = json_object_get(delta, "tool_calls");
            if (tc_delta && json_array_count(tc_delta) > 0) {
                for (size_t i = 0; i < json_array_count(tc_delta); i++) {
                    json_node_t *tc = json_array_get(tc_delta, i);
                    int idx = (int)json_object_get_number(tc, "index", 0);
                    if (idx >= 64) continue;
                    if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;
                    const char *id = json_object_get_string(tc, "id", NULL);
                    if (id && id[0])
                        snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", NULL);
                        if (name && name[0])
                            snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);
                        const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                        if (args_chunk && args_chunk[0]) {
                            size_t cur = strlen(ctx->tc_args[idx]);
                            size_t add = strlen(args_chunk);
                            if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                                memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                                ctx->tc_args[idx][cur + add] = '\0';
                            }
                        }
                    }
                }
            }

            /* OpenAI finish reason */
            const char *finish = json_object_get_string(choice, "finish_reason", NULL);
            if (finish && finish[0]) {
                ctx->streaming_done = true;
                snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);
                json_node_t *usage = json_object_get(root, "usage");
                if (usage) {
                    ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
                    ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
                }
            }
        }
    }

    json_free(root);
    return 0;
}

static int on_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(data, &err);
    if (!root) { free(err); return 0; } /* Skip non-JSON events */

    json_node_t *choices = json_object_get(root, "choices");
    if (!choices || json_array_count(choices) == 0) { json_free(root); return 0; }

    json_node_t *choice = json_array_get(choices, 0);
    json_node_t *delta = json_object_get(choice, "delta");
    if (!delta) { json_free(root); return 0; }

    /* Extract finish_reason */
    const char *finish = json_object_get_string(choice, "finish_reason", NULL);
    if (finish && finish[0])
        snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);

    /* Extract delta content */
    const char *content = json_object_get_string(delta, "content", NULL);
    if (content && content[0]) {
        /* P95: Track first token timing */
        if (!ctx->first_token_flag) {
            ctx->first_token_flag = true;
            ctx->resp->diag.first_token_time = mono_time();
            ctx->resp->diag.first_token_received = true;
            ctx->resp->diag.time_to_first_token =
                ctx->resp->diag.first_token_time - ctx->req_start_time;
        }
        ctx->token_count++;
        /* Append to accumulated content */
        size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
        size_t add = strlen(content);
        char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
        if (newc) {
            ctx->resp->content = newc;
            memcpy(ctx->resp->content + cur, content, add + 1);
        }
        /* Call token callback */
        if (ctx->token_cb) {
            int cb_ret = ctx->token_cb(content, ctx->userdata);
            if (cb_ret != 0) {
                /* Caller requested abort (e.g. user interrupt) */
                json_free(root);
                return 1;
            }
        }
    }

    /* Extract delta tool_calls (streaming builds them across chunks) */
    json_node_t *tc_delta = json_object_get(delta, "tool_calls");
    if (tc_delta && json_array_count(tc_delta) > 0) {
        for (size_t i = 0; i < json_array_count(tc_delta); i++) {
            json_node_t *tc = json_array_get(tc_delta, i);
            int idx = (int)json_object_get_number(tc, "index", 0);
            if (idx >= 64) continue;

            /* Track highest index */
            if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;

            /* id (only present in first chunk for this tool_call) */
            const char *id = json_object_get_string(tc, "id", NULL);
            if (id && id[0]) snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);

            /* function delta */
            json_node_t *fn = json_object_get(tc, "function");
            if (fn) {
                const char *name = json_object_get_string(fn, "name", NULL);
                if (name && name[0]) snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);

                const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                if (args_chunk && args_chunk[0]) {
                    size_t cur = strlen(ctx->tc_args[idx]);
                    size_t add = strlen(args_chunk);
                    if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                        memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                        ctx->tc_args[idx][cur + add] = '\0';
                    }
                }
            }
        }
    }

    /* Extract reasoning (some providers send it in chunks too) */
    const char *reasoning = json_object_get_string(delta, "reasoning", NULL);
    if (!reasoning) reasoning = json_object_get_string(delta, "reasoning_content", NULL);
    if (reasoning && reasoning[0]) {
        size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
        size_t add = strlen(reasoning);
        char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
        if (newr) {
            ctx->resp->reasoning = newr;
            memcpy(ctx->resp->reasoning + cur, reasoning, add + 1);
        }
    }

    /* L07: xAI encrypted reasoning content in streaming delta */
    if (!ctx->resp->encrypted_content) {
        json_node_t *enc_content = json_object_get(delta, "encrypted_content");
        if (enc_content && json_array_count(enc_content) > 0) {
            char *ser = json_serialize(enc_content);
            if (ser) ctx->resp->encrypted_content = ser;
        }
    }

    /* Extract usage if present (final chunk with finish_reason) */
    if (finish && finish[0]) {
        json_node_t *usage = json_object_get(root, "usage");
        if (usage) {
            ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
            ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
        }
    }

    json_free(root);
    return 0;
}

/* Build tool_calls array from accumulated streaming data */
static void finalize_stream_toolcalls(stream_ctx_t *ctx) {
    ctx->resp->tool_calls_count = ctx->max_tc_idx;
    for (int i = 0; i < ctx->max_tc_idx; i++) {
        snprintf(ctx->resp->tool_calls[i].id, sizeof(ctx->resp->tool_calls[i].id), "%s", ctx->tc_id[i]);
        snprintf(ctx->resp->tool_calls[i].name, sizeof(ctx->resp->tool_calls[i].name), "%s", ctx->tc_name[i]);
        snprintf(ctx->resp->tool_calls[i].arguments, sizeof(ctx->resp->tool_calls[i].arguments), "%s", ctx->tc_args[i]);
    }
}

/* P95: Populate upstream diagnostic headers from HTTP response.
 * Extracts key headers (cf-ray, x-openrouter-*, x-request-id, server)
 * from the raw HTTP response headers string into stream_diag_t. */
static void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers) {
    if (!resp || !raw_headers) return;
    char buf[384] = {0};
    size_t pos = 0;

    /* Header names to capture (lowercase for case-insensitive search) */
    const char *headers_to_capture[] = {
        "cf-ray",
        "x-openrouter-provider",
        "x-openrouter-model",
        "x-openrouter-id",
        "x-request-id",
        "x-vercel-id",
        "server",
        NULL
    };

    for (int i = 0; headers_to_capture[i] && pos < sizeof(buf) - 64; i++) {
        const char *name = headers_to_capture[i];
        size_t nlen = strlen(name);

        /* Case-insensitive search in raw headers */
        const char *h = raw_headers;
        while ((h = strcasestr(h, name)) != NULL) {
            const char *val = h + nlen;
            while (*val == ':' || *val == ' ') val++;
            const char *end = val;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - val);
            if (vlen > 120) vlen = 120;
            if (vlen > 0) {
                size_t remaining = sizeof(buf) - pos;
                int written = snprintf(buf + pos, remaining, "%s=%.*s ",
                                       name, (int)vlen, val);
                if (written > 0) pos += (size_t)written;
            }
            h = end; /* Continue after this match */
            break;    /* Only first occurrence */
        }
    }

    if (pos > 0) {
        /* Remove trailing space */
        if (pos > 0 && buf[pos-1] == ' ') buf[pos-1] = '\0';
        strncpy(resp->diag.upstream_headers, buf, sizeof(resp->diag.upstream_headers) - 1);
        resp->diag.upstream_headers[sizeof(resp->diag.upstream_headers) - 1] = '\0';
    }
}

/* Extract API error context from HTTP response headers/body.
 * Name parity: renamed from extract_rate_limit_reset to match Python.
 *
 * Port of Python agent/agent_runtime_helpers.py:extract_api_error_context().
 * Checks Retry-After and X-RateLimit-Reset headers, then falls back to
 * JSON body fields (retry_after, reset_at, resets_at).
 *
 * Returns Unix timestamp for when the rate limit resets, or 0 if unknown.
 */
static time_t extract_api_error_context(const http_resp_t *resp) {
    if (!resp || !resp->headers) return 0;

    time_t now = time(NULL);
    time_t reset_at = 0;

    /* 1. Retry-After header (RFC 7231) — seconds or HTTP-date */
    const char *h = resp->headers;
    const char *ra = strcasestr(h, "retry-after");
    if (ra) {
        const char *val = ra + 11;  /* len("retry-after") */
        while (*val == ':' || *val == ' ') val++;
        const char *end = val;
        while (*end && *end != '\r' && *end != '\n') end++;
        size_t vlen = (size_t)(end - val);
        if (vlen > 0 && vlen < 64) {
            char valbuf[64];
            memcpy(valbuf, val, vlen);
            valbuf[vlen] = '\0';
            double seconds = http_parse_retry_after(valbuf);
            if (seconds > 0) reset_at = now + (time_t)seconds;
        }
    }

    /* 2. X-RateLimit-Reset header */
    h = resp->headers;
    const char *rlr = strcasestr(h, "x-ratelimit-reset");
    if (rlr && reset_at == 0) {
        const char *val = rlr + 18;  /* len("x-ratelimit-reset") */
        while (*val == ':' || *val == ' ') val++;
        const char *end = val;
        while (*end && *end != '\r' && *end != '\n') end++;
        size_t vlen = (size_t)(end - val);
        if (vlen > 0 && vlen < 64) {
            char valbuf[64];
            memcpy(valbuf, val, vlen);
            valbuf[vlen] = '\0';
            char *ep = NULL;
            double ts = strtod(valbuf, &ep);
            if (ep != valbuf && *ep == '\0' && ts > 0) {
                if (ts > 1000000000.0)  /* Looks like Unix timestamp */
                    reset_at = (time_t)ts;
                else
                    reset_at = now + (time_t)ts;
            }
        }
    }

    /* 3. JSON body fields (retry_after, reset_at, resets_at) */
    if (resp->body && *resp->body && reset_at == 0) {
        json_t *body = json_parse(resp->body, NULL);
        if (body && body->type == JSON_OBJECT) {
            /* retry_after — seconds from now */
            const char *ra_val = json_get_str(body, "retry_after", NULL);
            if (ra_val) {
                char *ep = NULL;
                double seconds = strtod(ra_val, &ep);
                if (ep != ra_val && seconds > 0) reset_at = now + (time_t)seconds;
            }
            /* reset_at — Unix timestamp */
            if (reset_at == 0) {
                const char *rst = json_get_str(body, "reset_at", NULL);
                if (!rst) rst = json_get_str(body, "resets_at", NULL);
                if (rst) {
                    char *ep = NULL;
                    double ts = strtod(rst, &ep);
                    if (ep != rst && ts > 0) reset_at = (time_t)ts;
                }
            }
        }
        json_free(body);
    }

    return reset_at;
}

/* Port of Python agent/agent_runtime_helpers.py:dump_api_request_debug().
 * Dump a debug-friendly HTTP request record for the active inference API.
 * Captures the request body and error context. Intended for debugging
 * provider-side 4xx failures where retries are not useful.
 * Returns true if the dump file was written successfully.
 */
bool dump_api_request_debug(
    const char *request_body,
    const char *session_id,
    const char *reason,
    const char *provider,
    const char *model,
    const char *url,
    int http_status,
    const char *error_body)
{
    if (!getenv("SLERMES_DEBUG"))
        return false;

    char log_path[1024];
    hermes_log_dir(log_path, sizeof(log_path));

    struct stat st = {0};
    if (stat(log_path, &st) == -1) {
        mkdir(log_path, 0755);
    }

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char ts_buf[64];
    strftime(ts_buf, sizeof(ts_buf), "%Y%m%d_%H%M%S", tm_now);

    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/request_dump_%s_%s.json",
             log_path, session_id ? session_id : "unknown", ts_buf);

    json_node_t *root = json_new_object();
    json_object_set(root, "timestamp", json_new_string(ts_buf));
    json_object_set(root, "session_id", json_new_string(session_id ? session_id : ""));
    json_object_set(root, "reason", json_new_string(reason ? reason : ""));
    json_object_set(root, "provider", json_new_string(provider ? provider : ""));
    json_object_set(root, "model", json_new_string(model ? model : ""));
    json_object_set(root, "url", json_new_string(url ? url : ""));

    json_node_t *req = json_new_object();
    json_object_set(req, "method", json_new_string("POST"));
    json_object_set(req, "url", json_new_string(url ? url : ""));
    json_object_set(req, "body", json_new_string(request_body ? request_body : ""));
    json_object_set(root, "request", req);

    if (http_status > 0 || error_body) {
        json_node_t *err = json_new_object();
        if (http_status > 0)
            json_object_set(err, "status_code", json_new_number((double)http_status));
        if (error_body)
            json_object_set(err, "body", json_new_string(error_body));
        json_object_set(root, "error", err);
    }

    char *payload = json_serialize(root);
    json_free(root);

    if (!payload)
        return false;

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        free(payload);
        return false;
    }
    fputs(payload, fp);
    fclose(fp);

    fprintf(stderr, "[llm] debug-request-dump: %s\n", filename);

    if (getenv("SLERMES_DEBUG_DUMP_STDOUT"))
        printf("%s\n", payload);

    free(payload);
    return true;
}

/* P95: Finalize stream diagnostic info */
static void finalize_stream_diag(stream_ctx_t *ctx) {
    if (!ctx->resp) return;
    stream_diag_t *d = &ctx->resp->diag;
    d->stream_end_time = mono_time();
    d->total_tokens = ctx->token_count;
    if (d->first_token_received) {
        d->total_stream_time = d->stream_end_time - d->first_token_time;
        if (d->total_stream_time > 0.001)
            d->tokens_per_second = (double)d->total_tokens / d->total_stream_time;
    }
}

llm_response_t *llm_chat_completion_stream(llm_config_t *cfg,
                                            const message_t **messages,
                                            size_t message_count,
                                            json_node_t *tools_json,
                                            llm_token_cb_t token_cb,
                                            void *userdata) {
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    /* AL07: Responses API mode — use codex_responses provider ops */
    if (prov && cfg->api_mode[0] && strcmp(cfg->api_mode, "codex_responses") == 0) {
        prov->ops = &PROVIDER_OPS_CODEX;
    }

    /* P91: Pass system cache state to provider */
    if (prov) prov->system_cached = cfg->system_cached;

    /* Copy LLM request params from config to provider (streaming path) */
    if (prov) {
        prov->config.max_tokens = cfg->max_tokens;
        prov->config.temperature = cfg->temperature;
        prov->config.top_p = cfg->top_p;
        prov->config.stop_count = cfg->stop_count;
        memcpy(prov->config.stop_sequences, cfg->stop_sequences,
               sizeof(prov->config.stop_sequences));
        prov->config.presence_penalty = cfg->presence_penalty;
        prov->config.frequency_penalty = cfg->frequency_penalty;
        prov->config.seed = cfg->seed;
        prov->config.logprobs = cfg->logprobs;
        prov->config.top_logprobs = cfg->top_logprobs;
        memcpy(prov->config.user, cfg->user, sizeof(prov->config.user));
        memcpy(prov->config.service_tier, cfg->service_tier,
               sizeof(prov->config.service_tier));
        memcpy(prov->config.reasoning_effort, cfg->reasoning_effort,
               sizeof(prov->config.reasoning_effort));
        memcpy(prov->config.response_format, cfg->response_format,
               sizeof(prov->config.response_format));
        memcpy(prov->config.metadata, cfg->metadata,
               sizeof(prov->config.metadata));
        memcpy(prov->config.tool_choice, cfg->tool_choice,
               sizeof(prov->config.tool_choice));
        prov->config.parallel_tool_calls = cfg->parallel_tool_calls;
        prov->config.top_k = cfg->top_k;
        prov->config.candidate_count = cfg->candidate_count;
        prov->config.json_mode = cfg->json_mode;
        prov->config.response_format_strict = cfg->response_format_strict;
        memcpy(prov->config.safety_settings, cfg->safety_settings,
               sizeof(prov->config.safety_settings));
        prov->config.max_tool_calls = cfg->max_tool_calls;
        prov->config.n = cfg->n;
    }

    if (prov && prov->ops) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        /* P158: Verify URL is trusted for this provider's API key */
        const char *effective_key = cfg->api_key;
        if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
            effective_key = "";
        }
        char *headers = ops->build_headers(prov, effective_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, true);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            provider_free(prov); free(resp); return NULL;
        }
        /* Use true streaming via http_stream_request with provider-aware callback */
        stream_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.resp = resp;
        ctx.token_cb = token_cb;
        ctx.userdata = userdata;
        ctx.prov_ops = ops;
        ctx.prov = prov;
        ctx.req_start_time = mono_time(); /* P95: start timing */

        http_t *h = http_new(60);
        int r = http_stream_request(h, HTTP_POST, url,
                                    headers, body, strlen(body),
                                    on_provider_stream_chunk, &ctx);
        free(url); free(headers); free(body);
        /* P95: Capture response headers for stream diagnostics */
        populate_stream_diag_headers(resp, http_get_resp_headers(h));
        http_free(h);

        /* P95: Finalize stream diagnostics */
        finalize_stream_diag(&ctx);

        if (r != 0 && r != -2) {
            /* Log structured stream error with diagnostics */
            const char *upstream = resp->diag.upstream_headers;
            double elapsed = resp->diag.total_stream_time > 0 ?
                resp->diag.total_stream_time : 0.0;
            size_t tokens = resp->diag.total_tokens;
            double ttfb = resp->diag.time_to_first_token;

            if (getenv("SLERMES_DEBUG")) fprintf(stderr, "[llm] Stream failed for %s/%s"
                    " elapsed=%.1fs tokens=%zu ttfb=%.2fs"
                    "%s%s\n",
                    cfg->provider, cfg->model,
                    elapsed, tokens, ttfb,
                    upstream[0] ? " upstream=[" : "",
                    upstream[0] ? upstream : "");
            if (!resp->content) {
                char drop_msg[512];
                snprintf(drop_msg, sizeof(drop_msg),
                         "Stream drop: %s/%s after %.1fs (%zu tokens, TTFB %.2fs)",
                         cfg->provider, cfg->model, elapsed, tokens, ttfb);
                resp->content = strdup(drop_msg);
            }
            provider_free(prov);
            return resp;
        }

        /* Finalize tool calls from accumulated chunks */
        finalize_stream_toolcalls(&ctx);
        provider_free(prov);
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));
    json_object_set(req, "stream", json_new_bool(true));

    /* Stream options: include usage in final chunk */
    json_node_t *stream_opts = json_new_object();
    json_object_set(stream_opts, "include_usage", json_new_bool(true));
    json_object_set(req, "stream_options", stream_opts);

    /* Messages */
    json_node_t *msgs = build_messages_json(messages, message_count);
    json_object_set(req, "messages", msgs);

    /* Add tools if provided */
    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(req, "tools", json_copy(tools_json));

    /* Serialize */
    char *body = json_serialize(req);

    /* Determine URL */
    char url[512];
    const char *base = cfg->base_url;
    if (base && strlen(base) > 0) {
        if (strstr(base, "/chat/completions"))
            snprintf(url, sizeof(url), "%s", base);
        else
            snprintf(url, sizeof(url), "%s/chat/completions", base);
    } else {
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");
    }

    /* Build auth header */
    char auth_header[2048];
    const char *effective_key = cfg->api_key;
    if (effective_key[0] && !provider_url_is_trusted(cfg->provider, url)) {
        effective_key = "";
    }
    if (effective_key[0]) {
        snprintf(auth_header, sizeof(auth_header),
                "Authorization: Bearer %s\r\nContent-TContent-Type: application/json",
                 effective_key);
    } else {
        snprintf(auth_header, sizeof(auth_header),
                 "Content-Type: application/json");
    }

    /* Make streaming request */
    stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.resp = resp;
    ctx.token_cb = token_cb;
    ctx.userdata = userdata;
    ctx.req_start_time = mono_time(); /* P95: start timing */

    /* Make streaming request */
    http_t *h = http_new(60);
    int r = http_stream_request(h, HTTP_POST, url,
                                auth_header, body, strlen(body),
                                on_stream_chunk, &ctx);
    json_free(req);
    free(body);
    /* P95: Capture response headers for stream diagnostics */
    populate_stream_diag_headers(resp, http_get_resp_headers(h));
    http_free(h);

    /* P95: Finalize stream diagnostics */
    finalize_stream_diag(&ctx);

    if (r != 0 && r != -2 /* aborted by callback OK */) {
        double elapsed = resp->diag.total_stream_time > 0 ?
            resp->diag.total_stream_time : 0.0;
        size_t tokens = resp->diag.total_tokens;
        if (getenv("SLERMES_DEBUG")) fprintf(stderr, "[llm] Legacy stream failed after %.1fs (%zu tokens)\n",
                elapsed, tokens);
        char drop_msg[256];
        snprintf(drop_msg, sizeof(drop_msg),
                 "Stream drop after %.1fs (%zu tokens)", elapsed, tokens);
        resp->content = strdup(drop_msg);
        return resp;
    }

    /* Finalize tool calls from accumulated chunks */
    finalize_stream_toolcalls(&ctx);

    return resp;
}

void llm_response_free(llm_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp->encrypted_content);
    free(resp);
}

/* ================================================================
 *  P100: Background review — AI review of tool results
 * ================================================================ */

/* Review prompt template */
#define REVIEW_PROMPT_P100 \
    "Review the following tool execution results. Identify any potential issues, " \
    "suggest improvements, and note any security concerns. " \
    "Be concise. Output only your review:\n\n"

/* ── Background review thread worker ────────────────────────────
 *
 * Port of Python agent/background_review.py:_run_review_in_thread().
 * Port of Python agent/background_review.py:spawn_background_review_thread().
 * Runs a review prompt against the LLM using the provided config.
 * The Python version creates a forked AIAgent; C calls llm_chat_completion
 * directly with the same review prompt structure. Thread management is
 * handled by the conversation loop (agent_loop.c), not here.
 */
char *llm_background_review(llm_config_t *cfg,
                             const char *tool_name,
                             const char *tool_args,
                             const char *tool_result) {
    if (!cfg || !tool_name || !tool_result) return NULL;

    /* Build review text */
    size_t total = strlen(REVIEW_PROMPT_P100) + 128 +
                   strlen(tool_name) + 4 +
                   (tool_args ? strlen(tool_args) : 0) + 1 +
                   strlen(tool_result) + 1;

    char *text = (char *)malloc(total);
    if (!text) return NULL;
    text[0] = '\0';

    strcat(text, REVIEW_PROMPT_P100);
    size_t cur = strlen(text);
    snprintf(text + cur, total - cur,
             "Tool: %s\n"
             "Arguments: %s\n"
             "Result:\n%s\n",
             tool_name, tool_args ? tool_args : "{}", tool_result);

    /* Create review messages */
    message_t *sys = message_new(MSG_SYSTEM,
        "You are a code review expert. Review tool execution results "
        "and identify issues, improvements, and security concerns.");
    message_t *user = message_new(MSG_USER, text);
    const message_t *review_msgs[2] = {sys, user};

    /* Use a fresh llm_config copy with no streaming */
    llm_config_t review_cfg;
    memcpy(&review_cfg, cfg, sizeof(review_cfg));
    review_cfg.base_url[0] = '\0';  /* will use default URL */

    llm_response_t *resp = llm_chat_completion(&review_cfg, review_msgs, 2, NULL);
    char *review = NULL;
    if (resp && resp->content) {
        review = strdup(resp->content);
    }
    llm_response_free(resp);
    message_free(sys);
    message_free(user);
    free(text);
    return review;
}

/* ── Thinking reasoning pad detection ──────────────────────────────────
 *
 * Port of Python run_agent.py AIAgent._needs_thinking_reasoning_pad().
 *
 * Return true when the active provider enforces reasoning_content echo-back.
 * DeepSeek V4 thinking mode, Kimi/Moonshot thinking mode, and Xiaomi MiMo
 * thinking mode all reject replays of assistant messages that omit
 * reasoning_content.
 */
bool needs_thinking_reasoning_pad(const char *provider,
                                   const char *base_url,
                                   const char *model) {
    if (!provider) provider = "";
    if (!base_url) base_url = "";
    if (!model) model = "";

    char provider_lower[128];
    char model_lower[256];
    snprintf(provider_lower, sizeof(provider_lower), "%s", provider);
    snprintf(model_lower, sizeof(model_lower), "%s", model);
    for (int i = 0; provider_lower[i]; i++)
        provider_lower[i] = (char)tolower((unsigned char)provider_lower[i]);
    for (int i = 0; model_lower[i]; i++)
        model_lower[i] = (char)tolower((unsigned char)model_lower[i]);

    /* DeepSeek reasoning pad */
    if (strcmp(provider_lower, "deepseek") == 0 ||
        strstr(model_lower, "deepseek") != NULL ||
        url_host_matches(base_url, "api.deepseek.com"))
        return true;

    /* Kimi reasoning pad */
    if (strcmp(provider_lower, "kimi-coding") == 0 ||
        strcmp(provider_lower, "kimi-coding-cn") == 0 ||
        url_host_matches(base_url, "api.kimi.com"))
        return true;

    /* MiMo reasoning pad */
    if (strcmp(provider_lower, "xiaomi") == 0 ||
        strstr(model_lower, "mimo") != NULL ||
        url_host_matches(base_url, "api.xiaomimimo.com") ||
        url_host_matches(base_url, "xiaomimimo.com"))
        return true;

    return false;
}

/* ── Re-apply reasoning echo for provider ──────────────────────────────
 *
 * Port of Python agent/agent_runtime_helpers.py:reapply_reasoning_echo_for_provider().
 *
 * Re-pads assistant turns with reasoning_content for the active provider.
 * api_messages is a JSON array of message objects.  Calling this before
 * building request kwargs re-applies the pad against the current provider.
 * It is idempotent and a no-op unless needs_thinking_reasoning_pad() is true.
 *
 * Returns the number of assistant turns that gained reasoning_content.
 */
int reapply_reasoning_echo_for_provider(json_node_t *api_messages,
                                         const char *provider,
                                         const char *base_url,
                                         const char *model) {
    if (!api_messages) return 0;
    if (!needs_thinking_reasoning_pad(provider, base_url, model))
        return 0;

    int padded = 0;
    int count = json_array_count(api_messages);
    for (int i = 0; i < count; i++) {
        json_node_t *msg = json_array_get(api_messages, i);
        if (!msg) continue;
        const char *role = json_object_get_string(msg, "role", "");
        if (strcmp(role, "assistant") != 0) continue;

        /* Check if reasoning_content already set and non-empty */
        json_node_t *rc = json_object_get(msg, "reasoning_content");
        if (rc && rc->type == JSON_STRING && rc->str_val[0] == ' ')
            continue; /* already has our space placeholder */
        if (rc && rc->type == JSON_STRING && rc->str_val[0] != '\0')
            continue; /* already has real content */

        /* Inject single space to satisfy provider's requirement */
        json_object_set(msg, "reasoning_content", json_new_string(" "));
        padded++;
    }
    return padded;
}

/* ── Copy reasoning content for API replay ────────────────────────────
 *
 * Port of Python agent/agent_runtime_helpers.py:copy_reasoning_content_for_api().
 *
 * Copy provider-facing reasoning fields from a stored message onto an API
 * replay message. Handles edge cases:
 *   1. Explicit reasoning_content → preserve (empty → " " upgrade for pad providers)
 *   2. Cross-provider poisoned history → inject " "
 *   3. Promote 'reasoning' field to 'reasoning_content'
 *   4. DeepSeek/Kimi/MiMo pad → inject " " on all assistant messages
 *   5. Null/None reasoning_content → remove from API message
 */
void copy_reasoning_content_for_api(json_node_t *source_msg,
                                     json_node_t *api_msg,
                                     const char *provider,
                                     const char *base_url,
                                     const char *model) {
    if (!source_msg || !api_msg) return;

    /* Only operate on assistant messages */
    const char *role = json_object_get_string(source_msg, "role", "");
    if (strcmp(role, "assistant") != 0) return;

    /* Step 1: Explicit reasoning_content already set */
    json_node_t *existing = json_object_get(source_msg, "reasoning_content");
    if (existing && existing->type == JSON_STRING) {
        if (existing->str_val[0] == '\0' &&
            needs_thinking_reasoning_pad(provider, base_url, model)) {
            /* Empty placeholder → upgrade to space for pad providers */
            json_object_set(api_msg, "reasoning_content", json_new_string(" "));
        } else {
            json_object_set(api_msg, "reasoning_content",
                            json_new_string(existing->str_val));
        }
        return;
    }

    bool needs_pad = needs_thinking_reasoning_pad(provider, base_url, model);

    /* Check for reasoning field and tool_calls */
    const char *reasoning_val = json_object_get_string(source_msg, "reasoning", NULL);
    json_node_t *tool_calls = json_object_get(source_msg, "tool_calls");

    /* Step 2: Cross-provider poisoned history */
    if (needs_pad && tool_calls && reasoning_val && reasoning_val[0]) {
        json_object_set(api_msg, "reasoning_content", json_new_string(" "));
        return;
    }

    /* Step 3: Promote reasoning field to reasoning_content */
    if (reasoning_val && reasoning_val[0]) {
        json_object_set(api_msg, "reasoning_content", json_new_string(reasoning_val));
        return;
    }

    /* Step 4: DeepSeek/Kimi/MiMo thinking mode — pad all assistant messages */
    if (needs_pad) {
        json_object_set(api_msg, "reasoning_content", json_new_string(" "));
        return;
    }

    /* Step 5: No reasoning content to copy — leave api_msg unchanged */
}

/* ── Background review action summarizer ─────────────────────────
 *
 * Port of Python agent/background_review.py:summarize_background_review_actions().
 * Walks review_messages (JSON array of tool messages), filters out ones
 * already present in prior_snapshot, and builds a JSON array of action
 * descriptions (e.g. ["Memory updated", "Skill consolidated"]).
 *
 * Both inputs are JSON strings. Returns a malloc'd JSON array string
 * (caller must free) or NULL on error.
 */
char *summarize_background_review_actions(const char *review_messages_json,
                                           const char *prior_snapshot_json) {
    /* Parse review messages */
    char *jerr = NULL;
    json_t *review = review_messages_json ? json_parse(review_messages_json, &jerr) : NULL;
    if (jerr) { free(jerr); jerr = NULL; }

    json_t *prior = prior_snapshot_json ? json_parse(prior_snapshot_json, &jerr) : NULL;
    if (jerr) { free(jerr); jerr = NULL; }

    json_t *result = json_array();
    if (!result) { json_free(review); json_free(prior); return NULL; }

    if (!review || review->type != JSON_ARRAY) {
        char *out = json_serialize(result);
        json_free(result); json_free(review); json_free(prior);
        return out;
    }

    /* Build "existing" sets from prior_snapshot */
    #define MAX_EXISTING 256
    const char *existing_ids[MAX_EXISTING];
    const char *existing_contents[MAX_EXISTING];
    int n_ids = 0, n_cont = 0;

    if (prior && prior->type == JSON_ARRAY) {
        for (size_t i = 0; i < prior->c.count && n_ids < MAX_EXISTING && n_cont < MAX_EXISTING; i++) {
            json_t *msg = prior->c.items[i];
            if (!msg || msg->type != JSON_OBJECT) continue;
            const char *role = json_get_str(msg, "role", "");
            if (strcmp(role, "tool") != 0) continue;
            const char *tcid = json_get_str(msg, "tool_call_id", NULL);
            if (tcid && tcid[0])
                existing_ids[n_ids++] = strdup(tcid);
            else {
                const char *content = json_get_str(msg, "content", NULL);
                if (content && content[0])
                    existing_contents[n_cont++] = strdup(content);
            }
        }
    }

    /* Walk review messages */
    for (size_t i = 0; i < review->c.count; i++) {
        json_t *msg = review->c.items[i];
        if (!msg || msg->type != JSON_OBJECT) continue;
        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "tool") != 0) continue;

        const char *tcid = json_get_str(msg, "tool_call_id", NULL);
        if (tcid && tcid[0]) {
            int found = 0;
            for (int j = 0; j < n_ids; j++)
                if (strcmp(tcid, existing_ids[j]) == 0) { found = 1; break; }
            if (found) continue;
        } else {
            const char *content = json_get_str(msg, "content", NULL);
            if (content && content[0]) {
                int found = 0;
                for (int j = 0; j < n_cont; j++)
                    if (strcmp(content, existing_contents[j]) == 0) { found = 1; break; }
                if (found) continue;
            }
        }

        /* Parse content as action result JSON */
        const char *content_str = json_get_str(msg, "content", "{}");
        json_t *data = json_parse(content_str, &jerr);
        if (jerr) { free(jerr); jerr = NULL; }
        if (!data || data->type != JSON_OBJECT) { json_free(data); continue; }

        bool success = json_get_bool(data, "success", false);
        if (!success) { json_free(data); continue; }

        const char *message = json_get_str(data, "message", "");
        const char *target = json_get_str(data, "target", "");
        char action[512] = "";

        /* Determine action label */
        if (strstr(message, "created") || strstr(message, "Created"))
            snprintf(action, sizeof(action), "%s", message);
        else if (strstr(message, "updated") || strstr(message, "Updated"))
            snprintf(action, sizeof(action), "%s", message);
        else if (strstr(message, "added") || strstr(message, "Added") ||
                 (target[0] && strstr(message, "add"))) {
            if (strcmp(target, "memory") == 0)
                snprintf(action, sizeof(action), "Memory updated");
            else if (strcmp(target, "user") == 0)
                snprintf(action, sizeof(action), "User profile updated");
            else if (target[0])
                snprintf(action, sizeof(action), "%s updated", target);
            else
                snprintf(action, sizeof(action), "%s", message);
        } else if (strstr(message, "Entry added")) {
            if (strcmp(target, "memory") == 0)
                snprintf(action, sizeof(action), "Memory updated");
            else if (strcmp(target, "user") == 0)
                snprintf(action, sizeof(action), "User profile updated");
            else if (target[0])
                snprintf(action, sizeof(action), "%s updated", target);
            else
                snprintf(action, sizeof(action), "Entry added");
        } else if (strstr(message, "removed") || strstr(message, "Removed") ||
                   strstr(message, "replaced") || strstr(message, "Replaced")) {
            if (strcmp(target, "memory") == 0)
                snprintf(action, sizeof(action), "Memory updated");
            else if (strcmp(target, "user") == 0)
                snprintf(action, sizeof(action), "User profile updated");
            else if (target[0])
                snprintf(action, sizeof(action), "%s updated", target);
            else
                snprintf(action, sizeof(action), "%s", message);
        } else if (action[0] == '\0') {
            /* Default: use message as-is if it's non-empty */
            if (message[0])
                snprintf(action, sizeof(action), "%s", message);
        }

        json_free(data);
        if (action[0])
            json_append(result, json_string(action));
    }

    /* Cleanup existing sets */
    for (int i = 0; i < n_ids; i++) free((void*)existing_ids[i]);
    for (int i = 0; i < n_cont; i++) free((void*)existing_contents[i]);

    /* Save and cleanup */
    char *out = json_serialize(result);
    json_free(result);
    json_free(review);
    json_free(prior);
    return out;
}

/* ── Background memory write metadata builder ──────────────────
 *
 * Port of Python agent/background_review.py:build_memory_write_metadata().
 * Builds a JSON string containing provenance metadata for external
 * memory-provider mirrors. Returns malloc'd string (caller must free).
 *
 * All params are optional (NULL = use default):
 *   write_origin: "assistant_tool" (default), "user", "system"
 *   execution_context: "foreground" (default), "background", "review"
 *   session_id: the session identifier
 *   task_id: the current task identifier
 *   tool_call_id: the tool call that triggered the write
 */
char *build_memory_write_metadata(const char *write_origin,
                                   const char *execution_context,
                                   const char *session_id,
                                   const char *task_id,
                                   const char *tool_call_id)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_set(obj, "write_origin",
        json_string(write_origin && write_origin[0] ? write_origin : "assistant_tool"));
    json_set(obj, "execution_context",
        json_string(execution_context && execution_context[0] ? execution_context : "foreground"));
    json_set(obj, "session_id",
        json_string(session_id ? session_id : ""));
    if (task_id && task_id[0])
        json_set(obj, "task_id", json_string(task_id));
    if (tool_call_id && tool_call_id[0])
        json_set(obj, "tool_call_id", json_string(tool_call_id));

    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}
