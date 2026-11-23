#ifndef NOC_W11_HOSTEDTOOL_48C71A_H
#define NOC_W11_HOSTEDTOOL_48C71A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_hostedtool_48c71a_message @ tools/hosted_tool_policy.py:_message */
struct json_t *w11_hostedtool_48c71a_message(const char *what, const char *why);

/* PoP: w11_hostedtool_48c71a_tool_refusal @ tools/hosted_tool_policy.py:hosted_tool_refusal
 * `hosted` is an explicit seam for _hosted() (Windows CRT cannot hold Python's env). */
struct json_t *w11_hostedtool_48c71a_tool_refusal(const char *tool_name, int hosted);

/* PoP: w11_hostedtool_48c71a_feature_enabled @ tools/hosted_tool_policy.py:host_feature_enabled
 * Returns a JSON bool, or {"__raise__":"KeyError","message":...} for an unknown feature. */
struct json_t *w11_hostedtool_48c71a_feature_enabled(const char *feature, int hosted);

/* PoP: w11_hostedtool_48c71a_feature_refusal @ tools/hosted_tool_policy.py:host_feature_refusal */
struct json_t *w11_hostedtool_48c71a_feature_refusal(const char *feature, const char *what, int hosted);

/* PoP: w11_hostedtool_48c71a_require @ tools/hosted_tool_policy.py:require_host_feature
 * Returns JSON null, or an exception object on KeyError / HostedToolRefused. */
struct json_t *w11_hostedtool_48c71a_require(const char *feature, const char *what, int hosted);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_HOSTEDTOOL_48C71A_H */
