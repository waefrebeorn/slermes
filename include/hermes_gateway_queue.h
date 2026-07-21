/**
 * @file hermes_gateway_queue.h
 * @brief Gateway message queue API (P101).
 */
#ifndef HERMES_GATEWAY_QUEUE_H
#define HERMES_GATEWAY_QUEUE_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  P101: Gateway queue API
 * ================================================================ */

/* Initialize message queue */
void gw_queue_init(void);

/* Push a message onto the queue. Blocks if full (up to 1s). Returns true on success. */
bool gw_queue_push(const char *platform, const char *chat_id,
                    const char *text, const char *thread_id);

/* Pop next message from queue. Non-blocking — returns false if empty. */
bool gw_queue_pop(gateway_msg_t *msg);

/* Get current queue depth */
int gw_queue_depth(void);
void gw_queue_drain_all(void);

#endif /* HERMES_GATEWAY_QUEUE_H */