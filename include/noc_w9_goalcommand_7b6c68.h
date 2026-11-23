#ifndef NOC_W9_GOALCOMMAND_7B6C68_H
#define NOC_W9_GOALCOMMAND_7B6C68_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_goalcommand_7b6c68_is_goal_control @ hermes_cli/goal_command.py:is_goal_control
 * Whether a /goal command controls an existing goal rather than replacing it.
 * Returns 1/0. Mirrors arg.strip().lower() then membership in the exact-handler
 * set or the control first-word set. */
int w9_goalcommand_7b6c68_is_goal_control(const char *arg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_GOALCOMMAND_7B6C68_H */
