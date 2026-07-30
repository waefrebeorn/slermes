/* skill_scaffolding.h — skill-scaffolding extractor surface, faithful C11
 * port of the pure extractor half of agent/skill_commands.py.
 *
 * Recovers what the user actually typed from a /skill- or /bundle-expanded
 * model-facing message. Pure string logic; self-contained; minimal includes.
 * Implemented in src/agent/port_agent_skill_commands.c.
 */

#ifndef SLERMES_SKILL_SCAFFOLDING_H
#define SLERMES_SKILL_SCAFFOLDING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Recover the user's instruction from a slash-skill-expanded turn.
 * Returns (caller frees):
 *   - the original string unchanged when content is NOT skill scaffolding;
 *   - the extracted user instruction when the scaffolding carried one;
 *   - NULL for a bare /skill invocation (no user content worth storing). */
char *extract_user_instruction_from_skill_message(const char *content);

/* Render a slash-skill-expanded turn the way the user typed it:
 * "/work — fix the title leak", "/work" for a bare invocation, or NULL when
 * content is not skill scaffolding. Caller frees. */
char *describe_skill_invocation(const char *content);

/* Lower-level extractors (single-skill appends the instruction after the
 * body — last marker wins; bundle puts it before the loaded skills — first
 * marker wins). Return NULL when absent/empty. Caller frees. */
char *extract_single_skill_user_instruction(const char *message);
char *extract_bundle_user_instruction(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SKILL_SCAFFOLDING_H */
