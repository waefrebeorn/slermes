#ifndef NOC_W11_COMPUTEHOS_D481EB_H
#define NOC_W11_COMPUTEHOS_D481EB_H

/* C API for the tui_gateway/compute_host.py port (gate w11_computehos_d481eb). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_computehos_d481eb_history_meta @ tui_gateway/compute_host.py:_history_meta */
json_t *w11_computehos_d481eb_history_meta(const json_t *session);
/* PoP: w11_computehos_d481eb_default_workers @ tui_gateway/compute_host.py:_default_workers
 * env_value models os.environ.get("HERMES_TUI_RPC_POOL_WORKERS") for a deterministic seam. */
json_t *w11_computehos_d481eb_default_workers(const json_t *env_value);
/* PoP: w11_computehos_d481eb_host_transport_write @ tui_gateway/compute_host.py:_HostTransport.write
 * Returns the frame passed to emit (the transport's write() return value is always True). */
json_t *w11_computehos_d481eb_host_transport_write(const json_t *obj);
/* PoP: w11_computehos_d481eb_frame_handler @ tui_gateway/compute_host.py:ComputeHost._FRAME_HANDLERS */
json_t *w11_computehos_d481eb_frame_handler(const char *kind);
/* PoP: w11_computehos_d481eb_control_failure @ tui_gateway/compute_host.py:_CONTROL_FAILURES */
json_t *w11_computehos_d481eb_control_failure(const char *route);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_COMPUTEHOS_D481EB_H */
