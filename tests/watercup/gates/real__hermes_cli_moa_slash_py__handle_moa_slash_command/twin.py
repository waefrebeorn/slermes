"""twin for hermes_cli/moa_slash.py:handle_moa_slash_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_slash.py"
    PY_FN = "handle_moa_slash_command"
    C_FN = "handle_moa_slash_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
