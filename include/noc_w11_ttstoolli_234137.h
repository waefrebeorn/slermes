#ifndef NOC_W11_TTSTOOLI_234137_H
#define NOC_W11_TTSTOOLI_234137_H

/* C API for the tools/tts_tool_lifecycle.py port (gate w11_ttstoolli_234137). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_ttstoolli_234137_keep_warm_seconds @ tools/tts_tool_lifecycle.py:_keep_warm_seconds
 * raw is the configured keep_warm_seconds value (the config seam is injected by the fixture). */
json_t *w11_ttstoolli_234137_keep_warm_seconds(const json_t *raw);
/* PoP: w11_ttstoolli_234137_release_provider @ tools/tts_tool_lifecycle.py:release_tts_provider
 * cache_sizes maps cache name -> resident model count. Returns {"released": n}. */
json_t *w11_ttstoolli_234137_release_provider(const json_t *provider, const json_t *cache_sizes);
/* PoP: w11_ttstoolli_234137_warm_noop @ tools/tts_tool_lifecycle.py:warm_tts_provider
 * Local engines (piper/kittentts) and user-command hooks are out of scope for this pure slice;
 * the fixture uses providers that resolve to the documented noop branch. */
json_t *w11_ttstoolli_234137_warm_noop(const json_t *provider);
/* PoP: w11_ttstoolli_234137_lease_op @ tools/tts_tool_lifecycle.py:acquire_tts_lease
 * Also covers release_tts_lease and tts_lease_holders via `op`. Returns
 * {"result": <live result>, "holders": [sorted names]}. */
json_t *w11_ttstoolli_234137_lease_op(const char *op, const char *lease,
                                      const json_t *leases, const json_t *cache_sizes,
                                      double keep_warm);
/* PoP: w11_ttstoolli_234137_warmer_names @ tools/tts_tool_lifecycle.py:_local_tts_warmers */
json_t *w11_ttstoolli_234137_warmer_names(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TTSTOOLI_234137_H */
