/* Slermes C11 port — agent/relay_runtime.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: relay_instrumentation_enabled @ agent/relay_runtime.py:relay_instrumentation_enabled */
static bool relay_instrumentation_enabled(void) {
    return false;
}

