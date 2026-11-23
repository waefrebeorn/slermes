#ifndef NOC_W7_REDACTCISCO_982D70_H
#define NOC_W7_REDACTCISCO_982D70_H
struct json_t;
struct json_t *w7_redactdisco_982d70_is_discord_token(const char *candidate);
struct json_t *w7_redactdisco_982d70_mfa_left_ok(const char *text, int start);
struct json_t *w7_redactdisco_982d70_redact_tokens(const char *text, const char *mask);
#endif
