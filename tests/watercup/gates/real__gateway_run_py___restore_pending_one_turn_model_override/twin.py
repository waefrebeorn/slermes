"""twin for gateway/run.py:_restore_pending_one_turn_model_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_restore_pending_one_turn_model_override"
    C_FN = "_restore_pending_one_turn_model_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
