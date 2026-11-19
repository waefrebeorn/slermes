"""twin for gateway/config.py:_scan_bundled_plugin_platforms (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "_scan_bundled_plugin_platforms"
    C_FN = "_scan_bundled_plugin_platforms"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
