/*
 * turn_context.c — canonical C11 port of gateway/turn_context.py.
 *
 * TurnContext is the extraction seam between GatewayRunner._run_agent_inner
 * and the TurnRunner collaborator: each closed-over local becomes a field.
 * In C the dataclass becomes an opaque heap struct with constructor +
 * accessors; single-element-list "cells" map to direct fields (C callers
 * share the struct pointer, which gives the same shared-mutation semantics).
 */

#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

typedef struct gw_turn_context {
    /* --- read-only turn identity / wiring --- */
    char  *source;
    int   (*run_still_current)(void);
    char  *live_status_mode;        /* "off" | ... */
    int    thinking_enabled;
    char  *progress_mode;
    char  *progress_grouping;       /* "grouped" */
    int    tool_progress_enabled;

    /* --- mutable cells (single-element-list pattern) --- */
    char  *last_progress_msg;
    char  *last_tool;
    int    last_was_terminal_block;
    long   repeat_count;
    int    long_tool_hint_fired;

    /* --- constants / cleanup bookkeeping --- */
    double long_tool_threshold_s;   /* 30.0 */
    int    cleanup_progress;
    char **cleanup_msg_ids;
    size_t cleanup_msg_ids_n;

    /* --- progress threading metadata --- */
    char  *progress_metadata_json;
    char  *progress_reply_to;

    /* --- the ex-``nonlocal`` turn message (rebindable) --- */
    char  *message;

    /* --- turn parameters / config snapshots (read-only in run_sync) --- */
    char  *context_prompt;
    char  *channel_prompt;
    char  *session_id;
    char  *session_key;
    long   run_generation;
    char  *process_task_id;
    int    interrupt_depth;
    char  *event_message_id;
    char  *moa_config_json;
    double persist_user_timestamp;
    char  *persist_user_display_kind;

    int    log_mode_enabled;
    int    interim_assistant_messages_enabled;
    int    needs_progress_queue;

    /* --- mutable holder cells --- */
    void  *result_holder;
    void  *tools_holder;
    void  *stream_consumer_holder;
    void  *streaming_tts_consumer_holder;

    /* --- voice-ack wiring --- */
    int    voice_ack_fired;
    char  *voice_ack_guild;

    /* --- Slack-native task-card progress (#29483) --- */
    int    native_slack_task_cards;
} gw_turn_context_t;

/* PoP: gwtc_create @ gateway/turn_context.py:TurnContext.__init__ */
gw_turn_context_t *gwtc_create(void) {
    gw_turn_context_t *ctx = calloc(1, sizeof *ctx);
    if (!ctx) return NULL;
    ctx->live_status_mode = strdup("off");
    ctx->progress_mode = strdup("off");
    ctx->progress_grouping = strdup("grouped");
    ctx->long_tool_threshold_s = 30.0;
    return ctx;
}

/* PoP: gwtc_free @ gateway/turn_context.py (dataclass lifetime) */
void gwtc_free(gw_turn_context_t *ctx) {
    if (!ctx) return;
    free(ctx->source); free(ctx->live_status_mode);
    free(ctx->progress_mode); free(ctx->progress_grouping);
    free(ctx->last_progress_msg); free(ctx->last_tool);
    for (size_t i = 0; i < ctx->cleanup_msg_ids_n; i++) free(ctx->cleanup_msg_ids[i]);
    free(ctx->cleanup_msg_ids);
    free(ctx->progress_metadata_json); free(ctx->progress_reply_to);
    free(ctx->message); free(ctx->context_prompt); free(ctx->channel_prompt);
    free(ctx->session_id); free(ctx->session_key); free(ctx->process_task_id);
    free(ctx->event_message_id); free(ctx->moa_config_json);
    free(ctx->persist_user_display_kind);
    free(ctx->voice_ack_guild);
    free(ctx);
}

/*
 * PoP: gwtc_set_message @ gateway/turn_context.py:TurnContext.message
 */
void gwtc_set_message(gw_turn_context_t *ctx, const char *msg) {
    free(ctx->message);
    ctx->message = msg ? strdup(msg) : NULL;
}

/* PoP: gwtc_message @ gateway/turn_context.py:TurnContext.message */
const char *gwtc_message(const gw_turn_context_t *ctx) { return ctx->message; }

/* PoP: gwtc_run_still_current @ gateway/turn_context.py:TurnContext._run_still_current */
int gwtc_run_still_current(const gw_turn_context_t *ctx) {
    return ctx->run_still_current ? ctx->run_still_current() : 0;
}

/* PoP: gwtc_bump_repeat_count @ gateway/turn_context.py:repeat_count cell mutation */
long gwtc_bump_repeat_count(gw_turn_context_t *ctx) {
    return ++ctx->repeat_count;
}

/* PoP: gwtc_long_tool_threshold @ gateway/turn_context.py:_LONG_TOOL_THRESHOLD_S */
double gwtc_long_tool_threshold(const gw_turn_context_t *ctx) {
    (void)ctx;
    return 30.0;
}
