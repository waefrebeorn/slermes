/*
 * computer_use_windows.h - Win32 computer-use backend (user32 + gdi32).
 *
 * Module-internal header (computer_use.c <-> computer_use_windows.c).
 */
#ifndef COMPUTER_USE_WINDOWS_H
#define COMPUTER_USE_WINDOWS_H

#include <stdbool.h>
#include "computer_use_util.h"
#include "hermes_computer_use.h"   /* cu_backend_t */

cu_backend_t * make_windows_backend(void);

#endif /* COMPUTER_USE_WINDOWS_H */
