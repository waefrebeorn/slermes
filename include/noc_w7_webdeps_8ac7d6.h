#ifndef NOC_W7_WEBDEPS_8AC7D6_H
#define NOC_W7_WEBDEPS_8AC7D6_H
/*
 * Public C API for the port of hermes_cli/web_deps.py (gate w7_webdeps_8ac7d6).
 *
 * The live module late-binds attributes of a module that exists only in the
 * web_server process. The harness injects the resolved target value (the
 * fixture's "target" key); these functions implement the forwarding semantics
 * of late()'s proxy and LateState's dunder methods on that value.
 */
#include "hermes_json.h"

/* late(name, module).__name__ / __qualname__ — the proxy keeps the target name. */
json_t *w7_webdeps_late_name(const char *name, const char *module);

/* LateState(name, module) operation on the injected target.
 * op: getitem | contains | len | bool | repr | attr | eq | ne | lt | le | gt | ge
 * arg is the key/index/item/other operand as appropriate. */
json_t *w7_webdeps_state_op(const char *op, const char *name,
                            const json_t *target, const json_t *arg);

#endif /* NOC_W7_WEBDEPS_8AC7D6_H */
