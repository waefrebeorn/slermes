#ifndef NOC_W8_APPROVALFLO_1E3A51_H
#define NOC_W8_APPROVALFLO_1E3A51_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_approvalflo_1e3a51_user_deny_block_result @ tools/approval_floors.py:_user_deny_block_result */
struct json_t *w8_approvalflo_1e3a51_user_deny_block_result(const char *pattern);

/* PoP: w8_approvalflo_1e3a51_sudo_stdin_block_result @ tools/approval_floors.py:_sudo_stdin_block_result */
struct json_t *w8_approvalflo_1e3a51_sudo_stdin_block_result(const char *description);

/* PoP: w8_approvalflo_1e3a51_hardline_block_result @ tools/approval_floors.py:_hardline_block_result
 * Non-recovery descriptions only (no payload save / filesystem access). Caller frees. */
struct json_t *w8_approvalflo_1e3a51_hardline_block_result(const char *description, const char *command);

/* PoP: w8_approvalflo_1e3a51_has_allowlist_shell_operator @ tools/approval_floors.py:_has_allowlist_shell_operator */
int w8_approvalflo_1e3a51_has_allowlist_shell_operator(const char *command);

/* PoP: w8_approvalflo_1e3a51_matching_permanent_allowlist_entry @ tools/approval_floors.py:_matching_permanent_allowlist_entry
 * Explicit patterns seam: `patterns` is a JSON array of strings, injected instead of
 * reading tools.approval global state. Returns a malloc'd matching pattern or NULL. */
char *w8_approvalflo_1e3a51_matching_allowlist_ex(const char *command, const struct json_t *patterns);

/* PoP: w8_approvalflo_1e3a51_command_matches_permanent_allowlist @ tools/approval_floors.py:_command_matches_permanent_allowlist */
int w8_approvalflo_1e3a51_command_matches_allowlist_ex(const char *command, const struct json_t *patterns);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_APPROVALFLO_1E3A51_H */