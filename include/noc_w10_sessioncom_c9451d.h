#ifndef NOC_W10_SESSIONCOM_C9451D_H
#define NOC_W10_SESSIONCOM_C9451D_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessioncom_c9451d_first_word @ gateway/session_commands.py:_first_word */
struct json_t *w10_sessioncom_c9451d_first_word(const char *arg);
/* PoP: w10_sessioncom_c9451d_first_token @ gateway/session_commands.py:_first_token */
struct json_t *w10_sessioncom_c9451d_first_token(const char *arg);
/* PoP: w10_sessioncom_c9451d_command_verdict @ gateway/session_commands.py:command_verdict */
struct json_t *w10_sessioncom_c9451d_command_verdict(const char *name, const char *arg);
/* PoP: w10_sessioncom_c9451d_refusal_hint @ gateway/session_commands.py:REFUSAL_HINTS */
struct json_t *w10_sessioncom_c9451d_refusal_hint(const char *name);
/* PoP: w10_sessioncom_c9451d_text @ gateway/session_commands.py:_text */
struct json_t *w10_sessioncom_c9451d_text(const struct json_t *output);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONCOM_C9451D_H */
