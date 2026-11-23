#ifndef NOC_W8_MCPTOOLSCH_81805C_H
#define NOC_W8_MCPTOOLSCH_81805C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_mcptoolsch_81805c_sanitize_mcp_name_component @ tools/mcp_tool_schema.py:sanitize_mcp_name_component
 * Replace every byte outside [A-Za-z0-9_] with '_'. Caller frees. */
char *w8_mcptoolsch_81805c_sanitize_mcp_name_component(const char *value);

/* PoP: w8_mcptoolsch_81805c_mcp_prefixed_tool_name @ tools/mcp_tool_schema.py:mcp_prefixed_tool_name
 * mcp__<server>__<tool>, clamped to 64 chars with a sha256 suffix. Caller frees. */
char *w8_mcptoolsch_81805c_mcp_prefixed_tool_name(const char *server_name, const char *tool_name);

/* PoP: w8_mcptoolsch_81805c_matches_name_filter @ tools/mcp_tool_schema.py:matches_name_filter
 * Exact membership, then case-sensitive fnmatch for glob patterns. `patterns`
 * is a JSON array of strings (already normalized by _normalize_name_filter). */
int w8_mcptoolsch_81805c_matches_name_filter(const char *tool_name,
        const struct json_t *patterns);

/* PoP: w8_mcptoolsch_81805c_rewrite_local_refs @ tools/mcp_tool_schema.py:_rewrite_local_refs
 * Promote definitions -> $defs outside properties/patternProperties and rewrite
 * #/definitions/ refs. Returns a deep copy (caller frees). */
struct json_t *w8_mcptoolsch_81805c_rewrite_local_refs(const struct json_t *node);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_MCPTOOLSCH_81805C_H */
