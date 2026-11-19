"""twin for agent/vertex_adapter.py:_resolve_project_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/vertex_adapter.py"
    PY_FN = "_resolve_project_override"
    C_FN = "_resolve_project_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
