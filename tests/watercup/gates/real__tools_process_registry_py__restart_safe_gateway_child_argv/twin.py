"""twin for tools/process_registry.py:restart_safe_gateway_child_argv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "restart_safe_gateway_child_argv"
    C_FN = "restart_safe_gateway_child_argv"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
