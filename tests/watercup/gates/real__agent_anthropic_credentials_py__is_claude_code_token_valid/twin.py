"""twin for agent/anthropic_credentials.py:is_claude_code_token_valid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "is_claude_code_token_valid"
    C_FN = "is_claude_code_token_valid"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
