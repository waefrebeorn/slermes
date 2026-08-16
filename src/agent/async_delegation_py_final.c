/* Slermes C port — tools/async_delegation.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_async_delegation_reset_for_tests @ tools/async_delegation.py:_reset_for_tests */
json_t *tools_async_delegation_reset_for_tests(void) {
    return json_object();
}
