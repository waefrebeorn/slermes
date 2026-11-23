#ifndef NOC_W14_SIDEQUESTI_C37FA3_H
#define NOC_W14_SIDEQUESTI_C37FA3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sidequesti_c37fa3_has_server_side_tools @ agent/side_question_request.py:has_server_side_tools */
int w14_sidequesti_c37fa3_has_server_side_tools(const struct json_t *api_kwargs);

/* PoP: w14_sidequesti_c37fa3_runtime_key @ agent/side_question_request.py:runtime_key */
struct json_t *w14_sidequesti_c37fa3_runtime_key(const struct json_t *agent);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SIDEQUESTI_C37FA3_H */
