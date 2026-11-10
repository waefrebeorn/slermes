/*
 * omarchy_usage.h -- native omarchy agents-panel usage record for Slermes.
 *
 * omarchy_usage_scan() reads the Slermes/Hermes state.db (home + profiles) and
 * fills `out` (up to `cap` bytes) with the omarchy Hermes-usage JSON record:
 * ready "hermes", the full stats block, empty limits.  Returns 0 on success
 * (-1 if `out`/`cap` is unusable).  Emits "{}" when the buffers are bad.
 */
#ifndef OMARCHY_USAGE_H
#define OMARCHY_USAGE_H

#include <stddef.h>

int omarchy_usage_scan(const char *home, char *out, size_t cap);

#endif /* OMARCHY_USAGE_H */
