"""twin for agent/think_scrubber.py:_max_partial_suffix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/think_scrubber.py"
    PY_FN = "_max_partial_suffix"
    C_FN = "ths_max_partial_suffix"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
