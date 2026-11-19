"""twin for agent/turn_finalizer.py:_assistant_row_missing_visible_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_finalizer.py"
    PY_FN = "_assistant_row_missing_visible_text"
    C_FN = "_assistant_row_missing_visible_text"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
