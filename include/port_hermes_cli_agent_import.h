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
#include "hermes_json.h"

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

/* PoP: load_yaml_file @ hermes_cli/agent_import.py:load_yaml_file */
/* Parse a YAML string into a JSON string. Mirrors Python's yaml.safe_load:
 * returns "{}" for empty/non-dict input, parses YAML into JSON. */
char *ai_load_yaml_from_string(const char *yaml_text);

/* PoP: dump_yaml_file @ hermes_cli/agent_import.py:dump_yaml_file */
/* Serialize a JSON dict to YAML string. Mirrors Python's
 * yaml.safe_dump(data, default_flow_style=False, sort_keys=False,
 * allow_unicode=True). Produces block-style YAML. */
char *ai_dump_yaml_to_string(const char *json_dict);

/* PoP: default_source_dir @ hermes_cli/agent_import.py:default_source_dir */
/* Build the default source dir for an agent:
 * claude-code → "<home>/.claude", codex → "<home>/.codex" */
char *ai_default_source_dir(const char *agent, const char *home);

/* PoP: detect_agents @ hermes_cli/agent_import.py:detect_agents */
/* Return list of agents whose default dirs exist under home.
 * Returns malloc'd NULL-terminated array of malloc'd strings. */
char **ai_detect_agents(const char *home);

/* PoP: backup_memory_file @ hermes_cli/agent_import.py:backup_memory_file */
/* Build a backup path: "<path>.bak.<unix_ts>".
 * Returns NULL if the source file doesn't exist. */
char *ai_backup_path(const char *path, long unix_ts);

/* ── AgentImporter struct + orchestration ──────────────────────────────── */

/* Opaque struct mirroring hermes_cli/agent_import.py class AgentImporter. */
typedef struct {
    char *agent;
    char *source_root;
    char *target_root;
    bool execute;
    bool overwrite;
    json_t *items;         /* array of item dicts */
    json_t *stripped_secrets; /* array of secret key-name strings */
} AgentImporter;

/* PoP: __init__ @ hermes_cli/agent_import.py:AgentImporter.__init__ */
AgentImporter *ai_agent_importer_new(const char *agent,
                                     const char *source_root,
                                     const char *target_root,
                                     bool execute, bool overwrite);

/* PoP: load_target_config @ hermes_cli/agent_import.py:AgentImporter.load_target_config */
json_t *ai_load_target_config(AgentImporter *imp, const char *kind,
                              const char *source, const char *dest_yaml_path);

/* PoP: run @ hermes_cli/agent_import.py:AgentImporter.run */
json_t *ai_agent_importer_run(AgentImporter *imp);

/* PoP: _run_claude_code @ hermes_cli/agent_import.py:AgentImporter._run_claude_code */
void ai_agent_importer_run_claude_code(AgentImporter *imp);

/* PoP: _run_codex @ hermes_cli/agent_import.py:AgentImporter._run_codex */
void ai_agent_importer_run_codex(AgentImporter *imp);

/* PoP: _load_claude_settings @ hermes_cli/agent_import.py:AgentImporter._load_claude_settings */
json_t *ai_agent_importer_load_claude_settings(AgentImporter *imp);

/* PoP: _claude_mcp_servers @ hermes_cli/agent_import.py:AgentImporter._claude_mcp_servers */
json_t *ai_agent_importer_claude_mcp_servers(AgentImporter *imp,
                                             json_t *settings,
                                             const char *claude_json_path);

/* PoP: _load_codex_config @ hermes_cli/agent_import.py:AgentImporter._load_codex_config */
json_t *ai_agent_importer_load_codex_config(AgentImporter *imp);

/* PoP: import_context_file @ hermes_cli/agent_import.py:AgentImporter.import_context_file */
void ai_import_context_file(AgentImporter *imp, const char *source_path, const char *kind);

/* PoP: import_memories_dir @ hermes_cli/agent_import.py:AgentImporter.import_memories_dir */
void ai_import_memories_dir(AgentImporter *imp, const char *memories_dir);

/* PoP: _merge_memory_entries @ hermes_cli/agent_import.py:AgentImporter._merge_memory_entries */
char **ai_merge_memory_entries(AgentImporter *imp, const char *kind,
                               const char *source, const char *dest_path,
                               char **existing, char **incoming, int incoming_count);

/* PoP: import_permission_denylist @ hermes_cli/agent_import.py:AgentImporter.import_permission_denylist */
void ai_import_permission_denylist(AgentImporter *imp, json_t *settings);

/* PoP: import_permission_allowlist @ hermes_cli/agent_import.py:AgentImporter.import_permission_allowlist */
void ai_import_permission_allowlist(AgentImporter *imp, json_t *settings,
                                    const char *target_root);

/* PoP: import_mcp_servers @ hermes_cli/agent_import.py:AgentImporter.import_mcp_servers */
void ai_import_mcp_servers(AgentImporter *imp, json_t *servers, const char *kind);

/* PoP: import_skills @ hermes_cli/agent_import.py:AgentImporter.import_skills */
void ai_import_skills(AgentImporter *imp, const char *source_root,
                      const char *category);

/* PoP: import_agent_command @ hermes_cli/agent_import.py:import_agent_command */
void ai_import_agent_command(const char *agent, const char *source,
                             bool dry_run, bool overwrite, bool auto_yes);

/* PoP: print_import_report @ hermes_cli/agent_import.py:print_import_report */
void ai_print_import_report(json_t *report, bool dry_run);

/* lifecycle */
void ai_agent_importer_free(AgentImporter *imp);
void ai_agent_importer_set_execute(AgentImporter *imp, bool execute);

/* Minimal TOML parser for codex config.toml. Returns JSON object. */
json_t *ai_parse_toml(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_AGENT_IMPORT_H */
