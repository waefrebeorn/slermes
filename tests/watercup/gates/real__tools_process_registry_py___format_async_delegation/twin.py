"""twin for tools/process_registry.py:_format_async_delegation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_format_async_delegation"
    C_FN = "_format_async_delegation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
