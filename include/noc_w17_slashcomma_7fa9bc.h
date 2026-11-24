#ifndef NOC_W17_SLASHCOMMA_7FA9BC_H
#define NOC_W17_SLASHCOMMA_7FA9BC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_slashcomma_7fa9bc_clean_str @ gateway/slash_commands_status.py:_clean_str
 * Returns a heap copy: stripped value if non-empty, else "". Caller frees. */
char *w17_slashcomma_7fa9bc_clean_str(const struct json_t *value);
/* PoP: w17_slashcomma_7fa9bc_int_value @ gateway/slash_commands_status.py:_int_value */
long long w17_slashcomma_7fa9bc_int_value(const struct json_t *value);
/* PoP: w17_slashcomma_7fa9bc_fmt @ gateway/slash_commands_status.py:_fmt
 * Heap copy of f"{n:,}". Caller frees. */
char *w17_slashcomma_7fa9bc_fmt(const struct json_t *n);
/* PoP: w17_slashcomma_7fa9bc_pct @ gateway/slash_commands_status.py:_pct */
double w17_slashcomma_7fa9bc_pct(double used, double total);
/* PoP: w17_slashcomma_7fa9bc_clip @ gateway/slash_commands_status.py:_clip
 * Heap copy, UTF-8 codepoint-aware. Caller frees. */
char *w17_slashcomma_7fa9bc_clip(const char *text, int limit);
/* PoP: w17_slashcomma_7fa9bc_routes_for_default @ gateway/slash_commands_status.py:_routes_for_default
 * Returns [persisted_route, session_row] as a JSON array. */
struct json_t *w17_slashcomma_7fa9bc_routes_for_default(const struct json_t *default_route,
                                        const struct json_t *persisted_route,
                                        const struct json_t *session_row);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_SLASHCOMMA_7FA9BC_H */
