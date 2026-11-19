"""twin for tools/xai_http.py:build_xai_storage_options (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "build_xai_storage_options"
    C_FN = "build_xai_storage_options"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
