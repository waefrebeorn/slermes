#ifndef NOC_W18_CLICHATER_B6A2C6_H
#define NOC_W18_CLICHATER_B6A2C6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_clichater_b6a2c6_short @ hermes_cli/cli_chat_error_copy.py:_short
 * Copy of `_short`: first stripped line, truncated to `limit` code points
 * (limit < 0 == None == no truncation) with a trailing ellipsis.  Caller frees. */
char *w18_clichater_b6a2c6_short(const char *text, long long limit);

/* PoP: w18_clichater_b6a2c6_refusal_text @ hermes_cli/cli_chat_error_copy.py:refusal_text
 * The refusal as the client shows it.  `data` is the error's `data` dict (or
 * NULL when it has none).  Caller frees.  The launch-credentials special case
 * is delegated in Python to agent.turn_failure_copy and is seam-excluded here. */
char *w18_clichater_b6a2c6_refusal_text(const char *error_str, const struct json_t *data);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CLICHATER_B6A2C6_H */
