#ifndef NOC_W18_COMMANDSCO_AF81E3_H
#define NOC_W18_COMMANDSCO_AF81E3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_commandsco_af81e3_split_args @ hermes_cli/commands_completion.py:_split_args
 * Returns [completed_words (array of strings), partial (string)].  Caller frees. */
struct json_t *w18_commandsco_af81e3_split_args(const char *sub_text);

/* PoP: w18_commandsco_af81e3_extract_path_word @ hermes_cli/commands_completion.py:_extract_path_word
 * Returns a malloc'd string, or NULL for Python None.  Caller frees. */
char *w18_commandsco_af81e3_extract_path_word(const char *text);

/* PoP: w18_commandsco_af81e3_score_path @ hermes_cli/commands_completion.py:_score_path
 * Fuzzy path score (higher is better, 0 = no match, 1 = empty query). */
int w18_commandsco_af81e3_score_path(const char *filepath, const char *query);

/* PoP: w18_commandsco_af81e3_normalize_skill_token @ hermes_cli/commands_completion.py:SlashCommandCompleter._normalize_skill_token
 * Canonical hyphenated /slug.  Caller frees. */
char *w18_commandsco_af81e3_normalize_skill_token(const char *token);

/* PoP: w18_commandsco_af81e3_completion_text @ hermes_cli/commands_completion.py:SlashCommandCompleter._completion_text
 * Replacement text; exact non-picker matches get a trailing space.  Caller frees. */
char *w18_commandsco_af81e3_completion_text(const char *cmd_name, const char *word);

/* PoP: w18_commandsco_af81e3_extract_context_word @ hermes_cli/commands_completion.py:SlashCommandCompleter._extract_context_word
 * Returns a malloc'd bare @ token, or NULL for Python None.  Caller frees. */
char *w18_commandsco_af81e3_extract_context_word(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_COMMANDSCO_AF81E3_H */
