"""twin for agent/context_references.py:get_context_reference_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "get_context_reference_providers"
    C_FN = "get_context_reference_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
