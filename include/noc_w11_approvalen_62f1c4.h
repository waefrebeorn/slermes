#ifndef NOC_W11_APPROVALEN_62F1C4_H
#define NOC_W11_APPROVALEN_62F1C4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_approvalen_62f1c4_split_env_string @ tools/approval_env_split.py:_split_env_string
 * Returns a JSON array of argv strings, or JSON null for None. */
struct json_t *w11_approvalen_62f1c4_split_env_string(const char *payload);

/* PoP: w11_approvalen_62f1c4_env_split_payload @ tools/approval_env_split.py:_env_split_payload
 * Returns a JSON string (shlex.join of rebuilt argv), or JSON null for None. */
struct json_t *w11_approvalen_62f1c4_env_split_payload(const struct json_t *tokens);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_APPROVALEN_62F1C4_H */
