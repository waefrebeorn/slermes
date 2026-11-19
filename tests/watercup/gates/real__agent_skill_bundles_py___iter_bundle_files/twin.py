"""twin for agent/skill_bundles.py:_iter_bundle_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_bundles.py"
    PY_FN = "_iter_bundle_files"
    C_FN = "_iter_bundle_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
