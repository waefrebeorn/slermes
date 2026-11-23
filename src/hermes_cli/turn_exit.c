/*
 * turn_exit.c — C port of upstream Hermes hermes_cli/turn_exit.py.
 *
 * PoP: w7_turnexit_39298b_turn_exit_code @ hermes_cli/turn_exit.py:turn_exit_code
 * PoP: w7_turnexit_39298b_reasons @ hermes_cli/turn_exit.py:TRANSIENT_PROVIDER_REASONS / TERMINAL_PROVIDER_REASONS
 */
#include "noc_w7_turnexit_39298b.h"
#include "hermes_json.h"
#include <string.h>

#define KANBAN_RATE_LIMIT_EXIT_CODE 75
#define KANBAN_TERMINAL_PROVIDER_EXIT_CODE 78

static const char *const TRANSIENT[] = {
    "rate_limit", "upstream_rate_limit", "billing", "overloaded", "server_error", "timeout"
};
static const char *const TERMINAL[] = {
    "auth", "auth_permanent", "model_not_found", "ssl_cert_verification", "upstream_blocked"
};

/* Python truthiness for the JSON types a turn result may carry. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

static int in_set(const char *const *set, size_t n, const json_t *v) {
    if (!json_is_string(v)) return 0;
    for (size_t i = 0; i < n; i++)
        if (!strcmp(set[i], v->str_val)) return 1;
    return 0;
}

int w7_turnexit_39298b_turn_exit_code(const json_t *result,
                                      int kanban_worker,
                                      int credentials_rate_limited,
                                      int credentials_terminal) {
    if (!json_is_object(result)) {
        if (credentials_rate_limited && kanban_worker) return KANBAN_RATE_LIMIT_EXIT_CODE;
        if (credentials_terminal && kanban_worker) return KANBAN_TERMINAL_PROVIDER_EXIT_CODE;
        return 1;
    }
    if (truthy(json_obj_get(result, "interrupted"))) return 130;
    int failed = truthy(json_obj_get(result, "failed"));
    int partial = truthy(json_obj_get(result, "partial"));
    const json_t *completed = json_obj_get(result, "completed");
    int completed_is_false = json_is_bool(completed) && !completed->bool_val;
    if (!(failed || partial || completed_is_false)) return 0;
    if (kanban_worker) {
        const json_t *reason = json_obj_get(result, "failure_reason");
        if (in_set(TRANSIENT, sizeof(TRANSIENT) / sizeof(TRANSIENT[0]), reason))
            return KANBAN_RATE_LIMIT_EXIT_CODE;
        if (in_set(TERMINAL, sizeof(TERMINAL) / sizeof(TERMINAL[0]), reason))
            return KANBAN_TERMINAL_PROVIDER_EXIT_CODE;
    }
    return 1;
}

struct json_t *w7_turnexit_39298b_reasons(const char *which) {
    static const char *const transient_sorted[] = {
        "billing", "overloaded", "rate_limit", "server_error", "timeout", "upstream_rate_limit"
    };
    static const char *const terminal_sorted[] = {
        "auth", "auth_permanent", "model_not_found", "ssl_cert_verification", "upstream_blocked"
    };
    json_t *arr = json_array();
    if (which && !strcmp(which, "transient")) {
        for (size_t i = 0; i < sizeof(transient_sorted) / sizeof(transient_sorted[0]); i++)
            json_append(arr, json_string(transient_sorted[i]));
    } else if (which && !strcmp(which, "terminal")) {
        for (size_t i = 0; i < sizeof(terminal_sorted) / sizeof(terminal_sorted[0]); i++)
            json_append(arr, json_string(terminal_sorted[i]));
    }
    return arr;
}
