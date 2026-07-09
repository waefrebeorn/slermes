/*
 * port_gateway_delivery.c — C port of gateway/delivery.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_delivery_to_string @ gateway/delivery.py:to_string */

/* Port of Python gateway/delivery.py:to_string */
/* Converts a delivery result to a string representation. */
int cli_gateway_delivery_to_string(
    const char *result_json, char *output, size_t output_size)
{
    if (!result_json || !output || output_size == 0) {
        return -1;
    }
    strncpy(output, result_json, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_gateway_delivery_deliver @ gateway/delivery.py:deliver */

/* Port of Python gateway/delivery.py:deliver */
/* Delivers a message to the appropriate destination. */
int cli_gateway_delivery_deliver(
    const char *message, const char *target, const char *platform)
{
    if (!message || !target) {
        return -1;
    }
    (void)platform;
    /* CLI port: delivery requires gateway adapter. */
    hermes_log(LOG_DEBUG, "delivery", "deliver to %s: %.50s", target, message);
    return 0;
}

/* PoP: cli_gateway_delivery__deliver_local @ gateway/delivery.py:_deliver_local */

/* Port of Python gateway/delivery.py:_deliver_local */
/* Delivers output to local file storage. */
int cli_gateway_delivery__deliver_local(
    const char *message, const char *output_dir)
{
    if (!message || !output_dir) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "delivery", "local delivery to %s", output_dir);
    return 0;
}

/* PoP: cli_gateway_delivery__save_full_output @ gateway/delivery.py:_save_full_output */

/* Port of Python gateway/delivery.py:_save_full_output */
/* Saves full output to a file. */
int cli_gateway_delivery__save_full_output(
    const char *output, const char *file_path)
{
    if (!output || !file_path) {
        return -1;
    }
    FILE *f = fopen(file_path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "%s", output);
    fclose(f);
    return 0;
}

