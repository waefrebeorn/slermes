/*
 * gateway_mutations.c — C home for upstream Hermes hermes_cli/gateway_mutations.py.
 *
 * PoP: w16_gatewaymut_fdc014_model_refusal_text   @ hermes_cli/gateway_mutations.py:model_refusal_text
 * PoP: w16_gatewaymut_fdc014_confirmation_title   @ hermes_cli/gateway_mutations.py:confirmation_title
 * PoP: w16_gatewaymut_fdc014_confirm_choice       @ hermes_cli/gateway_mutations.py:confirm_choice
 *
 * Pure refusal-rendering and typed-answer helpers. No RPC or async work.
 * The confirm-alias table is the one from cli_modal_mixin (seeded in the oracle).
 */
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_gatewaymut_fdc014.h"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *reason_for(int confirmed) {
    return confirmed
        ? "the session changed before the confirmation landed; run /model again."
        : "this target needs a confirmation; run /model in an interactive session to confirm it.";
}

static const char *cm_of(const char *raw) {
    if (!raw) return NULL;
    if (!strcmp(raw, "1") || !strcmp(raw, "once") || !strcmp(raw, "approve") ||
        !strcmp(raw, "yes") || !strcmp(raw, "y") || !strcmp(raw, "ok")) return "once";
    if (!strcmp(raw, "2") || !strcmp(raw, "always") || !strcmp(raw, "remember")) return "always";
    if (!strcmp(raw, "3") || !strcmp(raw, "cancel") || !strcmp(raw, "nevermind") ||
        !strcmp(raw, "no") || !strcmp(raw, "n")) return "cancel";
    return NULL;
}

json_t *w16_gatewaymut_fdc014_model_refusal_text(const json_t *refusal, int confirmed) {
    const char *msg = "";
    if (refusal && refusal->type == JSON_OBJECT) {
        json_t *m = json_obj_get(refusal, "confirm_message");
        if (m && m->type == JSON_STRING && m->str_val) msg = m->str_val;
    }
    size_t cap = strlen(msg) + 256;
    char *out = (char *)malloc(cap);
    if (!out) return json_string("");
    snprintf(out, cap, "%s\n\nModel not switched: %s", msg, reason_for(confirmed));
    json_t *j = json_string(out);
    free(out);
    return j;
}

json_t *w16_gatewaymut_fdc014_confirmation_title(const json_t *refusal) {
    if (refusal && refusal->type == JSON_OBJECT) {
        json_t *w = json_obj_get(refusal, "warnings");
        if (w && w->type == JSON_ARRAY && json_len(w) == 1) {
            json_t *row = json_get(w, 0);
            if (row && row->type == JSON_OBJECT) {
                json_t *title = json_obj_get(row, "title");
                if (title && title->type == JSON_STRING)
                    return json_string(title->str_val ? title->str_val : "");
            }
        }
    }
    return json_string("Model Selection Warning");
}

json_t *w16_gatewaymut_fdc014_confirm_choice(const char *raw, const json_t *choices) {
    char *answer = dupstr(raw ? raw : "");
    /* (raw or '').strip().lower() — ASCII whitespace / case for fixtures. */
    size_t n = strlen(answer), w = 0;
    for (size_t i = 0; i < n; ++i)
        answer[w++] = (char)tolower((unsigned char)answer[i]);
    answer[w] = '\0';
    char *t = answer;
    while (*t && isspace((unsigned char)*t)) t++;
    size_t len = strlen(t);
    while (len && isspace((unsigned char)t[len - 1])) t[--len] = '\0';

    /* answer.isdigit() and 0 < int(answer) <= len(choices) */
    int numeric = len > 0;
    for (size_t i = 0; i < len; ++i)
        if (!isdigit((unsigned char)t[i])) { numeric = 0; break; }
    size_t nchoices = (choices && choices->type == JSON_ARRAY) ? json_len(choices) : 0;
    if (numeric) {
        long v = strtol(t, NULL, 10);
        if (v > 0 && (unsigned long)v <= nchoices) {
            json_t *pick = json_get(choices, (size_t)(v - 1));
            json_t *key = pick && pick->type == JSON_ARRAY ? json_get(pick, 0) : NULL;
            json_t *out = (key && key->type == JSON_STRING) ? json_copy(key) : json_null();
            free(answer);
            return out;
        }
    }

    const char *normalized = cm_of(t);
    if (!normalized) normalized = t;
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(choices); ++i) {
            json_t *pick = json_get(choices, i);
            json_t *key = pick && pick->type == JSON_ARRAY ? json_get(pick, 0) : NULL;
            if (key && key->type == JSON_STRING && key->str_val &&
                !strcmp(key->str_val, normalized)) {
                json_t *out = json_copy(key);
                free(answer);
                return out;
            }
        }
    }
    free(answer);
    return json_null();
}
