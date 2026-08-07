/*
 * port_hermes_cli_agent_import.h — C11 port of pure helpers from
 * hermes_cli/agent_import.py.
 *
 * Ports the deterministic, I/O-free helpers from the agent-source
 * importer: secret-key detection, text normalization, markdown
 * memory-entry extraction, entry merging, Claude Code permission
 * rule conversion, and MCP env sanitization.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. Entry-list functions return malloc'd char* arrays
 * (caller frees each entry + the array).
 */

#ifndef PORT_HERMES_CLI_AGENT_IMPORT_H
#define PORT_HERMES_CLI_AGENT_IMPORT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: is_secret_key @ hermes_cli/agent_import.py:is_secret_key */
/* True when an env-var name looks like a credential. Case-insensitive
 * regex: (^|_)(API_KEY|APIKEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIALS?
 * |AUTH|PRIVATE_KEY|ACCESS_KEY)(_|$) or KEY$ */
bool ai_is_secret_key(const char *key);

/* PoP: normalize_text @ hermes_cli/agent_import.py:normalize_text */
/* Collapse runs of whitespace to a single space, strip, lowercase. */
char *ai_normalize_text(const char *text);

/* PoP: extract_markdown_entries @ hermes_cli/agent_import.py:extract_markdown_entries */
/* Split a markdown document into deduplicated memory entries.
 * Headings become context prefixes; bullets and paragraphs become
 * entries; code blocks and tables are skipped; entries are deduped
 * by normalized text. Returns a malloc'd NULL-terminated array of
 * malloc'd strings. */
char **ai_extract_markdown_entries(const char *text);

/* PoP: parse_existing_memory_entries @ hermes_cli/agent_import.py:parse_existing_memory_entries */
/* Split a memory store on ENTRY_DELIMITER (\n§\n), returning non-empty
 * stripped entries. Takes file CONTENTS (not a path). Returns a malloc'd
 * NULL-terminated array of malloc'd strings; caller frees each + array. */
char **ai_parse_existing_memory_entries(const char *file_contents);

/* PoP: merge_entries @ hermes_cli/agent_import.py:merge_entries */
/* Merge incoming entries into existing entries under a byte limit.
 * existing_json: JSON array of existing entry strings.
 * incoming_json: JSON array of incoming entry strings.
 * limit: max total bytes of the ENTRY_DELIMITER-joined result.
 * out_added/out_duplicates/out_overflowed: optional stats counters.
 * Returns a malloc'd NULL-terminated array of merged entry strings.
 * Caller frees each element + array. */
char **ai_merge_entries(const char *existing_json, const char *incoming_json,
                        long limit, size_t *out_added,
                        size_t *out_duplicates, size_t *out_overflowed);

/* PoP: claude_rule_to_command_pattern @ hermes_cli/agent_import.py:claude_rule_to_command_pattern */
/* Convert a Claude Code Bash(...) permission rule into a Hermes glob:
 * Bash(npm run build) → "npm run build", Bash(npm run test:*) →
 * "npm run test*", Bash → NULL, non-Bash rules → NULL. */
char *ai_claude_rule_to_command_pattern(const char *rule);

/* PoP: sanitize_mcp_env @ hermes_cli/agent_import.py:sanitize_mcp_env */
/* Split an MCP server env dict into (kept, stripped-secret-names).
 * env_json: JSON object of env vars (or non-object → empty kept).
 * out_kept: malloc'd JSON object string of kept entries.
 * out_stripped: malloc'd NULL-terminated array of secret key names. */
void ai_sanitize_mcp_env(const char *env_json,
                         char **out_kept, char ***out_stripped);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_AGENT_IMPORT_H */
