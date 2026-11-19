"""twin for agent/shell_hooks.py:_block_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "_block_message"
    C_FN = "_block_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
