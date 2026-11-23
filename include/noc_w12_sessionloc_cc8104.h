#ifndef NOC_W12_SESSIONLOC_CC8104_H
#define NOC_W12_SESSIONLOC_CC8104_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionloc_cc8104_session_scope @ gateway/session_local_mcp.py:session_scope
 * Returns malloc'd "editor-session:" + sha256("<profile_id>:<sid>").hexdigest(); caller frees. */
char *w12_sessionloc_cc8104_session_scope(const char *profile_id, const char *sid);

/* PoP: w12_sessionloc_cc8104_frozen_toolset @ gateway/session_local_mcp.py:_frozen_toolset
 * Faithful port of _frozen_toolset(data, toolset):
 *   if not data: False
 *   if 'names' in data: toolset in {f"mcp-{name}" for name in data["names"].values()}
 *   else: toolset.startswith(f"mcp-editor_{sha256(data['scope'])[:20]}_") */
int w12_sessionloc_cc8104_frozen_toolset(const struct json_t *data, const char *toolset);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONLOC_CC8104_H */
