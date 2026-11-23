/*
 * cli_modal_mixin.c — C port of upstream Hermes hermes_cli/cli_modal_mixin.py.
 *
 * PoP: w13_climodalm_3a2dee_approval_choices @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._approval_choices
 * PoP: w13_climodalm_3a2dee_normalize_choice @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._normalize_slash_confirm_choice
 * PoP: w13_climodalm_3a2dee_connection_opening_phase @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._connection_opening_phase
 * PoP: w13_climodalm_3a2dee_split_destructive_skip @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._split_destructive_skip
 * PoP: w13_climodalm_3a2dee_connection_fields @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._connection_fields
 *
 * Pure modal helpers: no event loop, prompt_toolkit or file I/O. All inputs
 * arrive as JSON seams; outputs are fresh json_t values owned by the caller.
 */
#include "noc_w13_climodalm_3a2dee.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 0;
}

static void lower_copy(const char *src, char *dst, size_t cap) {
    size_t k = 0;
    for (const char *p = src ? src : ""; *p && k + 1 < cap; p++)
        dst[k++] = (char)tolower((unsigned char)*p);
    dst[k] = '\0';
}

static void trim_ascii(char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (b > a) memmove(s, s + a, b - a);
    s[b - a] = '\0';
}

json_t *w13_climodalm_3a2dee_approval_choices(const json_t *command,
        int allow_permanent, int allow_session, int smart_denied) {
    json_t *out = json_array();
    if (smart_denied || !allow_session) {
        json_append(out, json_string("once"));
        json_append(out, json_string("deny"));
    } else if (allow_permanent) {
        json_append(out, json_string("once"));
        json_append(out, json_string("session"));
        json_append(out, json_string("always"));
        json_append(out, json_string("deny"));
    } else {
        json_append(out, json_string("once"));
        json_append(out, json_string("session"));
        json_append(out, json_string("deny"));
    }
    const char *cmd = (command && command->type == JSON_STRING) ? command->str_val : "";
    if (cmd && strlen(cmd) > 70) json_append(out, json_string("view"));
    return out;
}

json_t *w13_climodalm_3a2dee_normalize_choice(const struct json_t *raw,
        const struct json_t *choices) {
    if (!raw || raw->type != JSON_STRING || !raw->str_val) return json_null();
    char norm[1024];
    lower_copy(raw->str_val, norm, sizeof norm);
    trim_ascii(norm);
    if (!*norm) return json_null();

    int allowed_once = 0, allowed_always = 0, allowed_cancel = 0, allowed_raw = 0;
    if (json_is_array(choices)) {
        size_t n = json_len(choices);
        for (size_t i = 0; i < n; i++) {
            json_t *row = json_get(choices, i);
            const char *val = NULL;
            if (json_is_array(row) && json_len(row) > 0) {
                json_t *first = json_get(row, 0);
                if (first && first->type == JSON_STRING) val = first->str_val;
            } else if (row && row->type == JSON_STRING) {
                val = row->str_val;
            }
            if (!val) continue;
            if (!strcmp(val, "once")) allowed_once = 1;
            else if (!strcmp(val, "always")) allowed_always = 1;
            else if (!strcmp(val, "cancel")) allowed_cancel = 1;
            if (!strcmp(val, norm)) allowed_raw = 1;
        }
    }
    const char *canonical = NULL;
    if (!strcmp(norm, "1") || !strcmp(norm, "once") || !strcmp(norm, "approve") ||
        !strcmp(norm, "yes") || !strcmp(norm, "y") || !strcmp(norm, "ok"))
        canonical = "once";
    else if (!strcmp(norm, "2") || !strcmp(norm, "always") || !strcmp(norm, "remember"))
        canonical = "always";
    else if (!strcmp(norm, "3") || !strcmp(norm, "cancel") || !strcmp(norm, "nevermind") ||
             !strcmp(norm, "no") || !strcmp(norm, "n"))
        canonical = "cancel";
    if (canonical) {
        if ((!strcmp(canonical, "once") && allowed_once) ||
            (!strcmp(canonical, "always") && allowed_always) ||
            (!strcmp(canonical, "cancel") && allowed_cancel))
            return json_string(canonical);
        /* canonical found but not allowed: fall through to raw membership */
    }
    if (allowed_raw) return json_string(norm);
    return json_null();
}

json_t *w13_climodalm_3a2dee_connection_opening_phase(const struct json_t *target) {
    const char *state = json_get_str(target, "state", "");
    if (!state) state = "";
    json_t *url = json_obj_get(target, "connect_url");
    if (!strcmp(state, "initiated") && json_truthy(url)) return json_string("url");
    if (!strcmp(state, "failed")) return json_string("failed");
    if (!strcmp(state, "pending")) {
        const char *action = json_get_str(target, "action", "");
        if (!action || strcmp(action, "authorize") != 0) return json_string("form");
    }
    return json_string("waiting");
}

json_t *w13_climodalm_3a2dee_split_destructive_skip(const struct json_t *cmd_text) {
    const char *s = (cmd_text && cmd_text->type == JSON_STRING) ? cmd_text->str_val : "";
    char buf[8192];
    size_t n = strlen(s ? s : "");
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, s ? s : "", n);
    buf[n] = '\0';
    char *tok[512];
    size_t nt = 0;
    char *save = NULL;
    for (char *t = strtok_r(buf, " \t\r\n\f\v", &save); t; t = strtok_r(NULL, " \t\r\n\f\v", &save)) {
        if (nt < 512) tok[nt++] = t;
    }
    size_t start = 0;
    if (nt > 0 && tok[0][0] == '/') start = 1;
    char kept[512][64];
    size_t nk = 0;
    for (size_t i = start; i < nt; i++) {
        char low[64];
        lower_copy(tok[i], low, sizeof low);
        int is_skip = !strcmp(low, "now") || !strcmp(low, "--yes") || !strcmp(low, "-y");
        if (!is_skip && nk < 512) {
            size_t tl = strlen(tok[i]);
            if (tl >= sizeof kept[0]) tl = sizeof kept[0] - 1;
            memcpy(kept[nk], tok[i], tl);
            kept[nk][tl] = '\0';
            nk++;
        }
    }
    char joined[8192];
    size_t used = 0;
    joined[0] = '\0';
    for (size_t i = 0; i < nk; i++) {
        size_t tl = strlen(kept[i]);
        if (used + tl + 2 < sizeof joined) {
            if (used) { joined[used++] = ' '; }
            memcpy(joined + used, kept[i], tl);
            used += tl;
            joined[used] = '\0';
        }
    }
    json_t *out = json_array();
    json_append(out, json_string(joined));
    json_append(out, json_bool(nk != (nt - start)));
    return out;
}

json_t *w13_climodalm_3a2dee_connection_fields(const struct json_t *target) {
    json_t *out = json_array();
    json_t *req = json_obj_get(target, "required_env");
    if (json_is_array(req)) {
        size_t n = json_len(req);
        for (size_t i = 0; i < n; i++) {
            json_t *field = json_get(req, i);
            if (json_is_object(field)) json_append(out, json_copy(field));
        }
    }
    return out;
}
