/*
 * port_skills_hub.c — Port of Python
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python: do_diff */
void do_diff(const char * name, const char * console) {
    if (!name) return;
    /* do_diff */
    hermes_log(LOG_DEBUG, "port", "[do_diff] called");
    (void)name;
    (void)console;
    return;
}

/* Port of Python: do_list_modified */
void do_list_modified(const char * console, const char * as_json) {
    if (!console) return;
    /* do_list_modified */
    hermes_log(LOG_DEBUG, "port", "[do_list_modified] called");
    (void)console;
    (void)as_json;
    return;
}

