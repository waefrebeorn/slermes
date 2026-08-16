#ifndef CLI_MCP_SECURITY_H
#define CLI_MCP_SECURITY_H
void hermes_cli_mcp_security_command_basename(const char *command, char *out, size_t outsz);
void hermes_cli_mcp_security_inline_script(const char *args_json, char *out, size_t outsz);
void hermes_cli_mcp_security_entry_text(const char *entry, char *out, size_t outsz);
#endif
