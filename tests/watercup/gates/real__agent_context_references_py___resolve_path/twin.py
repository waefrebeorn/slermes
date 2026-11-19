"""twin for agent/context_references.py:_resolve_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "_resolve_path"
    C_FN = "_resolve_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
