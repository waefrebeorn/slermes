/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_DELEGATE_H
#define SLERMES_DELEGATE_H

#include "hermes_core_types.h"

/* Delegate spawn pause gate */
bool set_spawn_paused(bool paused);
bool is_spawn_paused(void);

#endif /* SLERMES_DELEGATE_H */
