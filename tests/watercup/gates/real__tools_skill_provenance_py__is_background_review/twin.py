"""twin for tools/skill_provenance.py:is_background_review (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_provenance.py"
    PY_FN = "is_background_review"
    C_FN = "skill_provenance_current_origin"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
