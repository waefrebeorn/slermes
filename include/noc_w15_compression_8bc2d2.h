#ifndef NOC_W15_COMPRESSION_8BC2D2_H
#define NOC_W15_COMPRESSION_8BC2D2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_compression_8bc2d2_carry @ agent/compression_automation.py:carry_compression_automation
 * Reproduces the observed call sequence of the carry/migrate side effects as
 * [{"fn":...,"args":[old,new],"kwargs":{...}?}, ...]. is_runtime_store mirrors
 * isinstance(agent._session_db, RuntimeSessionStore). Caller frees. */
struct json_t *w15_compression_8bc2d2_carry(const char *old_session_id,
                                            const char *new_session_id,
                                            int is_runtime_store);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_COMPRESSION_8BC2D2_H */
