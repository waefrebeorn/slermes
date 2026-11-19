"""twin for tools/plugin_guard.py:scan_plugin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/plugin_guard.py"
    PY_FN = "scan_plugin"
    C_FN = "scan_plugin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
