"""twin for agent/skill_bundles.py:list_bundles (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_bundles.py"
    PY_FN = "list_bundles"
    C_FN = "list_bundles"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
