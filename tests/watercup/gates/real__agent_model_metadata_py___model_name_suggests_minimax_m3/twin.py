"""twin for agent/model_metadata.py:_model_name_suggests_minimax_m3 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_model_name_suggests_minimax_m3"
    C_FN = "mmd_model_name_suggests_minimax_m3"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
