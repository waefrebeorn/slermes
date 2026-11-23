#ifndef NOC_W11_BROWSERURL_AAC45B_H
#define NOC_W11_BROWSERURL_AAC45B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_browserurl_aac45b_url_literals @ tools/browser_url_policy.py:url_literals
 * Returns the JSON array of extracted URL literals (same order as Python). */
struct json_t *w11_browserurl_aac45b_url_literals(const char *text, int protocol_relative);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_BROWSERURL_AAC45B_H */
