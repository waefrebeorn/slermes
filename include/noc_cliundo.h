#ifndef NOC_CLIUNDO_H
#define NOC_CLIUNDO_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: cliundo_undone_span @ hermes_cli/cli_undo_hint.py:_undone_span
 * before is the pre-undo history array; indices is the injected result of
 * _user_turn_indices (list of turn indices). turns_undone is a decimal string.
 * Returns JSON [target_stamp, kept_prefix] (a Python (ts, kept) tuple). */
struct json_t *cliundo_undone_span(const struct json_t *before,
                                   const struct json_t *indices,
                                   const char *turns_undone_decimal);

#ifdef __cplusplus
}
#endif
#endif /* NOC_CLIUNDO_H */
