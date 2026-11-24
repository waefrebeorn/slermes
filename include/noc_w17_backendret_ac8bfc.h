#ifndef NOC_W17_BACKENDRET_AC8BFC_H
#define NOC_W17_BACKENDRET_AC8BFC_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/backend_retirement.py (gate w17_backendret_ac8bfc).
 * RetirementFence's private state is injected as explicit arguments so each
 * method can be exercised deterministically (no threads, no wall clock, no
 * random tokens) by the differential oracle. */
json_t *w17_backendret_ac8bfc_acquire(int preparing, int has_token, int active,
                                      int committed, double now, double expires);
json_t *w17_backendret_ac8bfc_release(int active);
json_t *w17_backendret_ac8bfc_commit(const char *token, int token_is_null,
                                     int has_cur, const char *cur,
                                     int committed, double now, double expires);
json_t *w17_backendret_ac8bfc_cancel(const char *token, int token_is_null,
                                     int has_cur, const char *cur,
                                     int committed, double now, double expires);

#endif /* NOC_W17_BACKENDRET_AC8BFC_H */
