"""twin for tools/skill_ledger.py:_latest_skills_tarball (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "_latest_skills_tarball"
    C_FN = "_latest_skills_tarball"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
