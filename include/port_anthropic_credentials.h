#ifndef PORT_ANTHROPIC_CREDENTIALS_H
#define PORT_ANTHROPIC_CREDENTIALS_H

/* Port of Python: agent/anthropic_credentials.py — the pure classification core.
 *
 * Route/credential shape decisions only: which tokens are OAuth, whether a
 * base URL is the native Anthropic host, whether a Messages route carries the
 * Claude Code identity, and whether a Claude Code token is still valid. The
 * keychain, refresh POST and credential-pool paths are not ported here.
 */

/* True for OAuth/setup tokens (sk-ant-oat*, eyJ JWTs, cc-); false for the
 * Console API-key families (sk-ant-api/usr/svc/admin). Unknown sk-ant-*
 * prefixes still classify as OAuth so future OAuth prefixes keep working. */
int anc_is_oauth_token(const char *key);

/* Exact-hostname comparison: "" (native default) or api.anthropic.com. */
int anc_is_native_host(const char *base_url);

/* The route qualifies when it is the native host or provider == "anthropic",
 * and the credential is an OAuth token. `credential` is a plain string (a
 * callable token source is materialized by the caller). */
int anc_route_is_oauth(const char *base_url, const char *credential,
                       const char *provider);

/* Non-expired access token (60s buffer); no expiresAt means a managed key, so
 * the presence of accessToken decides. `now_ms` is epoch milliseconds. */
int anc_claude_token_valid(const char *creds_json, double now_ms);

/* Claude Code's shared OAuth file: <CLAUDE_CONFIG_DIR>/.credentials.json when
 * that env value is non-blank (with ~ expansion), else <home>/.claude/
 * .credentials.json. Malloc'd. */
char *anc_credentials_path(const char *claude_config_dir, const char *home);

/* Lowercased hostname of a base URL ("" when absent) — urlparse semantics:
 * tolerates a bare host[:port][/path], drops userinfo, port and path, strips
 * trailing dots. Malloc'd. */
char *anc_base_url_hostname(const char *base_url);

#endif /* PORT_ANTHROPIC_CREDENTIALS_H */
