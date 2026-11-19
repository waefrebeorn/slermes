"""twin for gateway/run.py:_bridge_max_turns_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_bridge_max_turns_from_config"
    C_FN = "gw_bridge_max_turns_from_config"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
