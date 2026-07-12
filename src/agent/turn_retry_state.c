/*
 * turn_retry_state.c — Per-attempt recovery bookkeeping for conversation turn loop.
 *
 * Port of Python agent/turn_retry_state.py (69 lines).
 * 0 module-level functions — TurnRetryState dataclass only.
 *
 * N/A: TurnRetryState — Python dataclass (C uses inline retry state in agent_loop.c)
 */

#include "hermes_core_types.h"
