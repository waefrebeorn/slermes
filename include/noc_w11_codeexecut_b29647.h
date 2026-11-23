#ifndef NOC_W11_CODEEXECUT_B29647_H
#define NOC_W11_CODEEXECUT_B29647_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_codeexecut_b29647_private_dirs_cmd @ tools/code_execution_rpc.py:_private_dirs_cmd */
struct json_t *w11_codeexecut_b29647_private_dirs_cmd(const char *root, const struct json_t *subdirs);

/* PoP: w11_codeexecut_b29647_rpc_token_ok @ tools/code_execution_rpc.py:_rpc_token_ok */
struct json_t *w11_codeexecut_b29647_rpc_token_ok(const struct json_t *request, const char *rpc_token);

/* PoP: w11_codeexecut_b29647_result_error @ tools/code_execution_rpc.py:_result_error */
struct json_t *w11_codeexecut_b29647_result_error(const char *result);

/* PoP: w11_codeexecut_b29647_tool_errors_since @ tools/code_execution_rpc.py:tool_errors_since */
struct json_t *w11_codeexecut_b29647_tool_errors_since(const struct json_t *tool_call_log, long long start);

/* PoP: w11_codeexecut_b29647_execute_checked @ tools/code_execution_rpc.py:_execute_checked
 * `execute_result` is the injected env.execute() result (no live subprocess). */
struct json_t *w11_codeexecut_b29647_execute_checked(const char *cmd, const char *what,
                                                     long long timeout,
                                                     const struct json_t *execute_result);

/* PoP: w11_codeexecut_b29647_remote_write @ tools/code_execution_rpc.py:_remote_write */
struct json_t *w11_codeexecut_b29647_remote_write(const char *remote_path, const char *content,
                                                  int atomic, long long timeout, int check,
                                                  const struct json_t *execute_result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_CODEEXECUT_B29647_H */
