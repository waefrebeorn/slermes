/*
 * delegate.h — Slermes C11 port of tools/delegate.py public API.
 *
 * Public surface consumed by the command dispatcher (commands.c,
 * cli_cmd_session.c). Faithful extraction from the god header so callers
 * no longer include hermes.h transitively.
 */

#ifndef DELEGATE_H
#define DELEGATE_H

#include "hermes_json.h"   /* json_node_t */

#ifdef __cplusplus
extern "C" {
#endif

/* List active delegated subagents into the provided result node. */
void delegate_list(json_node_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DELEGATE_H */
