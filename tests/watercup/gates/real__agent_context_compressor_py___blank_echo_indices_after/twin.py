"""twin for agent/context_compressor.py:_blank_echo_indices_after (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_blank_echo_indices_after"
    C_FN = "_blank_echo_indices_after"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
