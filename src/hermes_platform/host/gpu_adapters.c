/*
 * gpu_adapters.c -- real C11 port of the DXGI adapter record and enumeration
 * walk of upstream Hermes hermes_platform/host/gpu_adapters.py.
 *
 * PoP: w20_gpuadapter_3b3bcc_adapter @ hermes_platform/host/gpu_adapters.py:Adapter
 * PoP: w20_gpuadapter_3b3bcc_enumerate @ hermes_platform/host/gpu_adapters.py:windows_gpu_adapters
 * PoP: w20_gpuadapter_3b3bcc_constants @ hermes_platform/host/gpu_adapters.py:_SOFTWARE_FLAG
 *
 * What the upstream module computes is small and fully deterministic: Task
 * Manager's "GPU memory" is dedicated video memory plus shared system memory,
 * an adapter is "software" when DXGI_ADAPTER_FLAG_SOFTWARE is set, and the
 * enumeration probes at most 16 slots, skipping a slot whose GetDesc1 failed.
 * The ctypes/DXGI vtable reads themselves (the live COM seam) stay upstream;
 * this port takes their results as data.
 */
#include "noc_w20_gpuadapter_3b3bcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GA3B3BCC_SOFTWARE_FLAG 0x2
#define GA3B3BCC_MAX_ADAPTERS 16

/* int64 from a decimal string or a JSON number (never a double round-trip). */
static long long ga3b3bcc_i64(const json_t *v) {
    char buf[32];
    if (!v) return 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "0", NULL, 10);
    if (v->type == JSON_NUMBER) {
        snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        return strtoll(buf, NULL, 10);
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

static void ga3b3bcc_set_i64(json_t *obj, const char *key, long long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    json_set(obj, key, json_string(buf));
}

json_t *w20_gpuadapter_3b3bcc_constants(void) {
    json_t *out = json_object();
    json_t *slots = json_object();
    json_set(out, "software_flag", json_number(GA3B3BCC_SOFTWARE_FLAG));
    json_set(out, "max_adapters", json_number(GA3B3BCC_MAX_ADAPTERS));
    json_set(slots, "release", json_number(2));
    json_set(slots, "enum_adapters1", json_number(12));
    json_set(slots, "get_desc1", json_number(10));
    json_set(out, "slots", slots);
    return out;
}

json_t *w20_gpuadapter_3b3bcc_adapter(const json_t *desc) {
    json_t *out = json_object();
    const char *description;
    long long vendor_id, dedicated, shared, flags;
    if (!desc || desc->type != JSON_OBJECT) desc = NULL;

    description = desc ? json_get_str(desc, "description", "") : "";
    vendor_id = ga3b3bcc_i64(desc ? json_obj_get(desc, "vendor_id") : NULL);
    dedicated = ga3b3bcc_i64(desc ? json_obj_get(desc, "dedicated_bytes") : NULL);
    shared = ga3b3bcc_i64(desc ? json_obj_get(desc, "shared_bytes") : NULL);
    flags = ga3b3bcc_i64(desc ? json_obj_get(desc, "flags") : NULL);

    json_set(out, "description", json_string(description));
    ga3b3bcc_set_i64(out, "vendor_id", vendor_id);
    ga3b3bcc_set_i64(out, "dedicated_bytes", dedicated);
    ga3b3bcc_set_i64(out, "shared_bytes", shared);
    /* bool(desc.Flags & _SOFTWARE_FLAG) */
    json_set(out, "software", json_bool((flags & GA3B3BCC_SOFTWARE_FLAG) ? 1 : 0));
    /* Adapter.memory_bytes: what Windows lets this adapter allocate. */
    ga3b3bcc_set_i64(out, "memory_bytes", dedicated + shared);
    return out;
}

json_t *w20_gpuadapter_3b3bcc_enumerate(const json_t *items) {
    json_t *out = json_array();
    size_t i;
    if (!items || items->type != JSON_ARRAY) return out;
    for (i = 0; i < items->c.count && i < GA3B3BCC_MAX_ADAPTERS; i++) {
        json_t *item = items->c.items[i];
        json_t *rcv, *gotv;
        if (!item || item->type != JSON_OBJECT) continue;
        rcv = json_obj_get(item, "rc");
        /* enum(factory, index, ...) != 0 ends the list (DXGI_ERROR_NOT_FOUND). */
        if (rcv && ga3b3bcc_i64(rcv) != 0) break;
        gotv = json_obj_get(item, "got_desc");
        if (gotv && gotv->type == JSON_BOOL && !gotv->bool_val) continue; /* GetDesc1 failed */
        json_append(out, w20_gpuadapter_3b3bcc_adapter(item));
    }
    return out;
}
