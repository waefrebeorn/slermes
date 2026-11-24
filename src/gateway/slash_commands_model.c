/*
 * slash_commands_model.c — C port of upstream Hermes gateway/slash_commands_model.py.
 *
 * PoP: w16_slashcomma_e06e76_fast_selections_table @ gateway/slash_commands_model.py:_FAST_SELECTIONS
 * PoP: w16_slashcomma_e06e76_fast_selection @ gateway/slash_commands_model.py:_FAST_SELECTIONS.get
 * PoP: w16_slashcomma_e06e76_reasoning_toggles_table @ gateway/slash_commands_model.py:_REASONING_DISPLAY_TOGGLES
 * PoP: w16_slashcomma_e06e76_reasoning_toggle @ gateway/slash_commands_model.py:_REASONING_DISPLAY_TOGGLES.get
 * PoP: w16_slashcomma_e06e76_text_listing_models @ gateway/slash_commands_model.py:_TEXT_LISTING_MODELS
 *
 * The two tables are the single source of truth. Lookups mirror Python
 * dict.get semantics: a missing key (or a null argument) returns JSON null.
 */
#include "noc_w16_slashcomma_e06e76.h"
#include <string.h>

typedef struct {
    const char *arg;
    const char *tier;   /* NULL == Python None */
    const char *saved;
    const char *label;  /* NULL == Python None (caller uses value.upper()) */
} fast_sel_t;

static const fast_sel_t FAST_SELS[] = {
    { "fast",      "priority",  "fast",  "gateway.fast.label_fast"   },
    { "on",        "priority",  "fast",  "gateway.fast.label_fast"   },
    { "normal",    NULL,        "normal", "gateway.fast.label_normal" },
    { "off",       NULL,        "normal", "gateway.fast.label_normal" },
    { "auto",      "auto",      "auto",  NULL                        },
    { "cold",      "cold",      "cold",  NULL                        },
    { "ultrafast", "ultrafast", "ultrafast", NULL                    },
};
#define FAST_N (sizeof(FAST_SELS) / sizeof(FAST_SELS[0]))

static json_t *jstr_or_null(const char *s) {
    return s ? json_string(s) : json_null();
}

static json_t *fast_row(const fast_sel_t *sel) {
    json_t *row = json_array();
    json_append(row, jstr_or_null(sel->tier));
    json_append(row, json_string(sel->saved));
    json_append(row, jstr_or_null(sel->label));
    return row;
}

json_t *w16_slashcomma_e06e76_fast_selections_table(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < FAST_N; i++)
        json_set(out, FAST_SELS[i].arg, fast_row(&FAST_SELS[i]));
    return out;
}

json_t *w16_slashcomma_e06e76_fast_selection(const char *arg) {
    if (!arg) return json_null();
    for (size_t i = 0; i < FAST_N; i++)
        if (!strcmp(arg, FAST_SELS[i].arg)) return fast_row(&FAST_SELS[i]);
    return json_null();
}

typedef struct { const char *arg; bool show; } reason_t;
static const reason_t REASON_TOGGLES[] = {
    { "show", true }, { "on", true }, { "hide", false }, { "off", false },
};
#define REASON_N (sizeof(REASON_TOGGLES) / sizeof(REASON_TOGGLES[0]))

json_t *w16_slashcomma_e06e76_reasoning_toggles_table(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < REASON_N; i++)
        json_set(out, REASON_TOGGLES[i].arg, json_bool(REASON_TOGGLES[i].show));
    return out;
}

json_t *w16_slashcomma_e06e76_reasoning_toggle(const char *arg) {
    if (!arg) return json_null();
    for (size_t i = 0; i < REASON_N; i++)
        if (!strcmp(arg, REASON_TOGGLES[i].arg)) return json_bool(REASON_TOGGLES[i].show);
    return json_null();
}

int w16_slashcomma_e06e76_text_listing_models(void) { return 5; }
