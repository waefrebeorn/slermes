/*
 * agent_message_repair.c — Message sequence repair for provider compatibility.
 *
 * Ports from Python agent_runtime_helpers:
 *   - repair_message_sequence()
 *   - sanitize_tool_call_arguments()
 *   - sanitize_api_messages() (role filtering integrated into repair_message_sequence)
 *
 * Providers (OpenAI, OpenRouter, Anthropic) expect strict role alternation
 * and valid tool call argument JSON. These functions repair common protocol
 * violations before the API call.
 *
 * NOTE: Works with message_t** (array of pointers), not message_t* (contiguous
 * struct array), matching agent_state_t.messages layout.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include <string.h>
#include <stdio.h>

/* Known tool names that emit their own post-tool hooks.
 * Port of Python agent_runtime_helpers.AGENT_RUNTIME_POST_HOOK_TOOL_NAMES. */
static const char *AGENT_RUNTIME_POST_HOOK_TOOL_NAMES[] = {
    "todo", "session_search", "memory", "clarify", "delegate_task", NULL
};

/* Check if an agent-level tool path emits its own post hook.
 * Port of Python agent_runtime_helpers.agent_runtime_owns_post_tool_hook().
 *
 * Returns true when the tool name matches a known set of tools that fire
 * their own post-tool hooks, so the agent loop can skip the default one.
 * The Python original also checks context_engine_tool_names and the memory
 * manager's has_tool() for dynamically-registered plugin tools. In C,
 * plugins are statically compiled and don't register tools dynamically,
 * so only the static name check is performed (N/A for C architecture).
 * PoP: has_tool @ tools/computer_use/cua_backend.py:_has_tool
 */
bool agent_runtime_owns_post_tool_hook(const char *function_name) {
    if (!function_name || !function_name[0]) return false;
    for (int i = 0; AGENT_RUNTIME_POST_HOOK_TOOL_NAMES[i]; i++) {
        if (strcmp(function_name, AGENT_RUNTIME_POST_HOOK_TOOL_NAMES[i]) == 0)
            return true;
    }
    return false;
}

/* message_deep_copy removed — unused after S13 #1 fix refactor */

/* Drop thinking-only assistant messages from the message array, merging
 * adjacent user messages that result. Port of Python
 * agent_runtime_helpers.drop_thinking_only_and_merge_users().
 *
 * "Thinking-only" = assistant message with reasoning but no visible content
 * and no tool calls. Returns number of messages dropped. Caller should
 * rebuild tool_call_id tracking after calling this. */
/* Port of Python agent/agent_runtime_helpers.py:drop_thinking_only_and_merge_users(). */
int drop_thinking_only_and_merge_users(message_t **messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int n = *count;
    int dropped = 0;

    /* Pass 1: Compact array, dropping thinking-only assistant messages */
    int write_idx = 0;
    for (int i = 0; i < n; i++) {
        message_t *msg = messages[i];
        if (!msg) continue;

        bool thinking_only = false;
        if (msg->role == MSG_ASSISTANT && msg->tool_calls_count == 0) {
            /* Check if content is empty or blank */
            bool content_empty = true;
            if (msg->content) {
                const char *p = msg->content;
                while (*p) {
                    if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                        content_empty = false;
                        break;
                    }
                    p++;
                }
            }
            /* Is there reasoning to make it thinking-only? */
            bool has_reasoning = msg->reasoning && msg->reasoning[0];
            if (content_empty && has_reasoning) {
                thinking_only = true;
            }
        }

        if (thinking_only) {
            /* Free the message and drop it */
            message_free(msg);
            free(msg);
            messages[i] = NULL;
            dropped++;
        } else {
            messages[write_idx++] = msg;
            messages[i] = NULL;
        }
    }

    /* Zero remaining slots */
    for (int i = write_idx; i < n; i++) {
        messages[i] = NULL;
    }
    *count = write_idx;

    if (dropped > 0) {
        /* Pass 2: Merge consecutive user messages that may now be adjacent */
        repair_message_sequence(messages, count);
    }

    return dropped;
}

