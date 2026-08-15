/* Slermes C11 port — agent/image_gen_provider.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: capabilities @ agent/image_gen_provider.py:capabilities */
static json_t *capabilities(void) {
    return json_object();
}

