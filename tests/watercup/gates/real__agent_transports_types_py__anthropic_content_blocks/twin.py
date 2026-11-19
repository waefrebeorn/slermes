"""twin for agent/transports/types.py:anthropic_content_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/types.py"
    PY_FN = "anthropic_content_blocks"
    C_FN = "anthropic_content_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
