"""twin for agent/command_token_source.py:build_command_token_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/command_token_source.py"
    PY_FN = "build_command_token_provider"
    C_FN = "build_command_token_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
