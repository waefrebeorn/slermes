/*
 * computer_use_resp.h - computer-use JSON response builders.
 *
 * Module-internal header (computer_use.c <-> computer_use_resp.c).
 */
#ifndef COMPUTER_USE_RESP_H
#define COMPUTER_USE_RESP_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_computer_use.h"

char * make_capture_response(const cu_capture_t *cap, int max_elements);
char * make_error_response(const char *fmt, ...);
char * make_text_response(const cu_action_t *act);

#endif /* COMPUTER_USE_RESP_H */
