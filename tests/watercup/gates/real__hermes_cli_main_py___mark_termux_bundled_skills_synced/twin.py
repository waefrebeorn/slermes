"""twin for hermes_cli/main.py:_mark_termux_bundled_skills_synced (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_mark_termux_bundled_skills_synced"
    C_FN = "main_u_mark_termux_bundled_skills_synced"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
