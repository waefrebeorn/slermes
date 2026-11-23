#ifndef NOC_W13_DEBUGREDAC_C651E1_H
#define NOC_W13_DEBUGREDAC_C651E1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_debugredac_c651e1_decoded_compact_name @ hermes_cli/debug_redaction.py:_decoded_compact_name
 * str(value or "") -> URL-unquote_plus (up to 3 stable rounds) -> casefold ->
 * compact alnum-only name. Hyphens/underscores from canonicalization are not alnum
 * and drop out, so the result is the lowercased alnum run of the decoded value. */
struct json_t *w13_debugredac_c651e1_decoded_compact_name(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_DEBUGREDAC_C651E1_H */
