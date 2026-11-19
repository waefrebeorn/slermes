"""twin for tools/skill_provenance.py:get_current_write_origin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_provenance.py"
    PY_FN = "get_current_write_origin"
    C_FN = "tools_skill_provenance_get_current_write_origin"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
