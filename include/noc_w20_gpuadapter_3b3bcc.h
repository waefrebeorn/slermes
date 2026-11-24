/*
 * noc_w20_gpuadapter_3b3bcc.h -- C11 port of the DXGI adapter record of upstream
 * hermes_platform/host/gpu_adapters.py.
 */
#ifndef NOC_W20_GPUADAPTER_3B3BCC_H
#define NOC_W20_GPUADAPTER_3B3BCC_H

#include "hermes_json.h"

/* {"software_flag", "max_adapters", "slots":{"release","enum_adapters1","get_desc1"}} */
json_t *w20_gpuadapter_3b3bcc_constants(void);

/*
 * One Adapter as the DXGI desc reports it:
 * {"description","vendor_id","dedicated_bytes","shared_bytes","software","memory_bytes"}
 * 64-bit fields are decimal strings on the way in and out.
 */
json_t *w20_gpuadapter_3b3bcc_adapter(const json_t *desc);

/*
 * The EnumAdapters1 walk: items are DXGI results in index order,
 * {"rc":0,"got_desc":true,"description":...,"vendor_id":...,"flags":...};
 * a nonzero rc ends the list, a false got_desc skips that adapter, at most
 * _MAX_ADAPTERS probes.
 */
json_t *w20_gpuadapter_3b3bcc_enumerate(const json_t *items);

#endif /* NOC_W20_GPUADAPTER_3B3BCC_H */
