/**
 * @file hermes_gateway_delivery.h
 * @brief Delivery helpers API (port of Python gateway/delivery.py).
 */
#ifndef HERMES_GATEWAY_DELIVERY_H
#define HERMES_GATEWAY_DELIVERY_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  Delivery Helpers
 * ================================================================ */

/* Extract error message from a delivery result JSON string.
 * Returns malloc'd string or NULL. Caller must free. */
char *send_result_error(const char *result_json);

/* Check if delivery error indicates Telegram thread-not-found.
 * Returns true if error matches thread/chat not found patterns. */
bool is_thread_not_found_delivery_error(const char *result_json);

#endif /* HERMES_GATEWAY_DELIVERY_H */