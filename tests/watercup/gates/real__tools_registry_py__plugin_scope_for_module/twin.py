"""twin for tools/registry.py:plugin_scope_for_module (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "plugin_scope_for_module"
    C_FN = "plugin_scope_for_module"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
