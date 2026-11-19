"""twin for agent/skill_bundles.py:scan_bundles (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_bundles.py"
    PY_FN = "scan_bundles"
    C_FN = "scan_bundles"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
