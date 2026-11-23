/*
 * hermes_state_loop_guard.c — faithful C11 port of the pure mode decisions of
 * upstream Hermes hermes_state_loop_guard.py (gate w5_hermesstate_a0163a).
 *
 * PoP: w5_hermesstate_a0163a_valid_mode @ hermes_state_loop_guard.py:install
 * PoP: w5_hermesstate_a0163a_install_mode @ hermes_state_loop_guard.py:install
 * PoP: w5_hermesstate_a0163a_install_from_config @ hermes_state_loop_guard.py:install_from_config
 *
 * The monkeypatch of StoreHolder and the warning/record state are side effects
 * that stay in Python; the mode-selection decision is ported verbatim here,
 * including Python's str(value).strip().lower() coercion and the
 * `value not in (None, False)` guard (which also matches numeric zero).
 */
#include "noc_w5_hermesstate_a0163a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int w5g_is_mode(const char *m) {
    return m && (!strcmp(m, "warn") || !strcmp(m, "raise") || !strcmp(m, "record"));
}

struct json_t *w5_hermesstate_a0163a_valid_mode(const char *mode) {
    return json_bool(w5g_is_mode(mode) ? 1 : 0);
}

struct json_t *w5_hermesstate_a0163a_install_mode(const char *mode, const char *current_mode) {
    if (w5g_is_mode(mode)) return json_string(mode);
    return current_mode ? json_string(current_mode) : json_null();
}

/* Python: value not in (None, False). False == 0 == 0.0, so numeric zero is
 * treated the same as None/False. */
static int w5g_none_or_false(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL && !v->bool_val) return 1;
    if (v->type == JSON_NUMBER && v->num_val == 0.0) return 1;
    return 0;
}

/* Faithful str(value) for the JSON scalar types used by config.yaml, followed
 * by .strip().lower(). Container types are not ported (fixtures stay scalar). */
static void w5g_mode_from_value(const struct json_t *v, char *out, size_t cap) {
    char raw[512];
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); }
    else if (v->type == JSON_BOOL) { snprintf(out, cap, v->bool_val ? "True" : "False"); }
    else if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); }
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val) && v->num_val >= -9.0e15 && v->num_val <= 9.0e15)
            snprintf(raw, sizeof(raw), "%.0f", v->num_val);
        else
            snprintf(raw, sizeof(raw), "%g", v->num_val);
        snprintf(out, cap, "%s", raw);
    } else { out[0] = '\0'; }

    /* strip ASCII whitespace then lower */
    {
        size_t a = 0, b = strlen(out);
        while (a < b && isspace((unsigned char)out[a])) a++;
        while (b > a && isspace((unsigned char)out[b-1])) b--;
        if (b > a) memmove(out, out + a, b - a);
        out[b - a] = '\0';
        for (size_t i = 0; out[i]; i++) out[i] = (char)tolower((unsigned char)out[i]);
    }
}

static int w5g_off_mode(const char *mode) {
    return !*mode || !strcmp(mode, "off") || !strcmp(mode, "false") || !strcmp(mode, "none");
}

struct json_t *w5_hermesstate_a0163a_install_from_config(const struct json_t *cfg,
                                                         const char *current_mode) {
    const struct json_t *gateway = NULL, *value = NULL;
    char mode[512];
    if (cfg && cfg->type == JSON_OBJECT) {
        gateway = json_obj_get((struct json_t *)cfg, "gateway");
        if (gateway && gateway->type == JSON_OBJECT)
            value = json_obj_get((struct json_t *)gateway, "sessiondb_loop_guard");
    }
    if (w5g_none_or_false(value)) mode[0] = '\0';
    else w5g_mode_from_value(value, mode, sizeof(mode));
    if (w5g_off_mode(mode)) return json_null();
    return w5_hermesstate_a0163a_install_mode(mode, current_mode);
}
