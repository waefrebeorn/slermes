"""twin for tools/process_registry.py:_is_supervised_gateway_process (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_is_supervised_gateway_process"
    C_FN = "process_registry_is_supervised_gateway_process"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
