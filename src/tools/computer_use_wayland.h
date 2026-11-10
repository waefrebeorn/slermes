/*
 * computer_use_wayland.h — Wayland computer-use backend (grim + ydotool + wtype).
 *
 * Module-internal header (computer_use.c <-> computer_use_wayland.c).
 */
#ifndef COMPUTER_USE_WAYLAND_H
#define COMPUTER_USE_WAYLAND_H

#include <stdbool.h>
#include "computer_use_util.h"
#include "hermes_computer_use.h"   /* cu_backend_t */

cu_backend_t * make_wayland_backend(void);

#endif /* COMPUTER_USE_WAYLAND_H */