/* Free a message_t and its owned string pointers. Zeros the struct. */
static void msg_free(message_t *msg) {
    if (!msg) return;
    free(msg->content);
    free(msg->tool_call_id);
    free(msg->tool_name);
    free(msg->reasoning);
    free(msg->encrypted_content);
    memset(msg, 0, sizeof(*msg));
}

/* Allocate a new heap message with given role, content, tool_call_id, tool_name.
 * Returns NULL on malloc failure. Caller owns the pointer. */
static message_t *msg_new(message_role_t role, const char *content,
                           const char *tool_call_id, const char *tool_name) {
    message_t *msg = calloc(1, sizeof(message_t));
    if (!msg) return NULL;
    msg->role = role;
    if (content) msg->content = strdup(content);
    if (tool_call_id) msg->tool_call_id = strdup(tool_call_id);
    if (tool_name) msg->tool_name = strdup(tool_name);
    return msg;
}

/* Stub content injected for tool results that went missing (e.g. compressed away) */
#define TOOL_RESULT_STUB_MARKER \
    "[Result unavailable — see context summary above]"

/* Repair message sequence in place for provider compatibility.
 *
 * Takes message_t** (array of pointers to heap-allocated messages).
 * Updates *count on return.
 *
 * Pass 1: Drops stray tool messages whose tool_call_id doesn't match
 *         any preceding assistant message's tool calls.
 * Pass 2: Merges consecutive user messages with "\n\n" separator.
 * Pass 3: Injects stub tool results for assistant tool calls that have
 *         no corresponding tool result (port of sanitize_api_messages).
 *
 * Returns number of repairs made.
 */
