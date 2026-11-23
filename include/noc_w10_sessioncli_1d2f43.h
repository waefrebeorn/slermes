#ifndef NOC_W10_SESSIONCLI_1D2F43_H
#define NOC_W10_SESSIONCLI_1D2F43_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessioncli_1d2f43_submit_cli_turn_fields @ gateway/session_cli_turn.py:submit_cli_turn_fields */
struct json_t *w10_sessioncli_1d2f43_submit_cli_turn_fields(const struct json_t *params);
/* PoP: w10_sessioncli_1d2f43_take_cli_result_fields @ gateway/session_cli_turn.py:take_cli_result_fields */
struct json_t *w10_sessioncli_1d2f43_take_cli_result_fields(const struct json_t *result);
/* PoP: w10_sessioncli_1d2f43_admit_cli_turn @ gateway/session_cli_turn.py:admit_cli_turn */
struct json_t *w10_sessioncli_1d2f43_admit_cli_turn(const struct json_t *params);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONCLI_1D2F43_H */
