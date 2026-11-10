/*
 * computer_use_noop.h - Noop computer-use backend (test/CI fallback).
 *
 * Module-internal header (computer_use.c <-> computer_use_noop.c).
 */
#ifndef COMPUTER_USE_NOOP_H
#define COMPUTER_USE_NOOP_H

#include <stdbool.h>
#include "computer_use_util.h"
#include "hermes_computer_use.h"   /* cu_backend_t */

typedef struct {
    int call_count;
} noop_state_t;

cu_backend_t * make_noop_backend(void);

#endif /* COMPUTER_USE_NOOP_H */