/* Port of Python agent/agent_runtime_helpers.py:repair_message_sequence(). */
int repair_message_sequence(message_t **messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int repairs = 0;
    int n = *count;

    /* Pass 1: Build filtered pointer array */
    char known_ids[64][64];
    int num_known = 0;

    message_t **filtered = calloc((size_t)n, sizeof(message_t *));
    if (!filtered) return 0;
    int filtered_count = 0;

    for (int i = 0; i < n; i++) {
        message_t *msg = messages[i];
        if (!msg) continue;

        /* Role allowlist: drop messages with roles the API won't accept.
         * Port of Python agent_runtime_helpers.sanitize_api_messages role filter. */
        if (msg->role != MSG_SYSTEM && msg->role != MSG_USER &&
            msg->role != MSG_ASSISTANT && msg->role != MSG_TOOL) {
            repairs++;
            msg_free(msg);
            free(msg);
            messages[i] = NULL;
            continue;
        }

        if (msg->role == MSG_ASSISTANT) {
            num_known = 0;
            for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
                if (msg->tool_calls[j].id[0]) {
                    snprintf(known_ids[num_known], sizeof(known_ids[0]),
                             "%s", msg->tool_calls[j].id);
                    num_known++;
                }
            }
            filtered[filtered_count++] = msg;
            messages[i] = NULL; /* ownership transferred */
        } else if (msg->role == MSG_TOOL) {
            bool found = false;
            if (msg->tool_call_id && msg->tool_call_id[0]) {
                for (int j = 0; j < num_known; j++) {
                    if (strcmp(msg->tool_call_id, known_ids[j]) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                filtered[filtered_count++] = msg;
                messages[i] = NULL;
            } else {
                repairs++;
                msg_free(msg);
                free(msg);
                messages[i] = NULL;
            }
        } else {
            if (msg->role == MSG_USER) {
                num_known = 0;
            }
            filtered[filtered_count++] = msg;
            messages[i] = NULL;
        }
    }

    /* Pass 2: Merge consecutive user messages */
    message_t **merged = calloc((size_t)filtered_count, sizeof(message_t *));
    if (!merged) { free(filtered); return 0; }
    int merged_count = 0;

    for (int i = 0; i < filtered_count; i++) {
        message_t *msg = filtered[i];
        if (!msg) continue;

        if (merged_count > 0 &&
            msg->role == MSG_USER &&
            merged[merged_count - 1]->role == MSG_USER) {
            /* Merge with previous user message */
            message_t *prev = merged[merged_count - 1];
            if (prev->content && msg->content) {
                size_t new_len = strlen(prev->content) + 3 + strlen(msg->content) + 1;
                char *new_content = malloc(new_len);
                if (new_content) {
                    snprintf(new_content, new_len, "%s\n\n%s",
                             prev->content, msg->content);
                    free(prev->content);
                    prev->content = new_content;
                }
            } else if (msg->content) {
                prev->content = strdup(msg->content);
            }
            msg_free(msg);
            free(msg);
            repairs++;
        } else {
            merged[merged_count++] = msg;
        }
    }

    /* Write merged results back into messages array */
    for (int i = 0; i < merged_count; i++) {
        messages[i] = merged[i];
    }

    /* Pass 3: Ingest stub tool results for assistant tool calls with no
     *         corresponding tool result (port of sanitize_api_messages).
     *
     * Walk the merged array, collecting surviving tool_call_ids, then
     * inject a stub MSG_TOOL for each call_id without a result. */
    {
        /* Build surviving tool_call_ids from assistant messages */
        int num_call_ids = 0;
        for (int i = 0; i < merged_count && num_call_ids < 64; i++) {
            if (messages[i] && messages[i]->role == MSG_ASSISTANT) {
                for (int j = 0; j < messages[i]->tool_calls_count && num_call_ids < 64; j++) {
                    if (messages[i]->tool_calls[j].id[0]) {
                        /* Check not already added */
                        bool dup = false;
                        for (int k = 0; k < num_call_ids; k++) {
                            if (strcmp(known_ids[k], messages[i]->tool_calls[j].id) == 0) {
                                dup = true; break;
                            }
                        }
                        if (!dup) {
                            snprintf(known_ids[num_call_ids], sizeof(known_ids[0]),
                                     "%s", messages[i]->tool_calls[j].id);
                            num_call_ids++;
                        }
                    }
                }
            }
        }

        /* Check which tool_call_ids have existing results */
        bool has_result[64];
        for (int j = 0; j < num_call_ids; j++) has_result[j] = false;
        for (int i = 0; i < merged_count; i++) {
            if (messages[i] && messages[i]->role == MSG_TOOL &&
                messages[i]->tool_call_id && messages[i]->tool_call_id[0]) {
                for (int j = 0; j < num_call_ids; j++) {
                    if (strcmp(messages[i]->tool_call_id, known_ids[j]) == 0) {
                        has_result[j] = true;
                        break;
                    }
                }
            }
        }

        /* Count how many stubs we need */
        int stubs_needed = 0;
        for (int j = 0; j < num_call_ids; j++) {
            if (!has_result[j]) stubs_needed++;
        }

        if (stubs_needed > 0) {
            int new_count = merged_count + stubs_needed;
            if (new_count > HERMES_MAX_MESSAGES)
                new_count = HERMES_MAX_MESSAGES;
            stubs_needed = new_count - merged_count;

            for (int j = 0, stub_idx = 0; j < num_call_ids && stub_idx < stubs_needed; j++) {
                if (!has_result[j]) {
                    message_t *stub = msg_new(MSG_TOOL, TOOL_RESULT_STUB_MARKER,
                                               known_ids[j], NULL);
                    if (stub) {
                        messages[merged_count + stub_idx] = stub;
                        stub_idx++;
                        repairs++;
                    }
                }
            }
            merged_count += stubs_needed;
        }
    }

    /* Zero remaining slots that were freed */
    for (int i = merged_count; i < n; i++) {
        messages[i] = NULL;
    }
    *count = merged_count;

    free(filtered);
    free(merged);
    return repairs;
}

/* Sanitize API messages for provider compatibility.
 *
 * Takes message_t** (array of pointers to heap-allocated messages).
 * Updates *count on return.
 *
 * Pass 1: Role allowlist — drops messages with roles the API won't accept
 *         (only SYSTEM, USER, ASSISTANT, TOOL are kept).
 * Pass 2: Drops stray tool messages whose tool_call_id doesn't match
 *         any preceding assistant message's tool calls.
 * Pass 3: Injects stub tool results for assistant tool calls that have
 *         no corresponding tool result.
 *
 * Unlike repair_message_sequence(), this does NOT merge consecutive
 * user messages. It only performs the sanitization steps required by
 * the API (role filtering, orphan tool result removal, stub injection).
 *
 * Returns number of repairs made.
 */
/* Port of Python agent/agent_runtime_helpers.py:sanitize_api_messages(). */
int sanitize_api_messages(message_t **messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int repairs = 0;
    int n = *count;

    /* Pass 1: Build filtered pointer array */
    char known_ids[64][64];
    int num_known = 0;

    message_t **filtered = calloc((size_t)n, sizeof(message_t *));
    if (!filtered) return 0;
    int filtered_count = 0;

    for (int i = 0; i < n; i++) {
        message_t *msg = messages[i];
        if (!msg) continue;

        /* Role allowlist: drop messages with roles the API won't accept. */
        if (msg->role != MSG_SYSTEM && msg->role != MSG_USER &&
            msg->role != MSG_ASSISTANT && msg->role != MSG_TOOL) {
            repairs++;
            msg_free(msg);
            free(msg);
            messages[i] = NULL;
            continue;
        }

        if (msg->role == MSG_ASSISTANT) {
            num_known = 0;
            for (int j = 0; j < msg->tool_calls_count && j < 64; j++) {
                if (msg->tool_calls[j].id[0]) {
                    snprintf(known_ids[num_known], sizeof(known_ids[0]),
                             "%s", msg->tool_calls[j].id);
                    num_known++;
                }
            }
            filtered[filtered_count++] = msg;
            messages[i] = NULL; /* ownership transferred */
        } else if (msg->role == MSG_TOOL) {
            bool found = false;
            if (msg->tool_call_id && msg->tool_call_id[0]) {
                for (int j = 0; j < num_known; j++) {
                    if (strcmp(msg->tool_call_id, known_ids[j]) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                filtered[filtered_count++] = msg;
                messages[i] = NULL;
            } else {
                repairs++;
                msg_free(msg);
                free(msg);
                messages[i] = NULL;
            }
        } else {
            if (msg->role == MSG_USER) {
                num_known = 0;
            }
            filtered[filtered_count++] = msg;
            messages[i] = NULL;
        }
    }

    /* Pass 2: Inject stub tool results for assistant tool calls with no
     *         corresponding tool result. */
    {
        /* Build surviving tool_call_ids from assistant messages */
        int num_call_ids = 0;
        for (int i = 0; i < filtered_count && num_call_ids < 64; i++) {
            if (filtered[i] && filtered[i]->role == MSG_ASSISTANT) {
                for (int j = 0; j < filtered[i]->tool_calls_count && num_call_ids < 64; j++) {
                    if (filtered[i]->tool_calls[j].id[0]) {
                        /* Check not already added */
                        bool dup = false;
                        for (int k = 0; k < num_call_ids; k++) {
                            if (strcmp(known_ids[k], filtered[i]->tool_calls[j].id) == 0) {
                                dup = true; break;
                            }
                        }
                        if (!dup) {
                            snprintf(known_ids[num_call_ids], sizeof(known_ids[0]),
                                     "%s", filtered[i]->tool_calls[j].id);
                            num_call_ids++;
                        }
                    }
                }
            }
        }

        /* Check which tool_call_ids have existing results */
        bool has_result[64];
        for (int j = 0; j < num_call_ids; j++) has_result[j] = false;
        for (int i = 0; i < filtered_count; i++) {
            if (filtered[i] && filtered[i]->role == MSG_TOOL &&
                filtered[i]->tool_call_id && filtered[i]->tool_call_id[0]) {
                for (int j = 0; j < num_call_ids; j++) {
                    if (strcmp(filtered[i]->tool_call_id, known_ids[j]) == 0) {
                        has_result[j] = true;
                        break;
                    }
                }
            }
        }

        /* Count how many stubs we need */
        int stubs_needed = 0;
        for (int j = 0; j < num_call_ids; j++) {
            if (!has_result[j]) stubs_needed++;
        }

        if (stubs_needed > 0) {
            int new_count = filtered_count + stubs_needed;
            if (new_count > HERMES_MAX_MESSAGES)
                new_count = HERMES_MAX_MESSAGES;
            stubs_needed = new_count - filtered_count;

            for (int j = 0, stub_idx = 0; j < num_call_ids && stub_idx < stubs_needed; j++) {
                if (!has_result[j]) {
                    message_t *stub = msg_new(MSG_TOOL, TOOL_RESULT_STUB_MARKER,
                                               known_ids[j], NULL);
                    if (stub) {
                        filtered[filtered_count + stub_idx] = stub;
                        stub_idx++;
                        repairs++;
                    }
                }
            }
            filtered_count += stubs_needed;
        }
    }

    /* Write filtered results back into messages array */
    for (int i = 0; i < filtered_count; i++) {
        messages[i] = filtered[i];
    }

    /* Zero remaining slots that were freed */
    for (int i = filtered_count; i < n; i++) {
        messages[i] = NULL;
    }
    *count = filtered_count;

    free(filtered);
    return repairs;
}

/* Run repair_message_sequence and keep the SessionDB flush cursor consistent.
 * Port of Python agent/agent_runtime_helpers.py:repair_message_sequence_with_cursor().
 *
 * The agent_state_t._last_flushed_db_idx indexes into the messages array.
 * After compaction, this cursor can point past the new end. We track which
 * message pointers from the flushed prefix survive, and update the cursor
 * to count survivors.
 *
 * Returns the number of repairs made (same as repair_message_sequence).
 */
int repair_message_sequence_with_cursor(message_t **messages, int *count,
                                         int *last_flushed_db_idx) {
    if (!messages || !count || *count <= 0) return 0;

    int n = *count;
    int cursor = 0;
    if (last_flushed_db_idx && *last_flushed_db_idx > 0) {
        cursor = *last_flushed_db_idx;
        if (cursor > n) cursor = n;
    }

    /* Snapshot the message pointers in the flushed prefix */
    message_t **flushed_ptrs = NULL;
    int flushed_count = 0;
    if (cursor > 0) {
        flushed_ptrs = calloc((size_t)cursor, sizeof(message_t *));
        if (!flushed_ptrs) {
            /* Fallback: just clamp cursor on return */
            flushed_ptrs = NULL;
        } else {
            for (int i = 0; i < cursor; i++) {
                if (messages[i]) {
                    flushed_ptrs[flushed_count++] = messages[i];
                }
            }
        }
    }

    int repairs = repair_message_sequence(messages, count);

    if (repairs > 0 && last_flushed_db_idx && flushed_count > 0) {
        int new_cursor = 0;
        for (int i = 0; i < *count; i++) {
            if (messages[i]) {
                for (int j = 0; j < flushed_count; j++) {
                    if (messages[i] == flushed_ptrs[j]) {
                        new_cursor++;
                        break;
                    }
                }
            }
        }
        *last_flushed_db_idx = new_cursor;
    } else if (repairs > 0 && last_flushed_db_idx) {
        /* No prefix snapshot available — fall back to clamping */
        if (*last_flushed_db_idx > *count) {
            *last_flushed_db_idx = *count;
        }
    }

    if (flushed_ptrs) free(flushed_ptrs);
    return repairs;
}

/* Corruption marker injected into tool results when arguments were corrupted */
#define TOOL_CALL_ARGS_CORRUPTION_MARKER \
    "[hermes-agent: tool call arguments were corrupted in this session and " \
    "have been dropped to keep the conversation alive. See issue #15236.]"

/* Sanitize tool call arguments in place.
 *
 * NOTE: Still uses message_t* (not **). Only called from places that
 * provide a contiguous struct array. If callers switch to pointer arrays,
 * this must be updated similarly.
 *
 * Two-pass approach:
 *   Pass 1: Walk messages. For each assistant with tool_calls, check each
 *           tool_call's arguments. NULL/empty/corrupted → replace with "{}".
 *           Record corrupted tool_call_ids for marker prepending.
 *   Pass 2: Walk messages again. For each tool result with a recorded
 *           tool_call_id, prepend the corruption marker to its content.
 *           If no matching tool result exists, insert a new one.
 *
 * Returns number of repairs made. May insert new messages (updates *count).
 */
/* Port of Python agent/agent_runtime_helpers.py:sanitize_tool_call_arguments(). */
int sanitize_tool_call_arguments(message_t *messages, int *count) {
    if (!messages || !count || *count <= 0) return 0;

    int repairs = 0;
    int n = *count;
    int max_msgs = HERMES_MAX_MESSAGES;
    if (max_msgs > 8192) max_msgs = 8192;

    /* Track tool_call_ids that need corruption markers (up to 64) */
    char needs_marker[64][64];
    int num_needs_marker = 0;

    /* Also track which tool_call_ids already have existing results */
    bool has_existing_result[64];
    for (int i = 0; i < 64; i++) has_existing_result[i] = false;

    /* === Pass 1: Find corruptions and fix arguments === */
    for (int i = 0; i < n; i++) {
        message_t *msg = &messages[i];
        if (msg->role == MSG_ASSISTANT && msg->tool_calls_count > 0) {
            int tc_count = msg->tool_calls_count;
            if (tc_count > 64) tc_count = 64;

            for (int j = 0; j < tc_count; j++) {
                tool_call_t *tc = &msg->tool_calls[j];
                bool corrupted = false;

                if (!tc->arguments[0]) {
                    snprintf(tc->arguments, sizeof(tc->arguments), "{}");
                    corrupted = true;
                } else {
                    const char *a = tc->arguments;
                    bool valid_json = false;
                    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r') a++;
                    if (*a == '{' || *a == '[') valid_json = true;
                    if (!valid_json) {
                        snprintf(tc->arguments, sizeof(tc->arguments), "{}");
                        corrupted = true;
                    }
                }

                if (corrupted) {
                    repairs++;
                    if (num_needs_marker < 64) {
                        snprintf(needs_marker[num_needs_marker],
                                 sizeof(needs_marker[0]), "%s", tc->id);
                        num_needs_marker++;

                        /* Scan ahead to check if tool result exists */
                        for (int k = i + 1; k < n; k++) {
                            if (messages[k].role == MSG_TOOL &&
                                messages[k].tool_call_id &&
                                messages[k].tool_call_id[0] &&
                                strcmp(messages[k].tool_call_id, tc->id) == 0) {
                                has_existing_result[num_needs_marker - 1] = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (repairs == 0) return 0;

    /* === Pass 2: Build filtered list with markers and insertions === */
    message_t *filtered = calloc((size_t)n + (size_t)num_needs_marker + 16, sizeof(message_t));
    if (!filtered) return 0;
    int filtered_count = 0;

    for (int i = 0; i < n; i++) {
        message_t *msg = &messages[i];

        if (msg->role == MSG_TOOL && msg->tool_call_id && msg->tool_call_id[0]) {
            /* Check if this tool result needs a corruption marker */
            int marker_idx = -1;
            for (int j = 0; j < num_needs_marker; j++) {
                if (strcmp(msg->tool_call_id, needs_marker[j]) == 0) {
                    marker_idx = j;
                    break;
                }
            }
            if (marker_idx >= 0) {
                /* Prepend marker to content */
                size_t marker_len = strlen(TOOL_CALL_ARGS_CORRUPTION_MARKER);
                size_t content_len = msg->content ? strlen(msg->content) : 0;
                char *new_content = malloc(marker_len + content_len + 3);
                if (new_content) {
                    if (msg->content) {
                        snprintf(new_content, marker_len + content_len + 3,
                                 "%s\n%s", TOOL_CALL_ARGS_CORRUPTION_MARKER, msg->content);
                    } else {
                        snprintf(new_content, marker_len + 1, "%s",
                                 TOOL_CALL_ARGS_CORRUPTION_MARKER);
                    }
                    free(msg->content);
                    msg->content = new_content;
                }
                filtered[filtered_count++] = *msg;
                memset(msg, 0, sizeof(*msg));
                /* Mark as handled so we don't insert a duplicate */
                has_existing_result[marker_idx] = true;
            } else {
                filtered[filtered_count++] = *msg;
                memset(msg, 0, sizeof(*msg));
            }
        } else if (msg->role == MSG_ASSISTANT && msg->tool_calls_count > 0) {
            filtered[filtered_count++] = *msg;
            memset(msg, 0, sizeof(*msg));
        } else {
            filtered[filtered_count++] = *msg;
            memset(msg, 0, sizeof(*msg));
        }
    }

    /* Insert new tool results for any corrupted calls that had no existing result */
    for (int j = 0; j < num_needs_marker; j++) {
        if (!has_existing_result[j] && filtered_count < max_msgs) {
            message_t *tr = &filtered[filtered_count++];
            memset(tr, 0, sizeof(*tr));
            tr->role = MSG_TOOL;
            tr->content = strdup(TOOL_CALL_ARGS_CORRUPTION_MARKER);
            tr->tool_call_id = strdup(needs_marker[j]);
        }
    }

    /* Copy filtered results back, zeroing beyond new count.
     * Write ALL filtered entries — caller must provide enough capacity. */
    int write_count = filtered_count;
    int old_count = *count;
    for (int i = 0; i < write_count; i++) {
        messages[i] = filtered[i];
    }
    for (int i = write_count; i < old_count; i++) {
        memset(&messages[i], 0, sizeof(messages[i]));
    }
    *count = write_count;

    free(filtered);
    return repairs;
}

/* Append pending /steer text to the last tool result message in the recent tail.
 * Port of Python agent_runtime_helpers.apply_pending_steer_to_tool_results().
 *
 * Called at the end of a tool-call batch, before the next API call.
 * The steer is appended to the last MSG_TOOL message's content with a
 * clear marker so the model understands it came from the user and NOT
 * from the tool itself. Role alternation is preserved — nothing new is
 * inserted, we only modify existing content.
 *
 * Returns the number of characters appended (0 if nothing was done).
 * If no MSG_TOOL found in the tail, the steer is written back to
 * pending_steer so it can be delivered as a normal user message next turn.
 */
int apply_pending_steer_to_tool_results(message_t **messages, int *count,
                                         int num_tool_msgs, char *pending_steer) {
    if (!messages || !count || *count <= 0 || num_tool_msgs <= 0 || !pending_steer)
        return 0;

    /* Drain the pending steer */
    size_t steer_len = strlen(pending_steer);
    if (steer_len == 0) return 0;

    char *steer = strdup(pending_steer);
    if (!steer) return 0;
    pending_steer[0] = '\0';

    /* Find the last MSG_TOOL message in the recent tail.
     * Search backward within the last num_tool_msgs messages (defense
     * against non-tool messages at the boundary). */
    int target_idx = -1;
    int start = *count - 1;
    int end = *count - num_tool_msgs - 1;
    if (end < -1) end = -1;

    for (int i = start; i > end; i--) {
        if (messages[i] && messages[i]->role == MSG_TOOL) {
            target_idx = i;
            break;
        }
    }

    if (target_idx < 0) {
        /* No tool result in this batch — put the steer back so the
         * caller's fallback path can deliver it next turn. */
        size_t existing = strlen(pending_steer);
        if (existing + steer_len + 2 <= 4096) {
            if (existing > 0) {
                strncat(pending_steer, "\n", 4096 - existing - 1);
            }
            strncat(pending_steer, steer, 4096 - strlen(pending_steer) - 1);
        }
        free(steer);
        return 0;
    }

    /* Append steer to the tool message's content */
    const char *marker = "\n\nUser guidance: ";
    size_t marker_len = strlen(marker);
    message_t *target = messages[target_idx];
    size_t old_len = target->content ? strlen(target->content) : 0;

    char *new_content = malloc(old_len + marker_len + steer_len + 1);
    if (!new_content) {
        /* Re-queue the steer on allocation failure */
        size_t existing = strlen(pending_steer);
        if (existing + steer_len + 2 <= 4096) {
            if (existing > 0) {
                strncat(pending_steer, "\n", 4096 - existing - 1);
            }
            strncat(pending_steer, steer, 4096 - strlen(pending_steer) - 1);
        }
        free(steer);
        return 0;
    }

    if (target->content) {
        snprintf(new_content, old_len + marker_len + steer_len + 1,
                 "%s%s%s", target->content, marker, steer);
    } else {
        snprintf(new_content, marker_len + steer_len + 1,
                 "%s%s", marker + 2, steer); /* skip leading \n\n if no prior content */
    }

    free(target->content);
    target->content = new_content;
    free(steer);

    size_t appended = marker_len + steer_len;
    return (int)appended;
}
