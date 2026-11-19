"""twin for agent/verification_stop.py:_is_non_code_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_stop.py"
    PY_FN = "_is_non_code_path"
    C_FN = "verification_stop_is_non_code_path"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
