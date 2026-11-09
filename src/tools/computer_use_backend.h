/*
 * computer_use_backend.h - computer-use backend helpers.
 *
 * Module-internal header (computer_use.c <-> computer_use_backend.c).
 * NOTE: computer_use_util.h is a DIFFERENT, older module (from 22b361b)
 * holding _has_cmd() for the per-platform backends.
 */
#ifndef COMPUTER_USE_BACKEND_H
#define COMPUTER_USE_BACKEND_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_computer_use.h"   /* cu_backend_t, cu_action_t, cu_capture_t */

void cu_action_free(cu_action_t *act);
void cu_capture_free(cu_capture_t *cap);
bool is_blocked_key_combo(const char *keys);
bool is_blocked_type(const char *text);

#endif /* COMPUTER_USE_BACKEND_H */
