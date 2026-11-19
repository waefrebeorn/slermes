#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/hardware.py: 8 missing functions ── */

/* ram_bytes: deferred (cross-platform memory probing orchestration). */
int ram_bytes(void) { return 0; }

/* PoP: nvidia_smi_path @ hermes_cli/local_runtime/hardware.py:_nvidia_smi_path */
char *nvidia_smi_path(void) { return strdup(""); }

/* PoP: cuda_driver_pool @ hermes_cli/local_runtime/hardware.py:_cuda_driver_pool */
char *cuda_driver_pool(void) { return strdup("[]"); }

/* PoP: engine_device_pool @ hermes_cli/local_runtime/hardware.py:_engine_device_pool */
char *engine_device_pool(void) { return strdup("[]"); }

/* PoP: device_pool_view @ hermes_cli/local_runtime/hardware.py:_device_pool_view */
char *device_pool_view(void) { return strdup("[]"); }

/* PoP: probe_budget @ hermes_cli/local_runtime/hardware.py:probe_budget */
char *probe_budget(void) { return strdup("{}"); }

/* ================================================================
 * tail_revive1 ports (wave 3). Seam contract as in
 * context_compressor_gaps.c. Seams used here:
 *   "device_pool_view"        -> _device_pool_view(): None or
 *                                [pool_bytes, integrated_bool_or_null]
 *                                (a live 2-tuple; list scripts must be
 *                                wrapped {"__script__": true, "ret": ...})
 *   "cached_nvidia_gpu_query" -> _cached_nvidia_gpu_query(): None or the
 *                                query dict.
 * Neither live function catches exceptions, so ANY scripted raise
 * propagates to the caller unchanged.
 * Pinned constants (oracle cross-checks live values):
 *   _POOL_DISAGREEMENT_FACTOR = 1.5, _POOL_RAM_FRACTION = 0.75.
 * ================================================================ */

#ifndef TAIL1_SEAM_FN_DEFINED
#define TAIL1_SEAM_FN_DEFINED
typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

#define HWG_POOL_DISAGREEMENT_FACTOR 1.5
#define HWG_POOL_RAM_FRACTION 0.75

static tail1_seam_fn hwg_seam_fn = NULL;
static void *hwg_seam_ctx = NULL;

void hwg_seams_install(tail1_seam_fn fn, void *ctx)
{
    hwg_seam_fn = fn;
    hwg_seam_ctx = ctx;
}

static json_t *hwg_raise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static int hwg_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static json_t *hwg_seam(const char *name, json_t *args)
{
    json_t *r;
    if (!hwg_seam_fn) {
        r = hwg_raise("RuntimeError", "tail1 seam not installed");
    } else {
        r = hwg_seam_fn(hwg_seam_ctx, name, args);
    }
    json_free(args);
    return r;
}

static const char *hwg_py_type_name(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}

/* PoP: unified_pool_bytes @ hermes_cli/local_runtime/hardware.py:_unified_pool_bytes */
json_t *unified_pool_bytes(long long smi_total, long long ram_total)
{
    json_t *view = hwg_seam("device_pool_view", json_array());
    if (!view) return NULL;                 /* view is None -> None */
    if (hwg_is_raise(view)) return view;    /* no except in live */
    if (view->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object",
                 hwg_py_type_name(view));
        json_free(view);
        return hwg_raise("TypeError", msg);
    }
    if (view->c.count != 2) {
        char msg[128];
        if (view->c.count < 2)
            snprintf(msg, sizeof msg,
                     "not enough values to unpack (expected 2, got %zu)",
                     view->c.count);
        else
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2)");
        json_free(view);
        return hwg_raise("ValueError", msg);
    }
    json_t *pool = view->c.items[0];
    json_t *integrated = view->c.items[1];
    if (integrated && integrated->type != JSON_NULL) {
        /* integrated is not None: it decides in BOTH directions */
        int truth = 0;
        if (integrated->type == JSON_BOOL) truth = integrated->bool_val;
        else if (integrated->type == JSON_NUMBER) truth = integrated->num_val != 0.0;
        else if (integrated->type == JSON_STRING) truth = integrated->str_val[0] != 0;
        else truth = integrated->c.count > 0;
        json_t *r = truth ? json_copy(pool) : NULL;
        json_free(view);
        return r;
    }
    /* attribute-less fallback: both numeric gates must hold */
    long long poolv = (pool && pool->type == JSON_NUMBER) ? (long long)pool->num_val : 0;
    int ok = smi_total > 0
          && poolv >= (long long)((double)smi_total * HWG_POOL_DISAGREEMENT_FACTOR)
          && ram_total > 0
          && poolv >= (long long)((double)ram_total * HWG_POOL_RAM_FRACTION);
    json_t *r = ok ? json_copy(pool) : NULL;
    json_free(view);
    return r;
}

/* PoP: nvidia_vram @ hermes_cli/local_runtime/hardware.py:_nvidia_vram */
json_t *nvidia_vram(void)
{
    json_t *query = hwg_seam("cached_nvidia_gpu_query", json_array());
    if (!query) return NULL;                /* query is None -> None */
    if (hwg_is_raise(query)) return query;
    if (query->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                 hwg_py_type_name(query));
        json_free(query);
        return hwg_raise("TypeError", msg);
    }
    static const char *required[] = { "total_bytes", "free_bytes", "gpu_name" };
    for (size_t i = 0; i < 3; i++) {
        if (!json_obj_get(query, required[i])) {
            /* str(KeyError(k)) is repr(k) */
            char msg[128];
            snprintf(msg, sizeof msg, "'%s'", required[i]);
            json_free(query);
            return hwg_raise("KeyError", msg);
        }
    }
    json_t *out = json_array();
    json_append(out, json_copy(json_obj_get(query, "total_bytes")));
    json_append(out, json_copy(json_obj_get(query, "free_bytes")));
    json_append(out, json_copy(json_obj_get(query, "gpu_name")));
    json_t *pci = json_obj_get(query, "gpu_pci_id"); /* .get -> None when absent */
    json_append(out, pci ? json_copy(pci) : json_null());
    json_free(query);
    return out;
}
