#ifndef HERMES_CURSES_UI_H
#define HERMES_CURSES_UI_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

int curses_query_matches(const char *label, const char *query);
double curses_token_score(const char *orig, const char *lower, const char *token, int *matched);
double curses_fuzzy_score(const char *label, const char *query, int *matched);
json_t *curses_filter_indices(json_t *items, const char *query);
void curses_reconcile_cursor(json_t *filtered, int cursor, int *out_cursor, int *out_pos);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CURSES_UI_H */
