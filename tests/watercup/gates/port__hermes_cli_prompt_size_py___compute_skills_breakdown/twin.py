"""twin for hermes_cli/prompt_size.py:_compute_skills_breakdown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/prompt_size.py"
    PY_FN = "_compute_skills_breakdown"
    C_FN = "psize_compute_skills_breakdown"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
