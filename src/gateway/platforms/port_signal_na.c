/*
 * port_signal_na.c — Port of Python gateway/platforms/signal.py (NA_SDK functions)
 * Functions that don't exist in any other port file.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: _remux_aac_to_m4a */
bool _remux_aac_to_m4a(const char* input_path, const char* output_path)
{
    if (!input_path || !output_path) return false;
    hermes_log(LOG_DEBUG, "port", "_remux_aac_to_m4a: %s -> %s", input_path, output_path);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -i \"%s\" -c:a copy -f ipod \"%s\" -y 2>/dev/null",
             input_path, output_path);

    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "_remux_aac_to_m4a: ffmpeg failed");
        return false;
    }
    return true;
}

/* Port of Python: _quote_references_own_message */
bool _quote_references_own_message(json_t* quote, const char* own_id)
{
    if (!quote || !own_id) return false;
    hermes_log(LOG_DEBUG, "port", "_quote_references_own_message: own_id=%s", own_id);

    json_t* author = json_object_get(quote, "author");
    if (author) {
        const char* author_str = json_node_get_string(author);
        if (author_str && strcmp(author_str, own_id) == 0) return true;
    }

    json_t* uuid = json_object_get(quote, "authorUuid");
    if (uuid) {
        const char* uuid_str = json_node_get_string(uuid);
        if (uuid_str && strcmp(uuid_str, own_id) == 0) return true;
    }

    return false;
}

/* Port of Python: _validate_send_result */
bool _validate_send_result(json_t* result)
{
    if (!result) return false;
    hermes_log(LOG_DEBUG, "port", "_validate_send_result: called");

    json_t* error = json_object_get(result, "error");
    if (error) {
        const char* err_str = json_node_get_string(error);
        if (err_str && err_str[0]) {
            hermes_log(LOG_WARNING, "port", "_validate_send_result: error=%s", err_str);
            return false;
        }
    }

    json_t* success = json_object_get(result, "success");
    if (success) {
        const char* s = json_node_get_string(success);
        if (s && strcmp(s, "false") == 0) return false;
    }

    return true;
}
