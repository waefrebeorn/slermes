"""twin for tools/spill_safety.py:ensure_spill_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/spill_safety.py"
    PY_FN = "ensure_spill_dir"
    C_FN = "ensure_spill_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
