#define _POSIX_C_SOURCE 200809L
/* session_lost_and_found_pure.c — Faithful C11 port of the pure, deterministic
 * classification heuristics in hermes_cli/session_lost_and_found.py (page-level
 * schema-less session DB salvage).
 *
 * Ports (PoP):
 *   _is_session_id                  -> laf_is_session_id
 *   _looks_like_source              -> laf_looks_like_source
 *   classify_lost_and_found_row     -> laf_classify_row
 *   _heuristic_started_at           -> laf_heuristic_started_at
 *
 * The sqlite/.recover/CLI glue is NOT ported (subprocess + DB, not pure logic).
 *
 * laf_classify_row(nfield, cells_json) returns a malloc'd JSON string:
 *   a quoted class name ("sessions" | "messages" | "session_model_usage")
 *   or the literal "null" when unclassifiable. laf_heuristic_started_at
 *   returns a double (0.0 when no in-window numeric cell).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LAF_EPOCH_LOW  1000000000.0
#define LAF_EPOCH_HIGH 4000000000.0

/* SESSION_ID_PATTERN = ^\d{8}_\d{6}_ */
int laf_is_session_id(const char *value) {
    if (!value) return 0;
    for (int i = 0; i < 8; i++) if (value[i] < '0' || value[i] > '9') return 0;
    if (value[8] != '_') return 0;
    for (int i = 9; i < 15; i++) if (value[i] < '0' || value[i] > '9') return 0;
    if (value[15] != '_') return 0;
    return 1;
}

static int laf_known_source(const char *v) {
    static const char *known[] = {
        "cli", "telegram", "discord", "slack", "whatsapp", "signal", "matrix",
        "irc", "email", "x", "twitter", "api", "gateway", "web", "dashboard",
        "tool", "subagent", "cron", "recovered", "imported", "acp", NULL
    };
    for (int i = 0; known[i]; i++) if (strcmp(v, known[i]) == 0) return 1;
    return 0;
}

/* ^[a-z][a-z0-9_-]{0,31}$ */
static int laf_source_shape(const char *v) {
    size_t n = strlen(v);
    if (n == 0 || n > 32) return 0;
    if (!(v[0] >= 'a' && v[0] <= 'z')) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = v[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

int laf_looks_like_source(const char *value) {
    if (!value || !*value) return 0;
    return laf_known_source(value) || laf_source_shape(value);
}

/* MESSAGE_ROLES membership */
static int laf_is_role(const char *v) {
    return strcmp(v, "user") == 0 || strcmp(v, "assistant") == 0 ||
           strcmp(v, "tool") == 0 || strcmp(v, "system") == 0;
}

char *laf_classify_row(int nfield, const char *cells_json) {
    json_t *cells = json_parse(cells_json, NULL);
    char *result = (char *)malloc(8);
    strcpy(result, "null");

    if (cells && cells->type == JSON_ARRAY) {
        size_t n = json_len(cells);
        /* cells[0] for the rowid-alias messages case */
        json_t *c0 = (n > 0) ? json_array_get(cells, 0) : NULL;
        json_t *c1 = (n > 1) ? json_array_get(cells, 1) : NULL;
        json_t *c2 = (n > 2) ? json_array_get(cells, 2) : NULL;

        if (n >= 3 && c0 && c0->type == JSON_NULL) {
            if (c1 && c1->type == JSON_STRING && c1->str_val && *c1->str_val &&
                c2 && c2->type == JSON_STRING && c2->str_val &&
                laf_is_role(c2->str_val)) {
                free(result);
                result = strdup("\"messages\"");
                json_free(cells);
                return result;
            }
            json_free(cells);
            return result; /* null */
        }

        const char *idval = (c0 && c0->type == JSON_STRING) ? c0->str_val : NULL;
        if (!laf_is_session_id(idval)) {
            json_free(cells);
            return result; /* null */
        }

        const char *srcval = (c1 && c1->type == JSON_STRING) ? c1->str_val : NULL;

        if (nfield == 18) { /* SESSION_MODEL_USAGE_NFIELD */
            if (srcval && *srcval) {
                free(result);
                result = strdup("\"session_model_usage\"");
            }
        } else if (nfield == 55 || nfield == 54 || nfield == 52 || nfield == 14) {
            if (laf_looks_like_source(srcval)) {
                free(result);
                result = strdup("\"sessions\"");
            }
        } else if (nfield >= 30 && laf_looks_like_source(srcval)) {
            free(result);
            result = strdup("\"sessions\"");
        }
    }
    json_free(cells);
    return result;
}

double laf_heuristic_started_at(const char *cells_json) {
    json_t *cells = json_parse(cells_json, NULL);
    double found = 0.0;
    if (cells && cells->type == JSON_ARRAY) {
        size_t n = json_len(cells);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_array_get(cells, i);
            if (v && v->type == JSON_NUMBER) {
                double d = v->num_val;
                if (d >= LAF_EPOCH_LOW && d <= LAF_EPOCH_HIGH) { found = d; break; }
            }
        }
    }
    json_free(cells);
    return found;
}
