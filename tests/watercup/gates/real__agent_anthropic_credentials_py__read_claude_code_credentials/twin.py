"""twin for agent/anthropic_credentials.py:read_claude_code_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "read_claude_code_credentials"
    C_FN = "read_claude_code_credentials"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
