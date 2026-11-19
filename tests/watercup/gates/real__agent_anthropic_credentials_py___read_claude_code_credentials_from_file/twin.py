"""twin for agent/anthropic_credentials.py:_read_claude_code_credentials_from_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "_read_claude_code_credentials_from_file"
    C_FN = "_read_claude_code_credentials_from_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
