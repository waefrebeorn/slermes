"""twin for gateway/slash_commands.py:_model_switch_skew_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_model_switch_skew_guard"
    C_FN = "_model_switch_skew_guard"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
