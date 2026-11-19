"""twin for hermes_cli/voice.py:_debug (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/voice.py"
    PY_FN = "_debug"
    C_FN = "cli_hermes_cli_voice__debug"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
