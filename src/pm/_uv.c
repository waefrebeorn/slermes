/*
 * _uv.c — real C11 port of pm/_uv.py (gate w5_uv_f7190a).
 *
 * PoP: pmuv_toolchain @ pm/_uv.py:_toolchain
 * PoP: pmuv_needs_writable_copy @ pm/_uv.py:_toolchain
 *
 * Upstream _toolchain resolves everything through ensure/lockfile/registry/
 * store/sealed/writable_copy. Those results are injected at the gate; the
 * resolution decision tree (None vs InstallError vs success) is ported exactly.
 */
#include "noc_w5_uv_f7190a.h"
#include <stdlib.h>
#include <string.h>

int pmuv_needs_writable_copy(const char *target, int sealed) {
    return sealed && target && !strncmp(target, "win32", 5);
}

int pmuv_toolchain(int realize, const char *target, int sealed,
                   int uv_location, int uv_binary, const char *uv_path,
                   int py_location, int py_binary, const char *py_path,
                   int py_writable_ok,
                   char **out_uv, char **out_py, int *out_kind, char **out_name) {
    if (out_uv) *out_uv = NULL;
    if (out_py) *out_py = NULL;
    if (out_kind) *out_kind = 0;
    if (out_name) *out_name = NULL;

    /* for name in ("uv", "python"): */
    if (!uv_location) {
        if (!realize) return PMUV_NONE;
        if (out_kind) *out_kind = PMUV_UNAVAILABLE;
        if (out_name) *out_name = strdup("uv");
        return PMUV_ERROR;
    }
    if (!uv_binary) {
        if (!realize) return PMUV_NONE;
        if (out_kind) *out_kind = PMUV_MISSING;
        if (out_name) *out_name = strdup("uv");
        return PMUV_ERROR;
    }

    int py_loc = py_location;
    if (pmuv_needs_writable_copy(target, sealed)) {
        /* location = writable_copy(...); if location is None: return None */
        py_loc = py_writable_ok;
        if (!py_loc) return PMUV_NONE;
    }
    if (!py_loc) {
        if (!realize) return PMUV_NONE;
        if (out_kind) *out_kind = PMUV_UNAVAILABLE;
        if (out_name) *out_name = strdup("python");
        return PMUV_ERROR;
    }
    if (!py_binary) {
        if (!realize) return PMUV_NONE;
        if (out_kind) *out_kind = PMUV_MISSING;
        if (out_name) *out_name = strdup("python");
        return PMUV_ERROR;
    }

    if (out_uv) *out_uv = strdup(uv_path ? uv_path : "");
    if (out_py) *out_py = strdup(py_path ? py_path : "");
    return PMUV_OK;
}
