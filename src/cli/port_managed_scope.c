/*
 * port_managed_scope.c — Port of Python hermes_cli/managed_scope.c
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * _parse_env — Parse a .env file from a FILE* into a JSON object.
 *
 * Python: def _parse_env(f) -> Dict[str, str]:
 *   out = {}
 *   for line in f:
 *       line = line.strip()
 *       if not line or line.startswith("#") or "=" not in line: continue
 *       key, _, value = line.partition("=")
 *       out[key.strip()] = value.strip().strip("\"'")
 *   return out
 */
/* Port of Python: _parse_env */
json_t* _parse_env(FILE* f)
{
    if (!f) return NULL;

    json_t* out = json_new_object();
    if (!out) return NULL;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        /* Strip leading/trailing whitespace */
        char* start = line;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start);
        while (end > start && (*(end-1) == ' ' || *(end-1) == '\t')) {
            *(--end) = '\0';
        }

        /* Skip empty, comments, no equals */
        if (!start[0] || start[0] == '#') continue;
        char* eq = strchr(start, '=');
        if (!eq) continue;

        /* Split key/value */
        *eq = '\0';
        char* key = start;
        char* value = eq + 1;

        /* Strip quotes from value */
        if (value[0] == '"' || value[0] == '\'') {
            char quote = value[0];
            value++;
            size_t vlen = strlen(value);
            if (vlen > 0 && value[vlen-1] == quote) {
                value[vlen-1] = '\0';
            }
        }

        json_object_set(out, key, json_new_string(value));
    }

    return out;
}
