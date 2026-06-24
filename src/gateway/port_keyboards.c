/**
 * port_keyboards.c — Port of Python: gateway/keyboards.py
 *
 * Real C implementations for keyboard/button parsing.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: parse_approval_button_data */
const char *parse_approval_button_data(json_t *button_data)
{
    if (!button_data) {
        hermes_log(LOG_WARNING, "port", "parse_approval_button_data: null button_data");
        return "";
    }
    const char *action = json_node_get_string(json_object_get(button_data, "action"));
    const char *label = json_node_get_string(json_object_get(button_data, "label"));
    hermes_log(LOG_DEBUG, "port", "parse_approval_button_data: action=%s label=%s",
               action ? action : "(null)", label ? label : "(null)");
    if (action) return action;
    return label ? label : "";
}
