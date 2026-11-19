"""twin for cli.py:_confirm_and_apply_model_switch_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_confirm_and_apply_model_switch_result"
    C_FN = "_confirm_and_apply_model_switch_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
