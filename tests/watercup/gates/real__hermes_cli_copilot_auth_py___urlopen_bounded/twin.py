"""twin for hermes_cli/copilot_auth.py:_urlopen_bounded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/copilot_auth.py"
    PY_FN = "_urlopen_bounded"
    C_FN = "_urlopen_bounded"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
