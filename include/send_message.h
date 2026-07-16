/*
 * send_message.h — Slermes C11 port of tools/send_message.py handler API.
 *
 * Public handler surface (the rest of the agent/cli calls into this from
 * the command dispatcher). Faithful extraction from the god header so
 * callers no longer include hermes.h transitively.
 */

#ifndef SEND_MESSAGE_H
#define SEND_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Dispatch a send-message request. args_json is the tool-call arguments,
 * task_id the originating task (may be NULL). Returns a heap JSON string
 * (caller frees) or an error object. */
char *send_message_handler(const char *args_json, const char *task_id);

#ifdef __cplusplus
}
#endif

#endif /* SEND_MESSAGE_H */
