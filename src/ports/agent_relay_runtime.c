/* Slermes C11 port — agent/relay_runtime.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: __enter__ @ agent/relay_runtime.py:__enter__ */
json_t *__enter__(void) {
    return json_object();
}

/* PoP: __exit__ @ agent/relay_runtime.py:__exit__ */
void __exit__(json_t * self, json_t * *exc_info) {
}

