"""twin for tools/plugin_guard.py:_check_plugin_structure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/plugin_guard.py"
    PY_FN = "_check_plugin_structure"
    C_FN = "_check_plugin_structure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
