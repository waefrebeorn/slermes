#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/interrupt.py: 3 missing functions ── */

/* PoP: is_thread_interrupted @ tools/interrupt.py:is_thread_interrupted */
char *is_thread_interrupted(void) { return strdup(""); }

/* PoP: run_if_not_interrupted @ tools/interrupt.py:run_if_not_interrupted */
char *run_if_not_interrupted(void) { return strdup(""); }

/* PoP: get_interrupt_reason @ tools/interrupt.py:get_interrupt_reason */
char *get_interrupt_reason(void) { return strdup(""); }
