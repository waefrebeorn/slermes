"""twin for tools/skills_guard.py:_resolve_trust_level (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "_resolve_trust_level"
    C_FN = "cli_tools_skills_guard__resolve_trust_level"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
