/*
 * self_repo_guard.h — declaration surface for src/tools/port_self_repo_guard.c,
 * the faithful C11 port of Python tools/self_repo_guard.py (detect Git
 * operations that can rewrite the checkout backing this process).
 */

#ifndef SLERMES_SELF_REPO_GUARD_H
#define SLERMES_SELF_REPO_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build-time repo root (parent of the checkout containing src/tools). */
#ifndef SRG_REPO_ROOT
#define SRG_REPO_ROOT "/home/wubu/hermes-agent-dev"
#endif

typedef struct srg_heredoc {
    char *delimiter;
    bool strip_tabs;
    bool execute_as_shell;
    char *body;
    size_t body_len;
} srg_heredoc_t;

/* PoP: get_running_source_root @ tools/self_repo_guard.py:get_running_source_root */
char *srg_get_running_source_root(void);

/* PoP: _resolve @ tools/self_repo_guard.py:_resolve */
char *srg_resolve(const char *path_str, const char *base);

/* PoP: _is_within @ tools/self_repo_guard.py:_is_within */
bool srg_is_within(const char *path, const char *root);

/* PoP: _executable_name @ tools/self_repo_guard.py:_executable_name */
char *srg_executable_name(const char *value);

/* PoP: _shell_words_at @ tools/self_repo_guard.py:_shell_words_at */
char **srg_shell_words_at(const char *command, size_t start);

/* PoP: _command_parts @ tools/self_repo_guard.py:_command_parts */
void srg_command_parts(char **words, char **out_env_packed, char **out_executable, char ***out_args);

/* PoP: _consume_options @ tools/self_repo_guard.py:_consume_options */
size_t srg_consume_options(char **words, size_t start, const char *const *options_with_arg);
/* PoP: _next_positional @ tools/self_repo_guard.py:_next_positional */
size_t srg_next_positional(char **args, size_t index);

/* PoP: _shell_script_arg @ tools/self_repo_guard.py:_shell_script_arg */
char *srg_shell_script_arg(char **args);

/* PoP: _heredoc_specs @ tools/self_repo_guard.py:_heredoc_specs */
srg_heredoc_t **srg_heredoc_specs(const char *line);

/* PoP: _masked_line @ tools/self_repo_guard.py:_masked_line */
char *srg_masked_line(const char *line);

/* PoP: _mask_heredocs @ tools/self_repo_guard.py:_mask_heredocs */
void srg_mask_heredocs(const char *command, char **out_masked, char ***out_scripts);

/* PoP: _cd_target @ tools/self_repo_guard.py:_cd_target */
char *srg_cd_target(const char *executable, char **args, const char *cwd);

/* PoP: detect_self_repo_git_mutation @ tools/self_repo_guard.py:detect_self_repo_git_mutation */
bool srg_detect_self_repo_git_mutation(const char *command, const char *cwd,
                                       const char *source_root,
                                       char **out_operation, char **out_block_message);

/* PoP: _block_message @ tools/self_repo_guard.py:_block_message */
char *srg_block_message(const char *operation, const char *root);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SELF_REPO_GUARD_H */
