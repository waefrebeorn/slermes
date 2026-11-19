"""twin for agent/portal_tags.py:get_affinity_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/portal_tags.py"
    PY_FN = "get_affinity_scope"
    C_FN = "get_affinity_scope"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
