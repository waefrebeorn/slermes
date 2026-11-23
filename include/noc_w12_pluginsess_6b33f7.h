/* noc_w12_pluginsess_6b33f7.h — public C API port of hermes_cli/plugin_session.py */
#ifndef NOC_W12_PLUGINSESS_6B33F7_H
#define NOC_W12_PLUGINSESS_6B33F7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session_surface: if a policy exists (has_policy), return its platform even
 * when empty; otherwise the injected HERMES_SESSION_PLATFORM env value. */
void w12_pluginsess_6b33f7_session_surface(bool has_policy, const char *platform,
                          const char *env_platform, char *out, size_t cap);

/* context_surface: "cli" when the manager is the classic CLI process, else
 * the already-resolved session surface. */
void w12_pluginsess_6b33f7_context_surface(bool cli_ref, const char *surface, char *out, size_t cap);

/* session_cwd: scoped cwd, else the policy cwd (when a policy exists), else
 * the stripped terminal cwd — Python's `a or b or c` truthiness. */
void w12_pluginsess_6b33f7_session_cwd(const char *scoped, bool has_policy, const char *policy_cwd,
                     const char *terminal_cwd, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_PLUGINSESS_6B33F7_H */
