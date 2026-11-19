"""twin for agent/command_token_source.py:_mint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/command_token_source.py"
    PY_FN = "_mint"
    C_FN = "_mint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
