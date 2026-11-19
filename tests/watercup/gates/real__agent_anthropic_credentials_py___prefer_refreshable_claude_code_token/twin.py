"""twin for agent/anthropic_credentials.py:_prefer_refreshable_claude_code_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "_prefer_refreshable_claude_code_token"
    C_FN = "_prefer_refreshable_claude_code_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
