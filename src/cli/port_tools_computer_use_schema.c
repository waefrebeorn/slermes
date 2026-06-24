/*
 * port_tools_computer_use_schema.c — C port of tools/computer_use/schema.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_computer_use_schema_get_computer_use_schema @ tools/computer_use/schema.py:get_computer_use_schema */

/*
 * get_computer_use_schema: Return the generic OpenAI function-calling schema.
 *
 * Returns: pointer to static schema string (JSON).
 */
void* cli_tools_computer_use_schema_get_computer_use_schema(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    static char schema[4096];
    static int initialized = 0;

    if (!initialized) {
        snprintf(schema, sizeof(schema),
            "{"
            "\"name\":\"computer_use\","
            "\"description\":\"Drive the macOS desktop in the background — screenshots, mouse, keyboard, scroll, drag. Preferred workflow: call with action='capture' (mode='som' gives numbered element overlays), then click by element index for reliability.\","
            "\"parameters\":{"
            "\"type\":\"object\","
            "\"properties\":{"
            "\"action\":{\"type\":\"string\",\"enum\":[\"capture\",\"click\",\"type\",\"scroll\",\"drag\",\"key\",\"wait\",\"screenshot\"],\"description\":\"The action to perform\"},"
            "\"element\":{\"type\":\"integer\",\"description\":\"Element index from SOM capture\"},"
            "\"x\":{\"type\":\"integer\",\"description\":\"Pixel x-coordinate\"},"
            "\"y\":{\"type\":\"integer\",\"description\":\"Pixel y-coordinate\"},"
            "\"text\":{\"type\":\"string\",\"description\":\"Text to type\"},"
            "\"key\":{\"type\":\"string\",\"description\":\"Key or key combination\"},"
            "\"delta_x\":{\"type\":\"integer\",\"description\":\"Horizontal scroll delta\"},"
            "\"delta_y\":{\"type\":\"integer\",\"description\":\"Vertical scroll delta\"},"
            "\"mode\":{\"type\":\"string\",\"enum\":[\"som\",\"pixel\"],\"description\":\"Capture mode\"}"
            "},"
            "\"required\":[\"action\"]"
            "}"
            "}"
            );
        initialized = 1;
    }

    hermes_log(LOG_DEBUG, "port",
               "get_computer_use_schema: returned schema (%zu bytes)", strlen(schema));

    return schema;
}
