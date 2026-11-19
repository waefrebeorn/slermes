"""twin for agent/skill_bundles.py:delete_bundle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_bundles.py"
    PY_FN = "delete_bundle"
    C_FN = "delete_bundle"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
