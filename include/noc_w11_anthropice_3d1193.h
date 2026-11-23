#ifndef NOC_W11_ANTHROPICE_3D1193_H
#define NOC_W11_ANTHROPICE_3D1193_H
#include "hermes_json.h"

/* PoP port of agent/anthropic_endpoints.py (gate w11_anthropice_3d1193).
 * Every exported symbol is prefixed with the gate name to avoid collisions. */
char *w11_anthropice_3d1193_normalize_base_url_text(const char *base_url);
char *w11_anthropice_3d1193_normalized_lower(const char *base_url);
int   w11_anthropice_3d1193_is_third_party_anthropic_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_kimi_coding_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_opencode_endpoint(const char *base_url);
int   w11_anthropice_3d1193_model_name_is_kimi_family(const char *model);
int   w11_anthropice_3d1193_is_kimi_family_endpoint(const char *base_url, const char *model);
int   w11_anthropice_3d1193_model_name_is_deepseek_thinking(const char *model);
int   w11_anthropice_3d1193_is_deepseek_anthropic_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_minimax_anthropic_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_azure_anthropic_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_bedrock_runtime_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_vertex_ai_endpoint(const char *base_url);
int   w11_anthropice_3d1193_is_claude_platform_endpoint(const char *base_url);
int   w11_anthropice_3d1193_base_url_needs_context_1m_beta(const char *base_url);
int   w11_anthropice_3d1193_requires_bearer_auth(const char *base_url, int nous_override);
#endif /* NOC_W11_ANTHROPICE_3D1193_H */
