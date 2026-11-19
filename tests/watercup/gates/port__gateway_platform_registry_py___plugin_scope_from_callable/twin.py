"""twin for gateway/platform_registry.py:_plugin_scope_from_callable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platform_registry.py"
    PY_FN = "_plugin_scope_from_callable"
    C_FN = "_plugin_scope_from_callable"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
