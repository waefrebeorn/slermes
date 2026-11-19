"""twin for agent/coding_context.py:_project_facts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_project_facts"
    C_FN = "_project_facts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
