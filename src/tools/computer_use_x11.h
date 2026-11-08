/*
 * computer_use_x11.h — X11 computer-use backend (xdotool + ImageMagick).
 *
 * Module-internal header (computer_use.c <-> computer_use_x11.c).
 */
#ifndef COMPUTER_USE_X11_H
#define COMPUTER_USE_X11_H

#include <stdbool.h>
#include "computer_use_util.h"
#include "hermes_computer_use.h"   /* cu_backend_t */

cu_backend_t * make_x11_backend(void);

#endif /* COMPUTER_USE_X11_H */
