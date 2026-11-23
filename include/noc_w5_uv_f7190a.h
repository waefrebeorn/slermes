#ifndef NOC_W5_UV_F7190A_H
#define NOC_W5_UV_F7190A_H
#include <stddef.h>

/* Public C API for pm/_uv.py (gate w5_uv_f7190a).
 * _toolchain is entirely IO/dispatch driven upstream (ensure, lockfile,
 * registry, store, sealed), so every such result is injected at the gate.
 * The resolution decision tree itself is reproduced exactly. */

/* Outcome codes for pmuv_toolchain. */
#define PMUV_OK          1
#define PMUV_NONE        0
#define PMUV_ERROR       2
#define PMUV_UNAVAILABLE 1 /* InstallError "pinned tool is unavailable" */
#define PMUV_MISSING     2 /* InstallError "installed binary is missing" */

/* PoP: pmuv_toolchain @ pm/_uv.py:_toolchain
 * Returns PMUV_OK/PMUV_NONE/PMUV_ERROR. On OK sets *out_uv/*out_py (malloc'd).
 * On ERROR sets *out_kind (PMUV_UNAVAILABLE/PMUV_MISSING) and *out_name
 * (malloc'd "uv" or "python"). Caller frees all returned strings. */
int pmuv_toolchain(int realize, const char *target, int sealed,
                   int uv_location, int uv_binary, const char *uv_path,
                   int py_location, int py_binary, const char *py_path,
                   int py_writable_ok,
                   char **out_uv, char **out_py, int *out_kind, char **out_name);

/* PoP: pmuv_needs_writable_copy @ pm/_uv.py:_toolchain
 * target.startswith("win32") and sealed() — the writable-Python branch. */
int pmuv_needs_writable_copy(const char *target, int sealed);

#endif /* NOC_W5_UV_F7190A_H */
